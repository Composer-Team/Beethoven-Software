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

#include <zconf.h>
#include <composer_consts.h>
#include <fpga_handle.h>

#ifndef ROCC_H
#define ROCC_H

/**
 * RoCC commands have a destination register (rd) that is sent to Composer systems. Composer reserves a number of these
 * registers for AXI-Mem port statistics.
 *
 * The AXI Spec (https://developer.arm.com/documentation/102202/0300/Channel-signals) has 5 ports for memory \n
 * AW - Write address port \n
 * W - Write data port \n
 * B - Write response port \n
 * AR - Read address port \n
 * R - Read response port \n
 *
 * Composer counts the number of responses for each one of these ports for debugging purposes (presumably)
 * Composer also collects the number of cycles spent waiting for read responses(21) and for write responses(22)
 */
enum RD {
  /**
   * General purpose registers
   */
  R0 = 0,
  R1 = 1,
  R2 = 2,
  R3 = 3,
  R4 = 4,
  R5 = 5,
  R6 = 6,
  R7 = 7,
  R8 = 8,
  R9 = 9,
  R10 = 10,
  R11 = 11,
  R12 = 12,
  R13 = 13,
  R14 = 14,
  R15 = 15,
  /**
   * Special Registers
   **/
  AddressReadCnt = 16,
  AddressWriteCnt = 17,
  ReadCnt = 18,
  WriteCnt = 19,
  WriteResponseCnt = 20,
  ReadWait = 21,
  WriteWait = 22,
  /**
   * More general purpose registers
   */
  R23,
  R24,
  R25,
  R26,
  R27,
  R28,
  R29,
  R30,
  R31
};

struct rocc_cmd {
  uint32_t buf[5]{};

  rocc_cmd(uint16_t function,
           uint16_t system_id,
           uint8_t opcode = ROCC_CMD_ACCEL,
           uint8_t rs1_num = 0,
           uint8_t rs2_num = 0,
           uint8_t xd = 0,
           RD rd = R0,
           uint8_t xs1 = 1,
           uint8_t xs2 = 0,
           uint64_t rs1 = 0,
           uint64_t rs2 = 0);

  static rocc_cmd flush() {
    return {0, 0, ROCC_CMD_FLUSH, 0, 0};
  }

  void decode() const;
};

struct rocc_response {
  uint32_t data;
  uint8_t core_id;
  uint8_t system_id;
  uint8_t rd;
};

#endif
