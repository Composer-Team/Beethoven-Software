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

#ifndef FPGA_HANDLE_H
#define FPGA_HANDLE_H

#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <map>
#include "composer_verilator_server.h"

#ifdef USE_AWS
#include <fpga_mgmt.h>
#endif

#include <rocc.h>

struct fpga_handle_t {
  // return if the handle refers to a real FPGA or not
  [[nodiscard]] virtual bool is_real() const = 0;
  /**
   * @brief send a command to the FPGA
   * @return handle referring to response that the command will return. Allows for blocking on the response.
   */
  [[nodiscard]] virtual int send(const rocc_cmd &c) const = 0;
  /**
   * @brief using the handle returned by send, wait for a response
   * @param handle the handle returned by send
   * @return rocc_response
   */
  [[nodiscard]] virtual rocc_response get_response(int handle) = 0;

  /**
   * flush all in-flight commands
   */
   rocc_response flush();
};

class fpga_handle_sim_t : public fpga_handle_t {
public:
  explicit fpga_handle_sim_t();

  [[nodiscard]] int send(const rocc_cmd &c) const override;

  [[nodiscard]] rocc_response get_response(int handle) override;

  [[nodiscard]] bool is_real() const override;

  ~fpga_handle_sim_t();

  uint64_t malloc(size_t len);

  void copy_to_fpga(uint64_t fpga_addr, const void *host_addr, size_t len);

  void free(uint64_t fpga_addr);

private:
  cmd_server_file *cmd_server;
  int csfd;
  comm_file *data_server;
  int dsfd;
  std::map<uint64_t, std::tuple<int, void*, int> > device2virtual;
};

#ifdef USE_AWS
class fpga_handle_real_t : public fpga_handle_t {
public:
  explicit fpga_handle_real_t(int id);

  ~fpga_handle_real_t();

  void write(size_t addr, uint32_t data) const override;

  uint32_t read(size_t addr) const override;

  uint32_t is_write_ready() const override;

  void fpga_shutdown() const override;

  int get_write_fd() const override;

  int get_read_fd() const override;

  bool is_real() const override;

private:
  //    int rc;
  int slot_id = -1;
  pci_bar_handle_t pci_bar_handle = -1;
  int xdma_write_fd = -1;
  int xdma_read_fd = -1;
};
#endif

#endif
