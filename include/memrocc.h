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


#include "rocc.h"
#include "butil.h"

#pragma once

#define MAX_COMMANDS_STORED (402653184LL)
//12 GB of commands
extern uint32_t* cmdBuf;
extern uint64_t numCmd;


void encode_cmd_buf_mem(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t core);
void encode_cmd_buf_rd_mem(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd, uint8_t core);

void encode_cmd_buf_xs_mem(fpga_handle_t* mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                           uint8_t core, bool xs1, bool xs2, bool xd);
void send_cmd_buf(fpga_handle_t* handle, uint64_t addr);


static inline void mem_reduce_start(fpga_handle_t* mysim, int id, unsigned long len, int rd);
static inline void mem_reduce_set_addr(fpga_handle_t* mysim, int idx, void* addr, uint8_t core);
static inline void mem_reduce_set_comparator(fpga_handle_t* mysim, int op, int value, uint8_t core);
static inline void mem_reduce_set_op_and_cols(fpga_handle_t* mysim, int op, int num_cols, uint8_t core);
static inline void mem_reduce_set_len_and_nCores(fpga_handle_t* mysim, int lines, int nCores, uint8_t core);

static inline void exec_cmd(fpga_handle_t* handle, uint64_t addr, uint64_t len);
static inline void exec_host_cmd(fpga_handle_t* handle, uint64_t addr, uint64_t len);
