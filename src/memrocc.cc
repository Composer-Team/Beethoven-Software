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

#include <rocc.h>
#include "butil.h"
#include <memrocc.h>

#define MAX_COMMANDS_STORED (402653184LL)
//12 GB of commands
uint32_t *cmdBuf = (uint32_t *) calloc(MAX_COMMANDS_STORED, 8 * sizeof(uint32_t));
uint64_t numCmd = 0LL;


void encode_cmd_buf_mem(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t core) {
  encode_cmd_buf(funct, 0, core, 0, false, 0, 0, opcode, rs1, rs2, cmdBuf + 8 * numCmd);
  numCmd++;
  assert(numCmd <= MAX_COMMANDS_STORED);
}

void encode_cmd_buf_rd_mem(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                           uint8_t core) {
  encode_cmd_buf(funct, rd, core, 0, false, 0, 0, opcode, rs1, rs2, cmdBuf + 8 * numCmd);
  numCmd++;
  assert(numCmd <= MAX_COMMANDS_STORED);
}

void encode_cmd_buf_xs_mem(fpga_handle_t *mysim, uint8_t opcode, uint8_t funct, uint64_t rs1, uint64_t rs2, uint8_t rd,
                           uint8_t core, bool xs1, bool xs2, bool xd) {
  encode_cmd_buf(funct, rd, core, 0, xd, xs1, xs2, opcode, rs1, rs2, cmdBuf + 8 * numCmd);
  numCmd++;
  assert(numCmd <= MAX_COMMANDS_STORED);
}

void send_cmd_buf(fpga_handle_t *handle, uint64_t addr) {
  transfer_chunk_to_fpga(handle, cmdBuf, addr, numCmd * 32);
#ifndef NDEBUG
  printf("Sending the following commands:\n");
  for (uint64_t i = 0; i < numCmd; i++) {
    decode_cmd_buf(cmdBuf + 8 * i);
  }
  fflush(stdout);
#endif
  numCmd = 0;
  fsync(handle->get_write_fd());
}


static inline void mem_reduce_start(fpga_handle_t *mysim, int id, unsigned long len, int rd) {
  encode_cmd_buf_rd_mem(mysim, CUSTOM_3, 16, 0, len, rd, id);
}

static inline void mem_reduce_set_addr(fpga_handle_t *mysim, int idx, void *addr, uint8_t core) {
  encode_cmd_buf_mem(mysim, CUSTOM_3, 17, idx, (uintptr_t) addr,
                     core);        // 0x7b, 17, column_num, outputAddr, CORE0
}

static inline void mem_reduce_set_comparator(fpga_handle_t *mysim, int op, int value, uint8_t core) {
  encode_cmd_buf_mem(mysim, CUSTOM_3, 18, op, value, core);

}

static inline void mem_reduce_set_op_and_cols(fpga_handle_t *mysim, int op, int num_cols, uint8_t core) {
  encode_cmd_buf_mem(mysim, CUSTOM_3, 19, op, num_cols, core);
}

static inline void mem_reduce_set_len_and_nCores(fpga_handle_t *mysim, int lines, int nCores, uint8_t core) {
  encode_cmd_buf_mem(mysim, CUSTOM_3, 20, lines, nCores, core); // CUSTOM_3, 20, lines, 0, core
}

static inline void exec_cmd(fpga_handle_t *handle, uint64_t addr, uint64_t len) {
  if (len == 0) {
    return;
  }
#ifndef NDEBUG
  auto cmds = (uint32_t *) calloc(len, 32);
  get_chunk_from_fpga(handle, cmds, addr, len * 32);
  fsync(handle->get_read_fd());
  for (uint64_t i = 0; i < len; i++) {
    decode_cmd_buf(cmds + 8 * i);
  }
  free(cmds);
#endif
  encode_cmd_buf_xs(handle, CUSTOM_3, 0, addr, len, 0, 0, false, false, false); // don't expect return
}

static inline void exec_host_cmd(fpga_handle_t *handle, uint64_t addr, uint64_t len) {
  auto cmds = (uint32_t *) calloc(len, 32);
  get_chunk_from_fpga(handle, cmds, addr, len * 32);
  fsync(handle->get_read_fd());
  for (uint64_t i = 0; i < len; i++) {
    send_rocc_cmd(handle, cmds + 8 * i);
  }
  free(cmds);
}