//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "composer/fpga_handle.h"
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <vector>
#include <

#ifdef Kria

#include <sys/mman.h>

const unsigned kria_huge_page_sizes[] = {1 << 16, 1 << 21, 1 << 25, 1 << 30};
const unsigned kria_huge_page_flags[] = {16 << MAP_HUGE_SHIFT, 21 << MAP_HUGE_SHIFT, 25 << MAP_HUGE_SHIFT,
                                         30 << MAP_HUGE_SHIFT};
const unsigned kria_n_page_sizes = 4;

#endif

using namespace composer;


std::vector<fpga_handle_t *> composer::active_fpga_handles;

fpga_handle_t *composer::current_handle_context;

void composer::set_fpga_context(fpga_handle_t *handle) {
  current_handle_context = handle;
  if (std::find(active_fpga_handles.begin(), active_fpga_handles.end(), handle) == active_fpga_handles.end()) {
    std::cerr << "The provided handle appears to have not been properly constructed. Please use the provided"
                 " fpga_handle_t constructors." << std::endl;
    exit(1);
  }
}
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

#include "composer/verilator_server.h"
#include "composer/response_handle.h"
#include <unistd.h>
#include <pthread.h>

#include <fcntl.h>

using namespace composer;

#ifdef Kria
// https://stackoverflow.com/questions/2440385/how-to-find-the-physical-address-of-a-variable-from-user-space-in-linux
#include "cstdio"
#include "cinttypes"

const int __endian_bit = 1;
#define is_bigendian() ( (*(char*)&__endian_bit) == 0 )
#define PAGEMAP_ENTRY (8)
#define GET_BIT(X,Y) ((X & ((uint64_t)1<<Y)) >> Y)
#define GET_PFN(X) (X & 0x7FFFFFFFFFFFFF)

uint64_t vtop(unsigned long virt_addr) {
//  printf("Big endian? %d\n", is_bigendian());
  FILE *f = fopen("/proc/self/pagemap", "rb");
  if (!f) {
    std::cerr << "Error! Cannot open /proc/self/pagemap" << std::endl;
    throw std::exception();
  }

  //Shifting by virt-addr-offset number of bytes
  //and multiplying by the size of an address (the size of an entry in pagemap file)
  unsigned long file_offset = virt_addr / getpagesize() * PAGEMAP_ENTRY;
  printf("Vaddr: 0x%lx, Page_size: %d, Entry_size: %d\n", virt_addr, getpagesize(), PAGEMAP_ENTRY);
  printf("Reading pagemap at 0x%llx\n", (unsigned long long) file_offset);
  int status = fseek(f, (long)file_offset, SEEK_SET);
  if (status) {
    perror("Failed to do fseek!");
    return -1;
  }
  errno = 0;
  uint64_t read_val = 0;
  unsigned char c_buf[PAGEMAP_ENTRY];
  for (int i = 0; i < PAGEMAP_ENTRY; i++) {
    int c = getc(f);
    if (c == EOF) {
      printf("\nReached end of the file\n");
      return 0;
    }
    if (is_bigendian())
      c_buf[i] = (char)c;
    else
      c_buf[PAGEMAP_ENTRY - i - 1] = (char)c;
    printf("[%d]0x%x ", i, c);
  }
  for (unsigned char i : c_buf) {
    //printf("%d ",c_buf[i]);
    read_val = (read_val << 8) + i;
  }

  if (GET_BIT(read_val, 63)){
    fclose(f);
    return GET_PFN(read_val);
  } else {
    std::cerr << "Paged not mapped in!" << std::endl;
    fclose(f);
    throw std::exception();
  }
}

#endif

fpga_handle_t::fpga_handle_t() {
  csfd = shm_open(cmd_server_file_name.c_str(), O_RDWR, file_access_flags);
  if (csfd == -1) {
    std::cerr << "Error opening file " << cmd_server_file_name << " " << strerror(errno) << std::endl;
    exit(1);
  }
  cmd_server = (cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), file_access_prots, MAP_SHARED, csfd, 0);
  if (cmd_server == MAP_FAILED) {
    std::cerr << "Failed to map in cmd_server_file\t" << strerror(errno) << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }
  cmd_server->cmd = rocc_cmd::flush_cmd();

#ifndef Kria
  dsfd = shm_open(data_server_file_name.c_str(), O_RDWR, file_access_flags);
  if (dsfd < 0) {
    std::cerr << "Error opening file " << data_server_file_name << std::endl;
    exit(1);
  }
  data_server = (data_server_file *) mmap(nullptr, sizeof(data_server_file), file_access_prots, MAP_SHARED, dsfd, 0);
  if (data_server == MAP_FAILED) {
    std::cerr << "Failed to map in data_server_file" << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }
#endif
  active_fpga_handles.push_back(this);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"

#pragma clang diagnostic pop

fpga_handle_t::~fpga_handle_t() {
  munmap(cmd_server, sizeof(cmd_server_file));
  close(csfd);
  while (not device2virtual.empty()) {
    auto tup = device2virtual.begin();
    // TODO consider - is it proper to free it? fpga_handle_t has to be deconstructed but user may manually free segments
    //    in which case maybe the cleanest way for us to handle this is just implement unique_ptr
//    this->free(remote_ptr(tup->first, 0));
    void *mem = std::get<1>(tup->second);
    munmap(mem, std::get<2>(tup->second));
    shm_unlink(std::get<3>(tup->second).c_str());
    device2virtual.erase(device2virtual.begin());
  }
#ifndef Kria
  munmap(data_server, sizeof(data_server_file));
#endif
}


