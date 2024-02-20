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

  class remote_ptr {
    // TODO make these shared pointers
    friend fpga_handle_t;

    uint64_t fpga_addr;
    void *host_addr;
    size_t len;
    int16_t allocation_id = -1;
  public:
    [[nodiscard]] uint64_t getFpgaAddr() const {
      return fpga_addr;
    }

    [[nodiscard]] size_t getLen() const {
      return len;
    }

    [[nodiscard]] void *getHostAddr() const {
      return host_addr;
    }

    explicit remote_ptr(uint64_t fpgaAddr, size_t len) :
            fpga_addr(fpgaAddr), len(len), host_addr(nullptr) {}

    explicit remote_ptr(uint64_t fpgaAddr, void *hostAddr, size_t len)
            : fpga_addr(fpgaAddr), host_addr(hostAddr), len(len) {}

    explicit remote_ptr() : fpga_addr(0), host_addr(nullptr), len(0) {}

    bool operator==(const remote_ptr &other) const {
      return fpga_addr == other.fpga_addr && len == other.len;
    }

    template <typename t>
    explicit operator t() const {
      return static_cast<t>(host_addr);
    }

    remote_ptr(const remote_ptr &other) = default;

    remote_ptr operator +(int q) const {
      remote_ptr other;
      other = *this;
      other.fpga_addr += q;
      other.len -= q;
      other.host_addr = (char*)(other.host_addr) + q;
      return other;
    }

    remote_ptr operator -(int q) const {
      remote_ptr other;
      other = *this;
      other.fpga_addr -= q;
      other.len += q;
      other.host_addr = (char*)(other.host_addr) - q;
      return other;
    }
  };
}

#endif //COMPOSER_ALLOC_H
