//
// Created by Chris Kjellqvist on 9/19/22.
//

#include <cstdint>

#pragma once
namespace composer {
  uint64_t pack(uint32_t hi, uint32_t low);

  uint64_t log2Ceil(uint64_t num);

  uint64_t calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem);

  uint64_t calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem);

  uint64_t mask(uint64_t num, uint8_t length, uint8_t shift);

  struct composer_pack_info {
    int system_id_bits, core_id_bits, channelSelectionBits, transactionLengthBits;

    composer_pack_info(int systemIdBits, int coreIdBits, int channelSelectionBits, int transactionLengthBits);
  };

}