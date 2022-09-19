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

#pragma once

//#include "simif.h"    // from midas
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <map>

#include <fpga_pci.h>
#include <fpga_mgmt.h>
#include "rocc.h"

#define ROCC_CMD_ACCEL 0x7b
#define ROCC_CMD_FLUSH 0xb

#define RESP_BITS 0 //READONLY
#define RESP_VALID 1 //READONLY
#define RESP_READY 2 //WRITEONLY

#define CMD_BITS 3 //WRITEONLY
#define CMD_VALID 4 //WRITEONLY
#define CMD_READY 5 //READONLY


struct fpga_handle_t {
  virtual void write(size_t addr, uint32_t data) const = 0;

  virtual uint32_t read(size_t addr) const = 0;

  virtual uint32_t is_write_ready() const = 0;

  void check_rc(int rc, const std::string& infostr) const;

  virtual void fpga_shutdown() const = 0;

  virtual int get_write_fd() const = 0;

  virtual int get_read_fd() const = 0;

  void store_resp(RD rd, uint32_t unit_id, uint32_t retval);

  uint32_t resp_lookup(RD rd, uint32_t unit_id) const;

  virtual bool is_real() const = 0;

  void send(const rocc_cmd &) const;

  rocc_response get_response();

  rocc_response flush();

protected:
  std::map<uint32_t, std::map<uint32_t, uint32_t> > rocc_resp_table;
};

class fpga_handle_sim_t : public fpga_handle_t {
public:
  explicit fpga_handle_sim_t();

  ~fpga_handle_sim_t();

  void write(size_t addr, uint32_t data) const override;

  uint32_t read(size_t addr) const override;

  uint32_t is_write_ready() const override;

  void fpga_shutdown() const override;

  int get_write_fd() const override;

  int get_read_fd() const override;

  bool is_real() const override;

private:
  char driver_to_xsim[1024]{};
  char xsim_to_driver[1024]{};
  int driver_to_xsim_fd = -1;
  int xsim_to_driver_fd = -1;
};

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

