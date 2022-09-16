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

#pragma once

#include <zconf.h>
#include "fpga_handle.h"

#define SIZE_8 0
#define SIZE_16 1
#define SIZE_32 2
#define SIZE_64 3
#define SIZE_128 4
#define SIZE_256 5

#define CUSTOM_3 0x7b
#define CUSTOM_0 0xb

#define ACC_SLOT 3
#define MSG_SIZE 80
#define RESP_BITS 0 //READONLY
#define RESP_VALID 1 //READONLY
#define RESP_READY 2 //WRITEONLY

#define CMD_BITS 3 //WRITEONLY
#define CMD_VALID 4 //WRITEONLY
#define CMD_READY 5 //READONLY

#define OPCODE_REDUCER (2)
#define OPCODE_GEMM (2)

uint64_t pack(uint32_t hi, uint32_t low);

void decode_cmd_buf(uint32_t *buf);

void encode_cmd_buf(uint32_t funct, uint8_t rd, uint8_t rs1_num, uint8_t rs2_num, uint8_t xd, uint8_t xs1, uint8_t xs2,
                    uint8_t opcode, uint64_t rs1, uint64_t rs2, uint32_t *fpga_buf);

void send_rocc_cmd(fpga_handle_t *mysim, uint32_t *data);

uint64_t get_rocc_resp(fpga_handle_t *mysim);

std::pair<uint32_t, uint32_t> get_id_retval(fpga_handle_t *mysim);
void encode_cmd_buf_simple(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t core);

void encode_cmd_buf_rd(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                       uint8_t core);

void encode_cmd_buf_xs(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                       uint8_t core, bool xs1, bool xs2, bool xd);

uint32_t *gen_cmd_buf_rd(uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd);

static inline uint64_t flush(fpga_handle_t *mysim);
