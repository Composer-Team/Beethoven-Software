/*
 * Copyright (c) 2022,
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

#ifndef COMPOSER_ALLOC_H
#define COMPOSER_ALLOC_H

#include <cinttypes>
#include <array>
#include <tuple>
#include <set>
#include <vector>
#include <pthread.h>
#include <iostream>
#include <algorithm>

namespace composer {

  class fpga_handle_t;

  enum shared_fpga_region_ty {
    /**
     * An FPGA buffer can be allocated and used in a couple different ways. These types are named to pertain
     * with how the FPGA itself interacts with the regions.
     * READ - The CPU writes data and the FPGA reads the data
     * READWRITE - The CPU and FPGA may both read and write to the segment. However, the segment is only
     *             guaranteed to exhibit low-granularity coherency: the FPGA will see all writes made by the CPU
     *             at all times, but the CPU will only see writes from the FPGA so long as the segment does **NOT**
     *             exist in the CPU cache during the entire execution of the FPGA kernel. The execution is defined
     *             as the point from when the command is sent, until a response handle is retrieved by .get().
     * FPGAONLY - The CPU never accesses the buffer and the FPGA operates on the memory in any desired way.
     */
    READ, READWRITE, FPGAONLY, WRITE
  };

  class remote_ptr {
    friend fpga_handle_t;

    uint64_t fpga_addr;
    void *host_addr;
    size_t len;
    int16_t allocation_id = -1;
  public:
    shared_fpga_region_ty allocation_type;

    [[nodiscard]] uint64_t getFpgaAddr() const {
      return fpga_addr;
    }

    [[nodiscard]] size_t getLen() const {
      return len;
    }

    [[nodiscard]] void *getHostAddr() const {
      return host_addr;
    }

    explicit remote_ptr(uint64_t fpgaAddr, size_t len, shared_fpga_region_ty ownership) :
            fpga_addr(fpgaAddr), len(len), host_addr(nullptr), allocation_type(ownership) {}

    explicit remote_ptr(uint64_t fpgaAddr, void *hostAddr, size_t len, shared_fpga_region_ty ownership)
            : fpga_addr(fpgaAddr), host_addr(hostAddr), len(len), allocation_type(ownership) {}

    explicit remote_ptr() : fpga_addr(0), host_addr(nullptr), len(0), allocation_type(FPGAONLY) {}

    bool operator==(const remote_ptr &other) const {
      return fpga_addr == other.fpga_addr && len == other.len;
    }

    template <typename t>
    explicit operator t() const {
      return static_cast<t>(host_addr);
    }
  };
}

#endif //COMPOSER_ALLOC_H
