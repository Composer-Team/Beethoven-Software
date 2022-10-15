//
// Created by Chris Kjellqvist on 10/14/22.
//

#ifndef COMPOSER_COMPOSER_ALLOC_H
#define COMPOSER_COMPOSER_ALLOC_H

#include <cinttypes>
#include <tuple>

namespace composer {

  class remote_ptr {
    // NOTE: For security purposes we could probably pack this structure full of information that guarantees that the
    //       user is behaving
    uint64_t fpga_addr;
  public:
    explicit remote_ptr(uint64_t fpgaAddr);
    [[nodiscard]] uint64_t getFpgaAddr() const;

  };
  remote_ptr remote_alloc(uint64_t len);
  void remote_free(const remote_ptr &p);

}

#endif //COMPOSER_COMPOSER_ALLOC_H
