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

#ifndef BEETHOVEN_ALLOC_BAREMETAL_H
#define BEETHOVEN_ALLOC_BAREMETAL_H

#include <cstddef>
#include <cstdint>
#include <bit>

namespace beethoven {

class fpga_handle_t;

class remote_ptr {
  friend fpga_handle_t;

  intptr_t fpga_addr;
public:

  [[nodiscard]] uint64_t getFpgaAddr() const { return fpga_addr; }

  [[nodiscard]] void *getHostAddr() const { return reinterpret_cast<void*>(fpga_addr); }

  constexpr explicit remote_ptr() : fpga_addr(0) {}
  constexpr ~remote_ptr() {}

  bool operator==(const remote_ptr &other) const {
    return fpga_addr == other.fpga_addr;
  }

  remote_ptr(const remote_ptr &other) noexcept
      : fpga_addr(other.fpga_addr) {};

  constexpr explicit remote_ptr(const intptr_t &faddr) noexcept
      : fpga_addr(faddr) {}

  constexpr remote_ptr &operator=(remote_ptr &&other) noexcept {
    fpga_addr = other.fpga_addr;
    return *this;
  }

  constexpr remote_ptr &operator=(const remote_ptr &other) noexcept {
    fpga_addr = other.fpga_addr;
    return *this;
  }

  remote_ptr operator+(int q) const;

  remote_ptr operator-(int q) const;
};
} // namespace beethoven

#endif // BEETHOVEN_ALLOC_BAREMETAL_H
