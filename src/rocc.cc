/*
 * Copyright (c) 2022,
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

#pragma once

#include <zconf.h>
#include <rocc.h>
#include <composer_util.h>

void rocc_cmd::decode() const {
  uint32_t funct = (buf[0] >> 25) & 0x7f;
  uint8_t rd = (buf[0] >> 7) & 0x1f;
  uint8_t rs1_num = (buf[0] >> 15) & 0x1f;
  uint8_t rs2_num = (buf[0] >> 20) & 0x1f;
  uint8_t xd = (buf[0] >> 14) & 0x1;
  uint8_t xs1 = (buf[0] >> 13) & 0x1;
  uint8_t xs2 = (buf[0] >> 12) & 0x1;
  uint8_t opcode = (buf[0] >> 0) & 0x7f;
  uint64_t rs2 = pack(buf[3], buf[4]);
  uint64_t rs1 = pack(buf[1], buf[2]);
  printf("funct=%02x "
         "rd=%02x "
         "rs1_num=%02x "
         "rs2_num=%02x "
         "xd=%01x "
         "xs1=%01x "
         "xs2=%01x "
         "opcode=%02x "
         "rs1=%016lx "
         "rs2=%016lx\n",
         funct,
         rd,
         rs1_num,
         rs2_num,
         xd,
         xs1,
         xs2,
         opcode,
         rs1,
         rs2);
}

rocc_cmd::rocc_cmd(uint16_t function,
                   uint16_t system_id,
                   uint8_t opcode,
                   uint8_t rs1_num,
                   uint8_t rs2_num,
                   uint8_t xd,
                   RD rd,
                   uint8_t xs1,
                   uint8_t xs2,
                   uint64_t rs1,
                   uint64_t rs2) {
  memset(buf, 0, sizeof(int32_t) * 5);
  // TODO check that there are no overflows for the provided values (not outside logical range)
  buf[4] = rs2 & 0xFFFFFFFF;
  buf[3] = rs2 >> 32;
  buf[2] = (rs1 & 0xFFFFFFFF);
  buf[1] = rs1 >> 32;
  // 7 bits
  buf[0] |= opcode & 0x7F;
  // 5 bits
  buf[0] |= (((uint8_t) rd & 0x1F) << 7);
  // 1 bits
  buf[0] |= ((xs2 & 0x1) << 12);
  // 1 bits
  buf[0] |= ((xs1 & 0x1) << 13);
  // 1 bit
  buf[0] |= ((xd & 0x1) << 14);
  // 5 bits
  buf[0] |= ((rs1_num & 0x1F) << 15);
  // 5 bits
  buf[0] |= ((rs2_num & 0x1F) << 20);
  // 7 bits
  uint32_t funct = (system_id << 3) | (function & 0x7);
  buf[0] |= ((funct & 0x7F) << 25);
  // 7 + 5 + 1 + 1 + 1 + 5 + 5 + 7 = 32bit
}