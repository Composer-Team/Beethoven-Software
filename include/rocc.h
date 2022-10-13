/*
 * Copyright (c) 2019,
 * The University of California, Berkeley and Duke University.
 * All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROCC_H
#define ROCC_H

#include <zconf.h>
#include <composer_consts.h>
#include <cstdint>
#include <ostream>


struct rocc_cmd {
  /**
   * Generate a command to start kernel execution on the accelerator. TODO: maybe an accelerator
   * should be able to support more than one command per functional unit. You could of course do that
   * by commandeering another instruction field (e.g. rs1) but it would be more natural to use the
   * function bits
   * @param system_id
   * @param rs1_num
   * @param rs2_num
   * @param xd
   * @param rd
   * @param xs1
   * @param xs2
   * @param core_id
   * @param rs1
   * @param rs2
   * @return
   */

  uint16_t function;
  uint16_t system_id;
  uint8_t opcode;
  uint8_t rs1_num;
  uint8_t rs2_num;
  uint8_t xd;
  RD rd;
  uint8_t xs1;
  uint8_t xs2;
  uint8_t core_id;
  uint64_t rs1;
  uint64_t rs2;

  static rocc_cmd start_cmd(uint16_t system_id,
                            uint8_t rs1_num,
                            uint8_t rs2_num,
                            bool expect_response,
                            RD rd,
                            uint8_t xs1,
                            uint8_t xs2,
                            uint8_t core_id,
                            uint64_t rs1,
                            uint64_t rs2);

  static rocc_cmd addr_cmd(uint16_t system_id,
                           uint8_t core_id,
                           uint8_t channel_id,
                           uint64_t addr);

  static rocc_cmd flush_cmd();

  [[nodiscard]] uint32_t *pack() const;

  friend std::ostream &operator<<(std::ostream &os, const rocc_cmd &cmd);

private:
  rocc_cmd(uint16_t function, uint16_t systemId, uint8_t opcode, uint8_t rs1Num, uint8_t rs2Num, uint8_t xd, RD rd,
           uint8_t xs1, uint8_t xs2, uint8_t coreId, uint64_t rs1, uint64_t rs2);

};

struct composer_pack_info {
  int system_id_bits = -1;
  int core_id_bits = -1;
  composer_pack_info(int system_id_bits, int core_id_bits) {
    this->system_id_bits = system_id_bits;
    this->core_id_bits = core_id_bits;
  }
};

struct rocc_response {
  uint64_t data;
  uint8_t core_id;
  uint8_t system_id;
  uint8_t rd;
private:
  static uint64_t get_mask(int l) {
    uint64_t mask = 0;
    for (int i = 0; i < l; ++i)
      mask = (mask << 1) | 1;
    return mask;
  }
public:
  rocc_response(const uint32_t *buffer, const composer_pack_info &pack_info) {
    this->rd = (uint8_t) buffer[2];
    // system_id, core_id, data packed into buffer 0, data is appended to lower 32-bits in buffer[1]
    int system_id_loc = 32 - pack_info.system_id_bits;
    this->system_id = (buffer[0] >> system_id_loc) & get_mask(pack_info.system_id_bits);
    int core_id_loc = 32 - pack_info.system_id_bits - pack_info.core_id_bits;
    this->core_id = (buffer[0] >> core_id_loc) & get_mask(pack_info.core_id_bits);
    uint64_t data_top = (buffer[0] & get_mask(32-pack_info.core_id_bits - pack_info.system_id_bits)) << 32;
    this->data = data_top | buffer[1];
    printf("buff0: %08x\t buff1: %08x\t buff2: %08x", buffer[0], buffer[1], buffer[2]); fflush(stdout);
  }
};

#endif //ROCC_H
