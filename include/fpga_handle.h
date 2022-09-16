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

#ifndef __SIMIF_F1_H
#define __SIMIF_F1_H

//#include "simif.h"    // from midas
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <map>

#ifndef SIMULATION_XSIM

#include <fpga_pci.h>
#include <fpga_mgmt.h>

#endif

#define MMIO_WIDTH 8

struct fpga_handle_t {
  virtual void write(size_t addr, uint32_t data) = 0;

  virtual uint32_t read(size_t addr) = 0;

  virtual uint32_t is_write_ready() = 0;

  virtual void check_rc(int rc, std::string infostr) = 0;

  virtual void fpga_shutdown() = 0;

  virtual int get_write_fd() = 0;

  virtual int get_read_fd() = 0;

  void store_resp(uint32_t rd, uint32_t unit_id, uint32_t retval);

  uint32_t resp_lookup(uint32_t rd, uint32_t unit_id);

  virtual bool is_real() = 0;

protected:
  char in_buf[MMIO_WIDTH];
  char out_buf[MMIO_WIDTH];
  std::map<uint32_t, std::map<uint32_t, uint32_t> > rocc_resp_table;
};

class fpga_handle_sim_t : public fpga_handle_t {
public:
  explicit fpga_handle_sim_t(int id);

  ~fpga_handle_sim_t();

  void write(size_t addr, uint32_t data) override;

  uint32_t read(size_t addr) override;

  uint32_t is_write_ready() override;

  void check_rc(int rc, std::string infostr) override;

  void fpga_shutdown() override;

  int get_write_fd() override;

  int get_read_fd() override;

  bool is_real() override;

private:
  char driver_to_xsim[1024];
  char xsim_to_driver[1024];
  int driver_to_xsim_fd = -1;
  int xsim_to_driver_fd = -1;
};

class fpga_handle_real_t : public fpga_handle_t {
public:
  explicit fpga_handle_real_t(int id);

  ~fpga_handle_real_t();

  void write(size_t addr, uint32_t data) override;

  uint32_t read(size_t addr) override;

  uint32_t is_write_ready() override;

  void check_rc(int rc, std::string infostr) override;

  void fpga_shutdown() override;

  void fpga_setup(int id);

  int get_write_fd() override;

  int get_read_fd() override;

  bool is_real() override;

private:
  //    int rc;
  int slot_id = -1;
  pci_bar_handle_t pci_bar_handle = -1;
  int xdma_write_fd = -1;
  int xdma_read_fd = -1;
};

#endif // __SIMIF_F1_H
