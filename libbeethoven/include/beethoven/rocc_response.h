//
// Created by Chris Kjellqvist on 10/19/22.
//

#ifndef BEETHOVEN_ROCC_RESPONSE_H
#define BEETHOVEN_ROCC_RESPONSE_H

#include <cinttypes>
#include "util.h"

namespace beethoven {
  struct rocc_response {
    uint64_t data;
    uint8_t core_id;
    uint8_t system_id;
    uint8_t rd;
    rocc_response(const uint32_t *buffer, const beethoven_pack_info &pack_info);
    rocc_response() = default;
  };

}
#endif //BEETHOVEN_ROCC_RESPONSE_H
