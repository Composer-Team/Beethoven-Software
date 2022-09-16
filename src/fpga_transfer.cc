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
#include "fpga_transfer.h"
#include <iostream>

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

// calculates how many cache lines it takes to store
// <count> elements with <size> bytes each
uint64_t cacheLines(uint64_t size, uint64_t count) {
  return ((size * count - 1) / (BITS_PER_CACHE_LINE / BITS_PER_BYTE)) + 1;
}

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga_ocl(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes) {
  if (num_bytes == 0) {
    return;
  }

  // calculate the number cache lines
  uint64_t num_blocks = cacheLines(num_bytes, 1);
  uint64_t block_size = (BITS_PER_CACHE_LINE / BITS_PER_BYTE);
  uint64_t axi_size = 32;
  uint64_t axi_per_block = BITS_PER_CACHE_LINE/axi_size;

  // allocate a storage buffer for one cache line
  auto block_buf = (uint32_t*) malloc(block_size);
  // track how many bytes we've sent so we don't seg-fault
  uint64_t bytes_left = num_bytes;

  printf("Sending chunk: %zu bytes to %lx fpga base in %zu blocks\n",
         num_bytes, write_byte_addr, num_blocks);
  for (uint64_t i = 0; i < num_blocks; i++) {

    // send the base address of the write via MMIO registers
    uint64_t write_base_addr = write_byte_addr + i * 128;
    uint32_t addr = write_base_addr & 0xFFFFFFFF;
    uint32_t highaddr = (write_base_addr >> 32) & 0xFFFFFFFF;
    while (!(mysim->read(WRITE_READY))) {}
    mysim->write(WRITE_BITS, highaddr);
    mysim->write(WRITE_VALID, 0x1);
    while (!(mysim->read(WRITE_READY))) {}
    mysim->write(WRITE_BITS, addr);
    mysim->write(WRITE_VALID, 0x1);

    // move data to transfer into storage buffer
    memset(block_buf, 0, block_size);
    if (bytes_left >= block_size) {
      memcpy(block_buf, data + i * axi_per_block, block_size);
    }
      // if less data than one buffer, only copy remaining bytes
    else {
      memcpy(block_buf, data + i * axi_per_block, bytes_left);
    }

    // transfer the data via AXI
    for (uint64_t j = 0; j < axi_per_block; j++) {
      while (!(mysim->read(WRITE_READY))) {}
      mysim->write(WRITE_BITS, block_buf[j]);
      mysim->write(WRITE_VALID, 0x1);
    }

    bytes_left -= block_size;
  }

  free(block_buf);
}

// move <num_bytes> from the sh to the fpga
void transfer_chunk_to_fpga(fpga_handle_t *mysim, uint32_t * data, uint64_t write_byte_addr, uint64_t num_bytes, uint64_t dma_block_size) {
#ifdef SIMULATION_XSIM
  transfer_chunk_to_fpga_ocl(mysim,data,write_byte_addr,num_bytes);
    return;
  //printf("dma transfer only supported on f1 -\n\tuse transfer_chunk_to_fpga_ocl instead\n");
  //assert(false);
#endif

  int xdma_write_fd = mysim->get_write_fd();
  size_t ret;

#ifndef NDEBUG
  std::cout << "dma_block_size : " << dma_block_size/(1024*1024) << " MB\n";
  printf("Sending chunk: %zu bytes to %lx fpga base\n", num_bytes, write_byte_addr);
#endif
  uint64_t b;
  for (b = 0 ; (b+dma_block_size) < num_bytes ; b += dma_block_size) {
    ret = pwrite(xdma_write_fd,          // xdma h2c
                 ((char*) data) + b,     // data buffer to transfer
                 dma_block_size,        // size of transfer
                 write_byte_addr + b); // offset in fpga dram
    if (ret < 0) {
      std::cerr << "Write to xdma h2c failed" << std::endl;
      exit(1);
    }
    fsync(xdma_write_fd);
  }
  if (b < num_bytes) {
    ret = pwrite(xdma_write_fd,          // xdma h2c
                 ((char*) data) + b,     // data buffer to transfer
                 (num_bytes-b),        // size of transfer
                 write_byte_addr + b); // offset in fpga dram
    if (ret < 0) {
      std::cerr << "Write to xdma h2c failed" << std::endl;
      exit(1);
    }
  }
}

