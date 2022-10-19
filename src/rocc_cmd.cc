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
#include "composer/rocc_cmd.h"
#include "composer/alloc.h"
#include "composer/util.h"
#include <iostream>
#include <cstring>
#include "composer/fpga_handle.h"

using namespace composer;

uint32_t *rocc_cmd::pack() const {
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

rocc_cmd rocc_cmd::addr_cmd(uint16_t system_id, uint8_t core_id, uint8_t channel_id, const remote_ptr &ptr) {
  return {ROCC_FUNC_ADDR, system_id, ROCC_OP_ACCEL, 0, 0,
          0, RD::R0, 0, 0,
          core_id, (ptr.getLen() << 8) | channel_id, ptr.getFpgaAddr()};
}

rocc_cmd
rocc_cmd::start_cmd(uint16_t system_id, uint8_t rs1_num, uint8_t rs2_num, bool expect_response, RD rd, uint8_t xs1,
                    uint8_t xs2,
                    uint8_t core_id, uint64_t rs1, uint64_t rs2) {
  return {ROCC_FUNC_START, system_id, ROCC_OP_ACCEL, rs1_num, rs2_num,
          expect_response, rd, xs1, xs2,
          core_id, rs1, rs2};
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


uint16_t rocc_cmd::getFunction() const {
  return function;
}

uint16_t rocc_cmd::getSystemId() const {
  return system_id;
}

uint8_t rocc_cmd::getOpcode() const {
  return opcode;
}

uint8_t rocc_cmd::getRs1Num() const {
  return rs1_num;
}

uint8_t rocc_cmd::getRs2Num() const {
  return rs2_num;
}

uint8_t rocc_cmd::getXd() const {
  return xd;
}

RD rocc_cmd::getRd() const {
  return rd;
}

uint8_t rocc_cmd::getXs1() const {
  return xs1;
}

uint8_t rocc_cmd::getXs2() const {
  return xs2;
}

uint8_t rocc_cmd::getCoreId() const {
  return core_id;
}

uint64_t rocc_cmd::getRs1() const {
  return rs1;
}

uint64_t rocc_cmd::getRs2() const {
  return rs2;
}

std::ostream &operator<<(std::ostream &os, const rocc_cmd &cmd) {
  os << "function: " << cmd.getFunction() << " system_id: " << cmd.getSystemId() << " opcode: " << cmd.getOpcode()
     << " rs1_num: "
     << cmd.getRs1Num() << " rs2_num: " << cmd.getRs2Num() << " xd: " << cmd.getXd() << " rd: " << cmd.getRd()
     << " xs1: " << cmd.getXs1()
     << " xs2: " << cmd.getXs2() << " core_id: " << cmd.getCoreId() << " rs1: " << cmd.getRs1() << " rs2: "
     << cmd.getRs2();
  return os;
}

response_handle rocc_cmd::send() const {
  auto ctx = current_handle_context;
  if (ctx == nullptr) {
    switch (active_fpga_handles.size()) {
      case 1:
        ctx = active_fpga_handles[0];
        break;
      case 0:
        std::cerr << "Error: Attempting to perform an implicit send without having either declared a fpga_handle_t"
                     " or set it manually with set_fpga_context()" << std::endl;
        exit(1);
      default:
        std::cerr << "Error: Attempting to perform an implicit send after having declared multiple fpga_handle_t."
                     " When there are multiple handles, the active one must be declared using set_fpga_context or"
                     " perform the selection manually using the fpga_handle_t.send interface." << std::endl;
        break;
    }
  }
  return ctx->send(*this);
}