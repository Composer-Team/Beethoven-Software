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

#if !defined(BEETHOVEN_ALLOC_H) && !defined(BEETHOVEN_BAREMETAL)
#define BEETHOVEN_ALLOC_H

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <array>
#include <tuple>
#include <set>
#include <vector>
#include <pthread.h>
#include <iostream>
#include <algorithm>

namespace beethoven {

  class fpga_handle_t;

  class remote_ptr {
    friend fpga_handle_t;

    intptr_t fpga_addr;
    void *host_addr;

    size_t len;
    ptrdiff_t offset;
    std::mutex *mutex = nullptr;
    uint16_t *count = nullptr;

    remote_ptr(const intptr_t &faddr, void *haddr, const size_t &l,
               uint16_t *c,
               std::mutex *m,
               ptrdiff_t off
    ) noexcept;

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

    explicit remote_ptr(intptr_t fpgaAddr, void *hostAddr, size_t len);

    explicit remote_ptr();

    bool operator==(const remote_ptr &other) const {
      return fpga_addr == other.fpga_addr;
    }

    template<typename t>
    explicit operator t() const {
      return static_cast<t>(host_addr);
    }

    remote_ptr(const remote_ptr &other) noexcept;

    explicit remote_ptr(const intptr_t &faddr) noexcept;

    remote_ptr(const intptr_t &faddr, void *haddr, const size_t &l, ptrdiff_t off) noexcept;

    // don't need move constructor for baremetal
    remote_ptr(remote_ptr &&other) noexcept;

    remote_ptr &operator=(remote_ptr &&) noexcept;

    remote_ptr &operator=(const remote_ptr &other) noexcept;

    ~remote_ptr();

    remote_ptr operator+(int q) const;

    remote_ptr operator-(int q) const;
  };
}

#endif //BEETHOVEN_ALLOC_H