// move <num_bytes> from the fpga to the sh
void get_chunk_from_fpga_ocl(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes) {

  if (num_bytes == 0) {
    return;
  }

  // calculate the number cache lines
  uint64_t num_blocks = cacheLines(num_bytes, 1);
  uint64_t block_size = (BITS_PER_CACHE_LINE / BITS_PER_BYTE);
  uint64_t axi_size = 32;
  uint64_t axi_per_block = BITS_PER_CACHE_LINE/axi_size;

  // allocate a storage buffer for one cache line
  uint32_t *block_buf = (uint32_t*) malloc(block_size);
  // track how many bytes we've sent so we don't seg-fault
  uint64_t bytes_left = num_bytes;

  printf("Receiving chunk: %zu bytes from %lx fpga base in %zu blocks\n",
         num_bytes, read_byte_addr, num_blocks);
  for (uint64_t i = 0; i < num_blocks; i++) {

    // send the base address of the read via MMIO registers
    uint64_t read_base_addr = read_byte_addr + i * 128;
    uint32_t addr = read_base_addr & 0xFFFFFFFF;
    uint32_t highaddr = (read_base_addr >> 32) & 0xFFFFFFFF;
    while (!mysim->read(READ_ADDR_READY)) {}
    mysim->write(READ_ADDR_BITS, highaddr);
    mysim->write(READ_ADDR_VALID, 0x1);
    while (!mysim->read(READ_ADDR_READY)) {}
    mysim->write(READ_ADDR_BITS, addr);
    mysim->write(READ_ADDR_VALID, 0x1);

    memset(block_buf, 0, block_size);
    for (uint64_t j = 0; j < axi_per_block; j++) {
      while (!mysim->read(READ_VALID)) {}
      block_buf[j*(axi_size/32)] = mysim->read(READ_BITS);
      mysim->write(READ_READY, 0x1);
    }

    // write storage buffer to outputdata
    if (bytes_left >= block_size) {
      memcpy(outputdata + i * axi_per_block, block_buf, block_size);
    }
      // if less data than one buffer, only copy remaining bytes
    else {
      memcpy(outputdata + i * axi_per_block, block_buf, bytes_left);
    }

    bytes_left -= block_size;
  }

  free(block_buf);
}

void get_chunk_from_fpga(fpga_handle_t *mysim, uint32_t * outputdata, uint64_t read_byte_addr, uint64_t num_bytes) {
  if (!mysim->is_real()) {
    get_chunk_from_fpga_ocl(mysim, outputdata, read_byte_addr, num_bytes);
    return;
  }

  int xdma_read_fd = mysim->get_read_fd();
  int ret;

  printf("Receiving chunk: %zu bytes from %lx fpga base\n", num_bytes, read_byte_addr);
  uint64_t b;
  for (b = 0 ; (b+DMA_BLOCK_SIZE) < num_bytes ; b += DMA_BLOCK_SIZE) {
    ret = pread(xdma_read_fd,            // xdma c2h
                ((char*) outputdata) + b, // destination data buffer
                DMA_BLOCK_SIZE,          // size of transfer
                read_byte_addr + b);    // offset in fpga dram
    if (ret < 0) {
      perror("read from xdma c2h failed\n");
      exit(1);
    }
    fsync(xdma_read_fd);
  }
  if (b < num_bytes) {
    ret = pread(xdma_read_fd,          // xdma c2h
                ((char*) outputdata) + b,     // data buffer to transfer
                (num_bytes-b),        // size of transfer
                read_byte_addr + b); // offset in fpga dram
    if (ret < 0) {
      perror("write to xdma c2h failed");
      exit(1);
    }
  }
  printf("Successfully receieved %u bytes from fpga\n",ret);
}



void dataTransfer(fpga_handle_t* mysim, int num_cols, uint32_t** data, uint64_t* addrs, uint64_t* sizes) {
  printf("\n");
  for (int i = 0 ; i < num_cols; i++) {
    printf("Transferring array %d to FPGA...\n", i);
    transfer_chunk_to_fpga(mysim, data[i], addrs[i], sizes[i]);
  }
  printf("\n");
}

void waitForResponse(fpga_handle_t* mysim, char* unit_name) {
  printf("Waiting for response from %s...\n", unit_name);
  fflush(stdout);
  flush(mysim);
}

void waitForResponseNoPrint(fpga_handle_t* mysim) {
  flush(mysim);
}

uint64_t log2Ceil(uint64_t num) {
  uint64_t power = 1;
  while (power < num) {
    power = power * 2;
  }
  return power;
}

uint64_t calcNextAddr(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
  return prev_addr + num_elems * bytes_per_elem;
}

uint64_t calcNextAddrAligned(uint64_t prev_addr, int num_elems, int bytes_per_elem) {
  uint64_t min_addr = calcNextAddr(prev_addr, num_elems, bytes_per_elem);
  int remain = min_addr % 128;
  return min_addr + 128 - remain;
}
