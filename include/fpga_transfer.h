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

#ifndef TESTHELP_INCLUDED
#define TESTHELP_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <csignal>

#include "rocc.h"
#include "fpga_handle.h"

// Macro used for comparing output of unit for testing
#define COMPARE(i, a, b, name) {                                        \
        if (a != b) printf("%s idx %d: expected: %#x, actual: %#x\n", name, i, a, b); } 

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga_ocl(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes);

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes, uint64_t dma_block_size = 16777216);

// move <num_bytes> from the fpga to the sh
void get_chunk_from_fpga_ocl(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes);

void get_chunk_from_fpga(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes);

#endif //TESTHELP_INCLUDED
