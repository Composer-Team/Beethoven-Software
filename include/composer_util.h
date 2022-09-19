//
// Created by Chris Kjellqvist on 9/19/22.
//

#include <cstdint>

#pragma once

uint64_t pack(uint32_t hi, uint32_t low);

uint64_t log2Ceil(uint64_t num);

uint64_t calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem);

uint64_t calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem);
