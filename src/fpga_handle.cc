//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "composer/fpga_handle.h"
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <vector>

using namespace composer;

std::vector<fpga_handle_t*> composer::active_fpga_handles;

fpga_handle_t* composer::current_handle_context;

void composer::set_fpga_context(fpga_handle_t* handle) {
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
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include "composer/alloc.h"
#include <cerrno>

// for shared memory allocation + mmap
#include <sys/mman.h>
#include <fcntl.h>

using namespace composer;


fpga_handle_t::fpga_handle_t() {
  csfd = shm_open(cmd_server_file_name.c_str(), O_RDWR, S_IROTH | S_IWOTH);
  if (csfd == -1) {
    std::cerr << "Error opening file " << cmd_server_file_name << " " << strerror(errno) << std::endl;
    exit(1);
  }
  cmd_server = (cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), PROT_READ | PROT_WRITE, MAP_SHARED, csfd, 0);
  if (cmd_server == MAP_FAILED) {
    std::cerr << "Failed to map in cmd_server_file\t" << strerror(errno) <<  std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  cmd_server->cmd = rocc_cmd::flush_cmd();
  dsfd = shm_open(data_server_file_name.c_str(), O_RDWR, S_IWOTH | S_IROTH);
  if (dsfd < 0) {
    std::cerr << "Error opening file " << data_server_file_name << std::endl;
    exit(1);
  }
  data_server = (data_server_file *) mmap(nullptr, sizeof(data_server_file), PROT_READ | PROT_WRITE, MAP_SHARED, dsfd, 0);
  if (data_server == MAP_FAILED) {
    std::cerr << "Failed to map in data_server_file" << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }
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
}


rocc_response fpga_handle_t::get_response_from_handle(int handle) const {
  pthread_mutex_lock(&cmd_server->wait_for_response[handle]);
  // command is now ready
  auto resp = cmd_server->responses[handle];
  // now that we've read our response, we can release the resource to be used in future responses
  pthread_mutex_lock(&cmd_server->free_list_lock);
  cmd_server->free_list[++cmd_server->free_list_idx] = handle;
  pthread_mutex_unlock(&cmd_server->free_list_lock);
  return resp;
}

response_handle fpga_handle_t::send(const rocc_cmd &c) const {
  // acquire lock over client side
  int success = pthread_mutex_lock(&cmd_server->cmd_send_lock);
  // communicate data to shared space
  cmd_server->cmd = c;
  std::cout << "command in file is " << cmd_server->cmd << std::endl;
  // signal to server that we have a command ready
  success && pthread_mutex_unlock(&cmd_server->server_mut);
  // wait for server to signal that it has read our command
  success && pthread_mutex_lock(&cmd_server->cmd_recieve_server_resp_lock);
  // get the handle that we use to wait for response asynchronously
  uint64_t handle = cmd_server->pthread_wait_id;
  // release lock over client side
  success && pthread_mutex_unlock(&cmd_server->cmd_send_lock);
  if (not success) {
    printf("something failed! surprise!");
    fflush(stdout);
  }
  return response_handle(c.getXd(), handle, *this);
}

remote_ptr fpga_handle_t::malloc(size_t len) {
  // acquire lock over client side
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->op_argument = len;
  data_server->operation = ALLOC;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  // now the server has returned the device addr (for building commands), and the file name
  uint64_t addr = data_server->op_argument;
  int fd = shm_open(data_server->fname, O_RDWR, S_IWUSR | S_IRUSR);
  if (fd < 0) {
    std::cerr << "1) Error opening file '" << data_server->fname << "'" << std::endl;
    exit(1);
  }
  void *ptr = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    std::cerr << "Failed to map in file " << data_server->fname << std::endl;
    exit(1);
  }
  // add device addr to private map
  device2virtual[addr] = std::make_tuple(fd, ptr, len, std::string(data_server->fname));
  // release lock over client side
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
  return remote_ptr(addr, len);
}

void fpga_handle_t::copy_to_fpga(const remote_ptr &dst, const void *host_addr) {
  auto it = device2virtual.find(dst.getFpgaAddr());
  if (it == device2virtual.end()) {
    std::cerr << "Error: copy_to_fpga called with invalid fpga_addr" << std::endl;
    std::cerr << "\t Make sure that you're using the device addr returned from fpga_handle_t::malloc"
                 " and not a host address. Also, make sure it is the base address returned." << std::endl;
    exit(1);
  }
  auto tup = it->second;
//  memcpy(std::get<1>(tup), host_addr, dst.getLen());

  printf("copying to fpga %p %p %x\n", std::get<1>(tup), host_addr, dst.getLen());
  int *p = (int*)std::get<1>(tup);
  for (int *pt = p, *hs = (int*)host_addr; pt < p + dst.getLen()/sizeof(int); pt++, hs++ ) {
    printf("copying %x to %p\n", *hs, pt);
    *pt = *hs;
  }
  return;
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_TO_FPGA;
  data_server->op_argument = dst.getFpgaAddr();
  data_server->op2_argument = (uint64_t)host_addr;
  data_server->op3_argument = dst.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}

void fpga_handle_t::copy_from_fpga(void *host_addr, const remote_ptr &src) {
  auto it = device2virtual.find(src.getFpgaAddr());
  if (it == device2virtual.end()) {
    std::cerr << "Error: copy_from_fpga called with invalid fpga_addr" << std::endl;
    std::cerr << "\t Make sure that you're using the device addr returned from fpga_handle_t::malloc"
                 " and not a host address. Also, make sure it is the base address returned." << std::endl;
    exit(1);
  }
  void *srcaddr = std::get<1>(it->second);
  memcpy(host_addr, srcaddr, src.getLen());
//  int *p = (int*)std::get<1>(it->second);
//  for (int *pt = p, *hs = (int*)host_addr; pt < p + src.getLen()/sizeof(int); pt++, hs++ ) {
//    printf("copying %x from %p\n", *pt, pt);
//    *hs = *pt;
//  }
  return;
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_TO_FPGA;
  data_server->op_argument = (uint64_t)host_addr;
  data_server->op2_argument = src.getFpgaAddr();
  data_server->op3_argument = src.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}

void fpga_handle_t::free(remote_ptr ptr) {
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->op_argument = ptr.getFpgaAddr();
  data_server->operation = FREE;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  // now the server has returned the device addr (for building commands), and the file name
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}


rocc_response fpga_handle_t::flush() {
  auto q = rocc_cmd::flush_cmd();
  auto i = send(q);
  return i.get();
}
