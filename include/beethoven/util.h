//
// Created by Chris Kjellqvist on 9/19/22.
//

#ifndef BEETHOVEN_UTIL_H
#define BEETHOVEN_UTIL_H

#include <cstdint>
namespace beethoven {
//  uint64_t pack(uint32_t hi, uint32_t low);
//
//  uint64_t log2Ceil(uint64_t num);

//  uint64_t calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem);

//  uint64_t calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem);
//
//  uint64_t mask(uint64_t num, uint8_t length, uint8_t shift);

  struct beethoven_pack_info {
    int system_id_bits, core_id_bits;

    beethoven_pack_info(int systemIdBits, int coreIdBits);
  };

#ifdef BAREMETAL
  // TODO inspect ARM assembly
  template <typename t=uint32_t>
  void poke_addr(intptr_t ptr, t r) {
    *(reinterpret_cast<volatile t*>(ptr)) = r;
    return;
  }

  template <typename t=uint32_t>
  t peek_addr(intptr_t ptr) {
    return *(reinterpret_cast<volatile t*>(ptr));
  }
#endif

}
#endif
