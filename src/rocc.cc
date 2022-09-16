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

uint64_t pack(uint32_t hi, uint32_t low) {
  return (((uint64_t) hi) << 32) | ((uint64_t) low);
}

void decode_cmd_buf(uint32_t* buf) {
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
         "rs1=%016llx "
         "rs2=%016llx\n",
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

void encode_cmd_buf(
        uint32_t funct, uint8_t rd, uint8_t rs1_num, uint8_t rs2_num,
        uint8_t xd, uint8_t xs1, uint8_t xs2, uint8_t opcode,
        uint64_t rs1, uint64_t rs2, uint32_t* fpga_buf) {

  memset(fpga_buf, 0x0, 20);
  fpga_buf[4] = rs2 & 0xFFFFFFFF;
  fpga_buf[3] = rs2 >> 32;
  fpga_buf[2] = (rs1 & 0xFFFFFFFF);
  fpga_buf[1] = rs1 >> 32;
  // 7 bits
  fpga_buf[0] |= ((opcode & 0x7F));
  // 5 bits
  fpga_buf[0] |= ((rd & 0x1F) << 7);
  // 1 bits
  fpga_buf[0] |= ((xs2 & 0x1) << 12);
  // 1 bits
  fpga_buf[0] |= ((xs1 & 0x1) << 13);
  // 1 bit
  fpga_buf[0] |= ((xd & 0x1) << 14);
  // 5 bits
  fpga_buf[0] |= ((rs1_num & 0x1F) << 15);
  // 5 bits
  fpga_buf[0] |= ((rs2_num & 0x1F) << 20);
  // 7 bits
  fpga_buf[0] |= ((funct & 0x7F) << 25);
  // 7 + 5 + 1 + 1 + 1 + 5 + 5 + 7 = 32bit
}

void send_rocc_cmd(fpga_handle_t* mysim, uint32_t* data) {
  printf("Sending the following commands:\n");
  decode_cmd_buf(data);
  fflush(stdout);
  for (int i = 0; i < 5; i++) {
    while (!mysim->read(CMD_READY)) {}
    mysim->write(CMD_BITS, data[i]);
    mysim->write(CMD_VALID, 0x1);
  }
}

uint64_t get_rocc_resp(fpga_handle_t* mysim) {
  uint64_t retval;
  while (!mysim->read(RESP_VALID)) {}
  uint32_t resp_val1 = mysim->read(RESP_BITS);    // id
  retval = (uint64_t) resp_val1 << 32;
  mysim->write(RESP_READY, 0x1);
  while (!mysim->read(RESP_VALID)) {}
  uint32_t resp_val2 = mysim->read(RESP_BITS);    // len / error
  retval |= resp_val2;
  mysim->write(RESP_READY, 0x1);
  uint32_t rd;
  while (!mysim->read(RESP_VALID)) {}
  rd = mysim->read(RESP_BITS);
  mysim->write(RESP_READY, 0x1);
  mysim->store_resp(rd, resp_val1, resp_val2);
  return retval;
}

std::pair<uint32_t, uint32_t> get_id_retval(fpga_handle_t* mysim) {
  uint64_t resp = get_rocc_resp(mysim);
  uint32_t id = resp >> 32;
  uint32_t retval = resp & 0xffffffff;
  return std::make_pair(id, retval);
};

void encode_cmd_buf_simple(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t core) {
  uint32_t inputBuf[5];
  encode_cmd_buf(funct, 0, core, 0, true, 0, 0, opcode, rs1, rs2, inputBuf);
  send_rocc_cmd(mysim, inputBuf);
}

void encode_cmd_buf_rd(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                       uint8_t core) {
  uint32_t inputBuf[5];
  encode_cmd_buf(funct, rd, core, 0, true, 0, 0, opcode, rs1, rs2, inputBuf);
  send_rocc_cmd(mysim, inputBuf);
}

void encode_cmd_buf_xs(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                       uint8_t core, bool xs1, bool xs2, bool xd) {
  uint32_t inputBuf[5];
  encode_cmd_buf(funct, rd, core, 0, xd, xs1, xs2, opcode, rs1, rs2, inputBuf);
  send_rocc_cmd(mysim, inputBuf);
}

uint32_t* gen_cmd_buf_rd(uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd) {
  auto inputBuf = (uint32_t*) calloc(5, sizeof(uint32_t));
  encode_cmd_buf(funct, rd, 0, 0, 0, 1, 1, opcode, rs1, rs2, inputBuf);
  return inputBuf;
}

static inline uint64_t flush(fpga_handle_t* mysim) {
  encode_cmd_buf_simple(mysim, CUSTOM_0, 0, 0, 0, 0);
  return get_rocc_resp(mysim);
}
