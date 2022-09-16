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

#define NUM_ITERATIONS 12
#define N_COLS_TO_PASS 1
#define NUM_BUFFERS 2

#define MAX_KEY_BYTES 8
#define MAX_COL_BYTES 32

#define MAX_COLUMNS 3
#define MAX_PART_KEYS 1
#define MAX_SORT_KEYS 1

#define NUM_PARTERS 1
#define MAX_PARTITIONS 8
#define MAX_SPLITTERS 7
#define MAX_PART_ELEMS 1024
#define MAX_FINAL_PART_ELEMS 8192

#define NUM_SORTERS 7
#define MAX_SORT_ELEMS 1024

#define MAX_AGGERS 7
#define MAX_AGG_ELEMS 1024
#define NUM_AGG_GROUP 1
#define NUM_AGG_KEY 1
#define MAX_AGG_AUX 2

#define NUM_REDUCERS 1
#define NUM_REDUCE_COL 1

#define INIT_ADDR 0x0
#define BUFFER_SIZE 250
#define BITS_PER_CACHE_LINE 1024
#define BITS_PER_BYTE 8

#define DMA_BLOCK_SIZE 16777216

#define WRITE_BITS 6 //WRITEONLY
#define WRITE_VALID 7 //WRITEONLY
#define WRITE_READY 8 //READONLY

#define READ_ADDR_BITS 9 //WRITEONLY
#define READ_ADDR_VALID 10 //WRITEONLY
#define READ_ADDR_READY 11 //READONLY

#define READ_BITS 12 //READONLY
#define READ_VALID 13 //READONLY
#define READ_READY 14 //WRITEONLY

// Macro used for comparing output of unit for testing
#define COMPARE(i, a, b, name) {                                        \
        if (a != b) printf("%s idx %d: expected: %#x, actual: %#x\n", name, i, a, b); } 

// calculates how many cache lines it takes to store
// <count> elements with <size> bytes each
uint64_t cacheLines(uint64_t size, uint64_t count);

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga_ocl(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes);

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes, uint64_t dma_block_size = 16777216);

// move <num_bytes> from the fpga to the sh
void get_chunk_from_fpga_ocl(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes);
void get_chunk_from_fpga(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes);

void dataTransfer(fpga_handle_t* mysim, int num_cols, uint32_t** data, uint64_t* addrs, uint64_t* sizes);

void waitForResponse(fpga_handle_t* mysim, char* unit_name);

uint64_t calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem);

uint64_t calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem);

#endif //TESTHELP_INCLUDED
