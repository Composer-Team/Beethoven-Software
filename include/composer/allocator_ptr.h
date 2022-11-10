//
// Created by Chris Kjellqvist on 11/10/22.
//

#ifndef COMPOSER_ALLOCATOR_PTR_H
#define COMPOSER_ALLOCATOR_PTR_H

#include <cinttypes>
namespace composer {
  class remote_ptr {
    // NOTE: For security purposes we could probably pack this structure full of information that guarantees that the
    //       user is behaving
    uint64_t fpga_addr;
    uint32_t len;
  public:
    bool freed;

    [[nodiscard]] uint64_t getFpgaAddr() const {
      return fpga_addr;
    }

    [[nodiscard]] uint32_t getLen() const {
      return len;
    }

    explicit remote_ptr(uint64_t fpgaAddr, uint32_t len) : fpga_addr(fpgaAddr), len(len), freed(false) {}
  };
}

#endif //COMPOSER_ALLOCATOR_PTR_H