rocc_response fpga_handle_t::get_response_from_handle(int handle) const {
  // let response poller in server know that someone is now waiting
  int rc = pthread_mutex_lock(&cmd_server->process_waiting_count_lock);
  cmd_server->processes_waiting++;
  rc |= pthread_mutex_unlock(&cmd_server->process_waiting_count_lock);

  rc |= pthread_mutex_lock(&cmd_server->wait_for_response[handle]);
  // command is now ready
  auto resp = cmd_server->responses[handle];
  // now that we've read our response, we can release the resource to be used in future responses
  rc |= pthread_mutex_lock(&cmd_server->free_list_lock);
  cmd_server->free_list[++cmd_server->free_list_idx] = handle;
  rc |= pthread_mutex_unlock(&cmd_server->free_list_lock);
  if (rc) {
    printf("Error in fpga_handle.cc:get_response_from_handle\t%s\n", strerror(errno));
    exit(rc);
  }
  return resp;
}

response_handle fpga_handle_t::send(const rocc_cmd &c) const {
  // acquire lock over client side
  int error = pthread_mutex_lock(&cmd_server->cmd_send_lock);
  // communicate data to shared space
  cmd_server->cmd = c;
//  std::cout << "command in file is " << cmd_server->cmd << std::endl;
  // signal to server that we have a command ready
  error |= pthread_mutex_unlock(&cmd_server->server_mut);
  // wait for server to signal that it has read our command
  error |= pthread_mutex_lock(&cmd_server->cmd_recieve_server_resp_lock);
  // get the handle that we use to wait for response asynchronously
  uint64_t handle = cmd_server->pthread_wait_id;
  // release lock over client side
  error |= pthread_mutex_unlock(&cmd_server->cmd_send_lock);
  if (error) {
    printf("Error in send: %s\n", strerror(errno));
    fflush(stdout);
    exit(1);
  }
  return response_handle(c.getXd(), handle, *this);
}

remote_ptr fpga_handle_t::malloc(size_t len) {
#ifdef Kria
  void *addr;
  size_t sz;
  if (len <= 1 << 12) {
    sz = 1 << 12;
    addr = mmap(nullptr, sz, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_LOCKED | MAP_ANONYMOUS,
                -1, 0);
  } else {
    // Kria only uses local mappings via OS
    // see if allocation fits inside size classes
    int fit = -1;
    for (int i = 0; i < kria_n_page_sizes && fit == -1; ++i) {
      if (len < kria_huge_page_sizes[i])
        fit = i;
    }
    if (fit == -1) {
#ifndef NDEBUG
      std::cerr << "Error no size appropriate" << std::endl;
#endif
      return remote_ptr(0, nullptr, ERR_ALLOC_TOO_BIG);
    }

    addr = mmap(nullptr, kria_huge_page_sizes[fit], PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_HUGETLB | kria_huge_page_flags[fit] | MAP_ANONYMOUS | MAP_LOCKED,
                -1, 0);
    sz = kria_huge_page_sizes[fit];
  }
  if (addr == MAP_FAILED) {
#ifndef NDEBUG
    std::cerr << "Error in mmap: " << strerror(errno) << std::endl;
#endif
    return remote_ptr(errno, nullptr, ERR_MMAP_FAILURE);
  }


  // MUST lock to physical memory so it does not get swapped out and put back in some place different
//  int err = mlock(addr, kria_huge_page_sizes[fit]);
//
//  if (err == -1) {
//    munmap(addr, kria_huge_page_sizes[fit]);
//#ifndef NDEBUG
//    std::cerr << "Error in mlock: " << strerror(errno) << std::endl;
//#endif
//    return remote_ptr(errno, nullptr, ERR_MMAP_FAILURE);
//  }

  return remote_ptr(vtop((intptr_t) addr), addr, sz);
#else
  // acquire lock over client side
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->op_argument = len;
  data_server->operation = ALLOC;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  // now the server has returned the device addr (for building commands), and the file name
  uint64_t addr = data_server->op_argument;
  int fd = shm_open(data_server->fname, O_RDWR, file_access_flags);
  if (fd < 0) {
    std::cerr << "1) Error opening file '" << data_server->fname << "' - " << std::string(strerror(errno)) << std::endl;
    exit(1);
  }
  void *ptr = mmap(nullptr, len, file_access_prots, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    std::cerr << "Failed to map in file " << data_server->fname << " - " << std::string(strerror(errno)) << std::endl;

    exit(1);
  }
  // add device addr to private map
  device2virtual[addr] = std::make_tuple(fd, ptr, len, std::string(data_server->fname));
  // release lock over client side
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
  return remote_ptr(addr, ptr, len);
#endif
}

void fpga_handle_t::copy_to_fpga(const remote_ptr &dst) {
#ifndef Kria
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_TO_FPGA;
  data_server->op_argument = dst.getFpgaAddr();
  data_server->op3_argument = dst.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
#endif
}

void fpga_handle_t::copy_from_fpga(const remote_ptr &src) {
#ifndef Kria
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_FROM_FPGA;
  data_server->op2_argument = src.getFpgaAddr();
  data_server->op3_argument = src.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
#endif
}

void fpga_handle_t::free(remote_ptr ptr) {
#ifdef Kria
  munmap(ptr.getHostAddr(), ptr.getLen());
#else
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->op_argument = ptr.getFpgaAddr();
  data_server->operation = FREE;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  // now the server has returned the device addr (for building commands), and the file name
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
#endif
}

rocc_response fpga_handle_t::flush() {
  auto q = rocc_cmd::flush_cmd();
  auto i = send(q);
  return i.get();
}

void fpga_handle_t::shutdown() const {
  pthread_mutex_lock(&cmd_server->cmd_send_lock);
  pthread_mutex_unlock(&cmd_server->server_mut);
  cmd_server->quit = true;
  pthread_mutex_unlock(&cmd_server->cmd_send_lock);
}
