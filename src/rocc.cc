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
#include <rocc.h>
#include <composer_util.h>
#include <iostream>
#include <cstring>


uint32_t *rocc_cmd::pack() const{
  auto buf = new uint32_t[5];

#define CHECK(v, bits) if ((v) >= (1L << (bits))) {std::cerr << #v " out of range (" << (v) << std::endl; exit(1); }

  CHECK(function, 3)
  CHECK(system_id, 4)
  CHECK(rs1_num, 5)
  CHECK(rs2_num, 5)
  CHECK(int(rd), 5)
  CHECK(xs1, 1)
  CHECK(xs2, 1)
  CHECK(rs1, 56)

  // combine core id into rs1 for delivery to core
  uint64_t rs1_shadow = rs1 | long(core_id) << 56;

  memset(buf, 0, sizeof(int32_t) * 5);
  // TODO check that there are no overflows for the provided values (not outside logical range)
  buf[4] = rs2 & 0xFFFFFFFF;
  buf[3] = rs2 >> 32;
  buf[2] = (rs1_shadow & 0xFFFFFFFF);
  buf[1] = rs1_shadow >> 32;
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

  return buf;
}

rocc_cmd rocc_cmd::addr_cmd(uint16_t system_id, uint8_t core_id, uint8_t channel_id, uint64_t addr) {
  return {ROCC_FUNC_ADDR, system_id, ROCC_OP_ACCEL, 0, 0,
          0, RD::R0, 0, 0,
          core_id, channel_id, addr};
}

rocc_cmd
rocc_cmd::start_cmd(uint16_t system_id, uint8_t rs1_num, uint8_t rs2_num, uint8_t xd, RD rd, uint8_t xs1, uint8_t xs2,
                    uint8_t core_id, uint64_t rs1, uint64_t rs2) {
  return {ROCC_FUNC_START, system_id, ROCC_OP_ACCEL, rs1_num, rs2_num, xd, rd, xs1, xs2, core_id, rs1, rs2};
}

rocc_cmd
rocc_cmd::flush_cmd() {
  return {0, 0, ROCC_OP_FLUSH, 0, 0, 0, RD::R0, 0, 0, 0, 0, 0};
}

rocc_cmd::rocc_cmd(uint16_t function, uint16_t systemId, uint8_t opcode, uint8_t rs1Num, uint8_t rs2Num, uint8_t xd,
                   RD rd, uint8_t xs1, uint8_t xs2, uint8_t coreId, uint64_t rs1, uint64_t rs2) : function(function),
                                                                                                  system_id(systemId),
                                                                                                  opcode(opcode),
                                                                                                  rs1_num(rs1Num),
                                                                                                  rs2_num(rs2Num),
                                                                                                  xd(xd), rd(rd),
                                                                                                  xs1(xs1), xs2(xs2),
                                                                                                  core_id(coreId),
                                                                                                  rs1(rs1), rs2(rs2) {}

std::ostream &operator<<(std::ostream &os, const rocc_cmd &cmd) {
  os << "function: " << cmd.function << " system_id: " << cmd.system_id << " opcode: " << cmd.opcode << " rs1_num: "
     << cmd.rs1_num << " rs2_num: " << cmd.rs2_num << " xd: " << cmd.xd << " rd: " << cmd.rd << " xs1: " << cmd.xs1
     << " xs2: " << cmd.xs2 << " core_id: " << cmd.core_id << " rs1: " << cmd.rs1 << " rs2: " << cmd.rs2;
  return os;
}
