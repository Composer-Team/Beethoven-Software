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

#include <fcntl.h>
#include <sys/stat.h>
#include <iostream>

fpga_handle_sim_t::fpga_handle_sim_t() {
  char *user = getenv("USER");
  sprintf(driver_to_xsim, "/tmp/%s_driver_to_xsim", user);
  sprintf(xsim_to_driver, "/tmp/%s_xsim_to_driver", user);
  mkfifo(driver_to_xsim, 0666);
  fprintf(stderr, "opening driver to xsim\n");
  driver_to_xsim_fd = open(driver_to_xsim, O_WRONLY);
  fprintf(stderr, "opening xsim to driver\n");
  xsim_to_driver_fd = open(xsim_to_driver, O_RDONLY);
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

void fpga_handle_t::check_rc(int rc, const std::string& infostr) const {
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
void fpga_handle_real_t::fpga_shutdown() const {
  int rc = fpga_pci_detach(pci_bar_handle);
  // don't call check_rc because of fpga_shutdown call. do it manually:
  if (rc) {
    fprintf(stderr, "Failure while detaching from the fpga: %d\n", rc);
  }
}

void fpga_handle_sim_t::fpga_shutdown() const {}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"

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

#pragma clang diagnostic pop

fpga_handle_sim_t::~fpga_handle_sim_t() {
  int exit = 0x12459333;
  ::write(driver_to_xsim_fd, (char *) &exit, 4);
  close(xsim_to_driver_fd);
  close(driver_to_xsim_fd);
}

void fpga_handle_t::store_resp(RD rd, uint32_t unit_id, uint32_t retval) {
  rocc_resp_table[rd][unit_id] = retval;
}

uint32_t fpga_handle_t::resp_lookup(RD rd, uint32_t unit_id) const {
  return rocc_resp_table.at((uint32_t)rd).at(unit_id);
}

int fpga_handle_real_t::get_write_fd() const {
  return xdma_write_fd;
}

int fpga_handle_sim_t::get_write_fd() const {
  return int(NULL);
}

int fpga_handle_real_t::get_read_fd() const {
  return xdma_read_fd;
}

int fpga_handle_sim_t::get_read_fd() const {
  return int(NULL);
}

void fpga_handle_real_t::write(size_t addr, uint32_t data) const {
  // addr is really a (32-byte) word address because of zynq implementation
  addr <<= 2;
  int rc = fpga_pci_poke(pci_bar_handle, addr, data);
  check_rc(rc, "");
}

void fpga_handle_sim_t::write(size_t addr, uint32_t data) const {
  addr <<= 2;
  // printf("addr, data: %#x, %#x\n", addr, data);
  uint64_t cmd = (((uint64_t) (0x80000000 | addr)) << 32) | (uint64_t) data;
  char *buf = (char *) &cmd;
  ::write(driver_to_xsim_fd, buf, 8);
}


uint32_t fpga_handle_real_t::read(size_t addr) const {
  addr <<= 2;
  uint32_t value;
  int rc = fpga_pci_peek(pci_bar_handle, addr, &value);
  check_rc(rc, "From read");
  return value & 0xFFFFFFFF;
}

uint32_t fpga_handle_sim_t::read(size_t addr) const {
  addr <<= 2;
  uint64_t cmd = addr;
  char *buf = (char *) &cmd;
  ::write(driver_to_xsim_fd, buf, 8);

  size_t gotdata = 0;
  while (gotdata == 0) {
    gotdata = ::read(xsim_to_driver_fd, buf, 8);
    if (gotdata != 0 && gotdata != 8) {
      std::cerr << "Error! Got data: " << gotdata << std::endl;
    }
  }
  return *((uint64_t *) buf);
}

uint32_t fpga_handle_real_t::is_write_ready() const {
  uint32_t value;
  // TODO - check this is right. addr used to be 0x4, which was CMD_VALID - I think CMD_READY is correct
  int rc = fpga_pci_peek(pci_bar_handle, CMD_READY, &value);
  check_rc(rc, "from is_write_ready");
  return value & 0xFFFFFFFF;
}

uint32_t fpga_handle_sim_t::is_write_ready() const {
  uint64_t cmd = CMD_READY;
  char *buf = (char *) &cmd;
  ::write(driver_to_xsim_fd, buf, 8);

  size_t gotdata = 0;
  while (gotdata == 0) {
    gotdata = ::read(xsim_to_driver_fd, buf, 8);
    if (gotdata != 0 && gotdata != 8) {
      std::cerr << "Error! Got data: " << gotdata << std::endl;
    }
  }
  return *((uint64_t *) buf);
}

bool fpga_handle_sim_t::is_real() const { return false; }

bool fpga_handle_real_t::is_real() const { return true; }

void fpga_handle_t::send(const rocc_cmd &cmd) const {
#ifndef NDEBUG
  printf("Sending the following commands:\n");
  cmd.decode();
  fflush(stdout);
#endif
  for (const uint32_t c: cmd.buf) {
    while (!read(CMD_READY)) {}
    write(CMD_BITS, c);
    write(CMD_VALID, 0x1);
  }
}

rocc_response fpga_handle_t::get_response() {
  rocc_response retval{};
  while (!read(RESP_VALID)) {}
  uint32_t resp1 = read(RESP_BITS);    // id
  retval.core_id = resp1 & 0xFF;
  retval.system_id = (resp1 & 0xFF00) >> 8;

  write(RESP_READY, 0x1);
  while (!read(RESP_VALID)) {}

  uint32_t resp2 = read(RESP_BITS);    // len / error
  retval.data = resp2;
  write(RESP_READY, 0x1);
  uint32_t rd;
  while (!read(RESP_VALID)) {}
  rd = read(RESP_BITS);
  retval.rd = rd;
  write(RESP_READY, 0x1);
  store_resp(RD(rd), resp1, resp2);
  return retval;
}

rocc_response fpga_handle_t::flush() {
  auto q = rocc_cmd::flush_cmd();
  send(q);
  return get_response();
}