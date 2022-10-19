//
// Created by Chris Kjellqvist on 9/19/22.
//

#include "composer/util.h"


uint64_t composer::pack(uint32_t hi, uint32_t low) {
  return (((uint64_t) hi) << 32) | ((uint64_t) low);
}


uint64_t composer::log2Ceil(uint64_t num) {
  uint64_t power = 1;
  while (power < num) {
    power = power * 2;
  }
  return power;
}

uint64_t composer::calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
  return prev_addr + num_elems * bytes_per_elem;
}

uint64_t composer::calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
  uint64_t min_addr = calcNextAddr(prev_addr, num_elems, bytes_per_elem);
  int remain = int(min_addr & 0x7FL);
  return min_addr + 128 - remain;
}