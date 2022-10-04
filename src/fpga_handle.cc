/*
 * Copyright (c) 2019,
 * The University of California, Berkeley and Duke University.
 * All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fpga_handle.h"
#include "composer_verilator_server.h"
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>


fpga_handle_sim_t::fpga_handle_sim_t() {
  FILE *file = fopen(cmd_server_file_name.c_str(), "w");
  if (file == nullptr) {
    std::cerr << "Error opening file " << cmd_server_file_name << std::endl;
    exit(1);
  }
  csfd = fileno(file);
  cmd_server = (cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), PROT_READ | PROT_WRITE, MAP_SHARED, csfd, 0);
  if (cmd_server == MAP_FAILED) {
    std::cerr << "Failed to map in cmd_server_file" << std::endl;
    exit(1);
  }

  FILE *f2 = fopen(data_server_file_name.c_str(), "w");
  if (f2 == nullptr) {
    std::cerr << "Error opening file " << data_server_file_name << std::endl;
    exit(1);
  }
  dsfd = fileno(f2);
  data_server = (comm_file *) mmap(nullptr, sizeof(comm_file), PROT_READ | PROT_WRITE, MAP_SHARED, dsfd, 0);
  if (data_server == MAP_FAILED) {
    std::cerr << "Failed to map in data_server_file" << std::endl;
    exit(1);
  }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"

#pragma clang diagnostic pop

fpga_handle_sim_t::~fpga_handle_sim_t() {
  munmap(cmd_server, sizeof(cmd_server_file));
  close(csfd);
  for(auto & it : device2virtual) {
    auto tup = it.second;
    free(std::get<0>(tup));
  }
}

bool fpga_handle_sim_t::is_real() const { return false; }


rocc_response fpga_handle_sim_t::get_response(int handle) {
  pthread_mutex_lock(&cmd_server->wait_for_response[handle]);
  // command is now ready
  auto resp = cmd_server->responses[handle];
  // now that we've read our response, we can release the resource to be used in future responses
  pthread_mutex_lock(&cmd_server->free_list_lock);
  cmd_server->free_list[++cmd_server->free_list_idx] = handle;
  pthread_mutex_unlock(&cmd_server->free_list_lock);
  return resp;
}

int fpga_handle_sim_t::send(const rocc_cmd &c) const {
  // acquire lock over client side
  pthread_mutex_lock(&cmd_server->cmd_send_lock);
  // communicate data to shared space
  cmd_server->cmd = c;
  // signal to server that we have a command ready
  pthread_mutex_unlock(&cmd_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&cmd_server->cmd_recieve_server_resp_lock);
  // get the handle that we use to wait for response asynchronously
  uint64_t handle = cmd_server->pthread_wait_id;
  // release lock over client side
  pthread_mutex_unlock(&cmd_server->cmd_send_lock);
  return (int)handle;
}

uint64_t fpga_handle_sim_t::malloc(size_t len) {
  // acquire lock over client side
  pthread_mutex_lock(&data_server->client_mutex);
  data_server->fsize = len;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->wait_for_request_process);
  // now the server has returned the device addr (for building commands), and the file name
  uint64_t addr = data_server->addr;
  FILE *fn = fopen(data_server->fname, "w+");
  if (fn == nullptr) {
    std::cerr << "Error opening file " << data_server->fname << std::endl;
    exit(1);
  }
  int fd = fileno(fn);
  void *ptr = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    std::cerr << "Failed to map in file " << data_server->fname << std::endl;
    exit(1);
  }
  // add device addr to private map
  device2virtual[addr] = std::make_tuple(fd, ptr, len);
  // release lock over client side
  pthread_mutex_unlock(&data_server->client_mutex);
  return addr;
}

void fpga_handle_sim_t::copy_to_fpga(uint64_t fpga_addr, const void *host_addr, size_t len) {
  auto it = device2virtual.find(fpga_addr);
  if (it == device2virtual.end()) {
    std::cerr << "Error: copy_to_fpga called with invalid fpga_addr" << std::endl;
    std::cerr << "\t Make sure that you're using the device addr returned from fpga_handle_t::malloc"
                 " and not a host address. Also, make sure it is the base address returned." << std::endl;
    exit(1);
  }
  auto tup = it->second;
  memcpy(std::get<1>(tup), host_addr, len);
}

void fpga_handle_sim_t::free(uint64_t fpga_addr) {
  auto it = device2virtual.find(fpga_addr);
  if (it == device2virtual.end()) {
    std::cerr << "Error: free called with invalid fpga_addr" << std::endl;
    std::cerr << "\t Make sure that you're using the device addr returned from fpga_handle_t::malloc"
                 " and not a host address. Also, make sure it is the base address returned." << std::endl;
    exit(1);
  }
  auto tup = it->second;
  munmap(std::get<1>(tup), std::get<2>(tup));
  close(std::get<0>(tup));
  device2virtual.erase(it);
}


rocc_response fpga_handle_t::flush() {
  auto q = rocc_cmd::flush_cmd();
  auto i = send(q);
  return get_response(i);
}

#ifdef USE_AWS

void fpga_handle_t::check_rc(int rc, const std::string &infostr) const {
  if (rc) {
    std::cerr << "Invalid return code (" << rc << ") - " << infostr;
#ifdef NDEBUG
    std::cerr << "\n";
#else
    std::cerr << std::endl;
#endif
    fpga_shutdown();
    exit(1);
  }
}


void fpga_handle_real_t::send(const rocc_cmd &cmd) const {
#ifndef NDEBUG
  std::cout << "Sending the following command: " << cmd << std::endl;
#endif
  uint32_t *buf = cmd.pack();
  for (uint i = 0; i < 5; ++i) {
    while (!read(CMD_READY)) {}
    write(CMD_BITS, buf[i]);
    write(CMD_VALID, 0x1);
  }
  delete buf;
}

uint32_t fpga_handle_real_t::read(size_t addr) const {
  addr <<= 2;
  uint32_t value;
  int rc = fpga_pci_peek(pci_bar_handle, addr, &value);
  check_rc(rc, "From read");
  return value & 0xFFFFFFFF;
}

bool fpga_handle_real_t::is_real() const { return true; }

uint32_t fpga_handle_real_t::is_write_ready() const {
  uint32_t value;
  // TODO - check this is right. addr used to be 0x4, which was CMD_VALID - I think CMD_READY is correct
  int rc = fpga_pci_peek(pci_bar_handle, CMD_READY, &value);
  check_rc(rc, "from is_write_ready");
  return value & 0xFFFFFFFF;
}

void fpga_handle_real_t::write(size_t addr, uint32_t data) const {
  // addr is really a (32-byte) word address because of zynq implementation
  addr <<= 2;
  int rc = fpga_pci_poke(pci_bar_handle, addr, data);
  check_rc(rc, "");
}

int fpga_handle_real_t::get_read_fd() const {
  return xdma_read_fd;
}

int fpga_handle_real_t::get_write_fd() const {
  return xdma_write_fd;
}

fpga_handle_real_t::~fpga_handle_real_t() {
  if (close(xdma_write_fd) < 0) {
    perror("failed to close xdma h2c");
    exit(1);
  }
  if (close(xdma_read_fd) < 0) {
    perror("failed to close xdma c2h");
    exit(1);
  }
  fpga_shutdown();
}

fpga_handle_real_t::fpga_handle_real_t(int id) {
  /*
  * pci_vendor_id and pci_device_id values below are Amazon's and avaliable
  * to use for a given FPGA slot.
  * Users may replace these with their own if allocated to them by PCI SIG
  */
  uint16_t pci_vendor_id = 0x1D0F; /* Amazon PCI Vendor ID */
  uint16_t pci_device_id = 0xF000; /* PCI Device ID preassigned by Amazon for F1 applications */

  slot_id = id;
  int rc = fpga_pci_init();
  check_rc(rc, "fpga_pci_init FAILED");

  /* check AFI status */
  struct fpga_mgmt_image_info info = {0};

  /* get local image description, contains status, vendor id, and device id. */
  rc = fpga_mgmt_describe_local_image(slot_id, &info, 0);
  check_rc(rc, "Unable to get AFI information from slot. Are you running as root?");

  /* check to see if the slot is ready */
  if (info.status != FPGA_STATUS_LOADED) {
    rc = 1;
    check_rc(rc, "AFI in Slot is not in READY state !");
  }

  fprintf(stderr, "AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
          info.spec.map[FPGA_APP_PF].vendor_id,
          info.spec.map[FPGA_APP_PF].device_id);

  /* confirm that the AFI that we expect is in fact loaded */
  if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
      info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
    fprintf(stderr, "AFI does not show expected PCI vendor id and device ID. If the AFI "
                    "was just loaded, it might need a rescan. Rescanning now.\n");

    rc = fpga_pci_rescan_slot_app_pfs(slot_id);
    check_rc(rc, "Unable to update PF for slot");
    /* get local image description, contains status, vendor id, and device id. */
    rc = fpga_mgmt_describe_local_image(slot_id, &info, 0);
    check_rc(rc, "Unable to get AFI information from slot");

    fprintf(stderr, "AFI PCI  Vendor ID: 0x%x, Device ID 0x%x\n",
            info.spec.map[FPGA_APP_PF].vendor_id,
            info.spec.map[FPGA_APP_PF].device_id);

    /* confirm that the AFI that we expect is in fact loaded after rescan */
    if (info.spec.map[FPGA_APP_PF].vendor_id != pci_vendor_id ||
        info.spec.map[FPGA_APP_PF].device_id != pci_device_id) {
      rc = 1;
      check_rc(rc, "The PCI vendor id and device of the loaded AFI are not "
                   "the expected values.");
    }
  }

  /* attach to BAR0 */
  pci_bar_handle = PCI_BAR_HANDLE_INIT;
  rc = fpga_pci_attach(slot_id, FPGA_APP_PF, APP_PF_BAR0, 0, &pci_bar_handle);
  check_rc(rc, "fpga_pci_attach FAILED");

  char write_file_name[256];
  char read_file_name[256];
  sprintf(write_file_name, "/dev/xdma%d_h2c_0", id);
  sprintf(read_file_name, "/dev/xdma%d_c2h_0", id);
  if ((xdma_write_fd = open(write_file_name, O_WRONLY)) == -1) {
    perror("xdma h2c fd failed to open");
    exit(1);
  } else {
    printf("opened xdma h2c fd = %d\n", xdma_write_fd);
  }
  if ((xdma_read_fd = open(read_file_name, O_RDONLY)) == -1) {
    perror("xdma c2h fd failed to open");
    exit(1);
  } else {
    printf("opened xdma  c2h fd = %d\n", xdma_read_fd);
  }
}

void fpga_handle_real_t::fpga_shutdown() const {
  int rc = fpga_pci_detach(pci_bar_handle);
  // don't call check_rc because of fpga_shutdown call. do it manually:
  if (rc) {
    fprintf(stderr, "Failure while detaching from the fpga: %d\n", rc);
  }
}

#endif