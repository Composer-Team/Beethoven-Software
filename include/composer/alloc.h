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
#include <cstddef>
#include <memory>
#include <mutex>

#ifndef BAREMETAL
#include <array>
#include <tuple>
#include <set>
#include <vector>
#include <pthread.h>
#include <iostream>
#include <algorithm>
#endif

namespace composer {

  class fpga_handle_t;

  class remote_ptr {
    friend fpga_handle_t;

    uint64_t fpga_addr;
    void *host_addr;
    size_t len;
    ptrdiff_t offset = 0;

    std::mutex *mutex = nullptr;
    uint16_t *count = nullptr;

    remote_ptr(const uint64_t &faddr, void *haddr,
               const size_t &l, uint16_t *c,
               std::mutex *m, ptrdiff_t off)  noexcept :
            fpga_addr(faddr),
            host_addr(haddr),
            len(l),
            count(c),
            mutex(m) {
      if (mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        (*count)++;
      }
    }
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

//    explicit remote_ptr(uint64_t fpgaAddr, size_t len) :
//            fpga_addr(fpgaAddr), len(len), host_addr(nullptr) {}

    explicit remote_ptr(uint64_t fpgaAddr, void *hostAddr, size_t len):
            fpga_addr(fpgaAddr),
            host_addr(hostAddr),
            len(len) {
      mutex = new std::mutex();
      count = new uint16_t(1);
    }

    explicit remote_ptr() :
            fpga_addr(0),
            host_addr(nullptr),
            len(0),
            mutex(nullptr),
            count(nullptr) {}

    bool operator==(const remote_ptr &other) const {
      return fpga_addr == other.fpga_addr && len == other.len;
    }

    template<typename t>
    explicit operator t() const {
      return static_cast<t>(host_addr);
    }

    remote_ptr(const remote_ptr & other)  noexcept :
            fpga_addr(other.fpga_addr),
            host_addr(other.host_addr),
            len(other.len),
            count(other.count),
            mutex(other.mutex) {
      if (mutex) {
        std::lock_guard<std::mutex> lock(*mutex);
        (*count)++;
      }
    };

    remote_ptr(remote_ptr && other) noexcept :
            fpga_addr(other.fpga_addr),
            host_addr(other.host_addr),
            len(other.len),
            count(other.count),
            mutex(other.mutex) {
    };

    remote_ptr& operator=(remote_ptr &&) noexcept;

    remote_ptr& operator=(const remote_ptr& other) noexcept;

    ~remote_ptr();

    remote_ptr operator+(int q) const {
      return remote_ptr(this->fpga_addr + q, (char *) (this->host_addr) + q, this->len - q, this->count, this->mutex, offset+q);
    }

    remote_ptr operator-(int q) const {
      return remote_ptr(this->fpga_addr - q, (char *) (this->host_addr) - q, this->len + q, this->count, this->mutex, offset-q);
    }
  };
}

#endif //COMPOSER_ALLOC_H
