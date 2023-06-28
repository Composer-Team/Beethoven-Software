//
// Created by Chris Kjellqvist on 11/10/22.
//

#ifndef COMPOSER_ALLOCATOR_PTR_H
#define COMPOSER_ALLOCATOR_PTR_H

#include <cinttypes>
#include <string>
#include "fpga_handle.h"

namespace composer {
  class remote_ptr {
    // NOTE: For security purposes we could probably pack this structure full of information that guarantees that the
    //       user is behaving
    uint64_t fpga_addr;
    void * host_addr;
    uint32_t len;
  public:
    bool freed;

    explicit remote_ptr(size_t size) {
      auto me = current_handle_context->malloc(size);
      fpga_addr = me.getFpgaAddr();
      host_addr = me.getHostAddr();
      len = me.getLen();
      freed = false;
    }

    [[nodiscard]] uint64_t getFpgaAddr() const {
      return fpga_addr;
    }

    [[nodiscard]] uint32_t getLen() const {
      return len;
    }

    [[nodiscard]] void *getHostAddr() const {
      return host_addr;
    }

    explicit remote_ptr(uint64_t fpgaAddr, uint32_t len) : fpga_addr(fpgaAddr), len(len), freed(false), host_addr(nullptr) {}

    explicit remote_ptr(uint64_t fpgaAddr, void *hostAddr, uint32_t len) : fpga_addr(fpgaAddr), host_addr(hostAddr), len(len), freed(false){}

    [[nodiscard]] std::string printError();

  };

  const int ERR_ALLOC_TOO_BIG = 0x1;
  const int ERR_MMAP_FAILURE = 0x2;
  const int ERR_MLOCK_FAILURE = 0x4;
}

#endif //COMPOSER_ALLOCATOR_PTR_H
