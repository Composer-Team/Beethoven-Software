//
// Created by Chris Kjellqvist on 9/19/22.
//

#include "beethoven/util.h"


//uint64_t beethoven::pack(uint32_t hi, uint32_t low) {
//  return (((uint64_t) hi) << 32) | ((uint64_t) low);
//}


//uint64_t beethoven::log2Ceil(uint64_t num) {
//  uint64_t power = 1;
//  while (power < num) {
//    power = power * 2;
//  }
//  return power;
//}

//uint64_t beethoven::calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
//  return prev_addr + num_elems * bytes_per_elem;
//}

//uint64_t beethoven::calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
//  uint64_t min_addr = calcNextAddr(prev_addr, num_elems, bytes_per_elem);
//  int remain = int(min_addr & 0x7FL);
//  return min_addr + 128 - remain;
//}

beethoven::beethoven_pack_info::beethoven_pack_info(
        int systemIdBits, int coreIdBits) :
        system_id_bits(systemIdBits),
        core_id_bits(coreIdBits) {}

//uint64_t beethoven::mask(uint64_t num, uint8_t length, uint8_t shift) {
//  uint64_t acc = 0;
//  for (uint16_t i = 0; i < length; ++i) {
//    acc = (acc << 1) | 1;
//  }
//  return (num & acc) << shift;
//}
