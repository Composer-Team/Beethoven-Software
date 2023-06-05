//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "composer/rocc_response.h"
using namespace composer;

rocc_response::rocc_response(const uint32_t *buffer, const composer_pack_info &pack_info) {
  this->rd = (uint8_t) buffer[2] & 0x1F;
  this->system_id = (buffer[2] >> 5) & ((1 << pack_info.system_id_bits) - 1);
  this->core_id = (buffer[2] >> (5 + pack_info.system_id_bits)) & ((1 << pack_info.core_id_bits) - 1);
  this->data = (uint64_t)(buffer[0]) | buffer[1];
}
