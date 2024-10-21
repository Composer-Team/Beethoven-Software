//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "beethoven/fpga_handle.h"
#ifndef BAREMETAL
#include <iostream>
#include <algorithm>
#include <cerrno>
#include <vector>
#include <stdexcept>
#endif
#include <cstring>
#include "beethoven/alloc.h"

using namespace beethoven;


std::vector<fpga_handle_t *> beethoven::active_fpga_handles;

fpga_handle_t *beethoven::current_handle_context;

[[maybe_unused]] void beethoven::set_fpga_context(fpga_handle_t *handle) {
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

#include "beethoven/verilator_server.h"
#include "beethoven/response_handle.h"
#include <unistd.h>
#ifndef Kria
#include <pthread.h>
#endif

#include <fcntl.h>

using namespace beethoven;

static int cacheLineSz;
static int logCacheLineSz;

fpga_handle_t::fpga_handle_t() {
  // map in command server
  auto cmd_fname = cmd_server_file_name();
  csfd = shm_open(cmd_fname.c_str(), O_RDWR, file_access_flags);
  if (csfd == -1) {
    std::cerr << "Error opening file " << cmd_fname << " " << strerror(errno) << std::endl;
    exit(1);
  }
  cmd_server = (cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), file_access_prots, MAP_SHARED, csfd, 0);
  if (cmd_server == MAP_FAILED) {
    std::cerr << "Failed to map in cmd_server_file\t" << strerror(errno) << std::endl;
    std::cerr << strerror(errno) << std::endl;
    exit(1);
  }

  cmd_server->quit = false;

  // map in data server
  auto dataserver_fname = data_server_file_name();
  dsfd = shm_open(dataserver_fname.c_str(), O_RDWR, file_access_flags);
  if (dsfd < 0) {
    std::cerr << "Error opening file " << dataserver_fname << std::endl;
    exit(1);
  }
  data_server = (data_server_file *) mmap(nullptr, sizeof(data_server_file), file_access_prots, MAP_SHARED, dsfd, 0);
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
  munmap(data_server, sizeof(data_server_file));
}


rocc_response fpga_handle_t::get_response_from_handle(int handle) const {
  int rc = pthread_mutex_lock(&cmd_server->wait_for_response[handle]);
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

std::optional<rocc_response> fpga_handle_t::try_get_response_from_handle(int handle) const {
  int rc = pthread_mutex_trylock(&cmd_server->wait_for_response[handle]);
  if (rc) return {};
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


response_handle<rocc_response> fpga_handle_t::send(const rocc_cmd &c) {
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
  int handle = cmd_server->pthread_wait_id;
  // release lock over client side
  error |= pthread_mutex_unlock(&cmd_server->cmd_send_lock);

  cmd_server->quit = false;
  if (error) {
    printf("Error in send: %s\n", strerror(errno));
    fflush(stdout);
    exit(1);
  }
  return response_handle<rocc_response>(c.getXd(), handle, *this);
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

  int fd = shm_open(data_server->fname, O_RDWR, file_access_flags);
  if (fd < 0) {
    throw std::runtime_error("Failed to open file " + std::string(data_server->fname) + " - " + std::string(strerror(errno)));
  }
  void *ptr = mmap(nullptr, len, file_access_prots, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    throw std::runtime_error("Failed to map in file " + std::string(data_server->fname) + " - " + std::string(strerror(errno)));
  }
  // add device addr to private map
  device2virtual[addr] = std::make_tuple(fd, ptr, len, std::string(data_server->fname));
  // release lock over client side
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
  return remote_ptr(addr, ptr, len);
}

[[maybe_unused]] void fpga_handle_t::copy_to_fpga(const remote_ptr &dst) {
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_TO_FPGA;
  data_server->op_argument = dst.getFpgaAddr();
  data_server->op3_argument = dst.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}

[[maybe_unused]] void fpga_handle_t::copy_from_fpga(const remote_ptr &src) {
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->operation = data_server_op::MOVE_FROM_FPGA;
  data_server->op2_argument = src.getFpgaAddr();
  data_server->op3_argument = src.getLen();
  pthread_mutex_unlock(&data_server->server_mut);
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}

[[maybe_unused]] void fpga_handle_t::free(remote_ptr ptr) {
  pthread_mutex_lock(&data_server->data_cmd_send_lock);
  data_server->op_argument = ptr.getFpgaAddr();
  data_server->operation = FREE;
  pthread_mutex_unlock(&data_server->server_mut);
  // wait for server to signal that it has read our command
  pthread_mutex_lock(&data_server->data_cmd_recieve_resp_lock);
  // now the server has returned the device addr (for building commands), and the file name
  pthread_mutex_unlock(&data_server->data_cmd_send_lock);
}

[[maybe_unused]] void fpga_handle_t::shutdown() const {
  pthread_mutex_lock(&cmd_server->cmd_send_lock);
  pthread_mutex_unlock(&cmd_server->server_mut);
  cmd_server->quit = true;
  pthread_mutex_unlock(&cmd_server->cmd_send_lock);
}

[[maybe_unused]] void fpga_handle_t::request_startup() {
  // first, check that there isn't already a BeethovenRuntime process running
  // if there is, then we don't need to do anything

  // use ps utility to see if process is running
  // if it is, then we don't need to do anything

  // this is rough but maybe it can work alright
  system("killall BeethovenRuntime");

  // try to build and startup the beethoven runtime FOR SIMULATION ONLY
  // record current working directory
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  // change to beethoven runtime directory
  // get $BEETHOVEN_ROOT environment variable
  char *beethoven_root = getenv("BEETHOVEN_ROOT");
  // change to beethoven_root/Beethoven-Hardware/vsim/generated-src
  chdir(beethoven_root);
  chdir("Beethoven-Runtime");
  // make a build directory if one does not exist
  mkdir("build", 0777);
  // change to build directory
  chdir("build");
  // run cmake
  system("cmake .. -DTARGET=sim -DCMAKE_BUILD_TYPE=Debug");
  // run make
  system("make -j");
  // fork exec the executable that was just built (./BeethovenRuntime)
  // output system out to log file in the same directory (Beethoven.log)
  // then change back to original directory
  // fork and exec
  printf("FORKING\n");
  pid_t pid = fork();
  if (pid == 0) {
    // child
    // redirect stdout and stderr to log file
    int fd = open("Beethoven.log", O_WRONLY | O_CREAT | O_TRUNC, 0777);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
    // exec
    execl("./BeethovenRuntime", "./BeethovenRuntime", nullptr);
    // if we get here, exec failed
    std::cerr << "Failed to exec BeethovenRuntime" << std::endl;
    exit(1);
  } else if (pid < 0) {
    // fork failed
    std::cerr << "Failed to fork BeethovenRuntime" << std::endl;
    exit(1);
  } else {
    // parent
    // change back to original directory
    chdir(cwd);
  }
  sleep(1);
  printf("Returning\n");
}
