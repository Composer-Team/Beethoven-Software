//
// Created by Chris Kjellqvist on 9/28/22.
//

#ifndef BEETHOVEN_VERILATOR_DATA_SERVER_H
#define BEETHOVEN_VERILATOR_DATA_SERVER_H

#include <cmath>
#include <beethoven_hardware.h>
#include <queue>
#include <set>
#include "util.h"
#include <beethoven/verilator_server.h>

#if defined(SIM)
#if defined(BEETHOVEN_HAS_DMA)
#include <pthread.h>
extern pthread_mutex_t dma_lock;
extern pthread_mutex_t dma_wait_lock;
extern bool dma_valid;
extern unsigned char *dma_ptr;
extern uint64_t dma_fpga_addr;
extern size_t dma_len;
extern bool dma_write;
extern bool dma_in_progress;
#endif

#endif

struct data_server {
  static void start();
  ~data_server();
};

extern beethoven::data_server_file *dsf;

struct address_translator {
  struct addr_pair {
    uint64_t fpga_addr;
    uint64_t mapping_length;
    void *cpu_addr;

    explicit addr_pair(uint64_t fpgaAddr, void *cpuAddr, uint64_t map_length) : fpga_addr(fpgaAddr), cpu_addr(cpuAddr), mapping_length(map_length) {}

    bool operator<(const addr_pair &other) const {
      return fpga_addr < other.fpga_addr;
    }
  };
  std::set<addr_pair> mappings;

  [[nodiscard]] void *translate(uint64_t fp_addr) const;
  [[nodiscard]] std::pair<void *, uint64_t> get_mapping(uint64_t fpga_addr) const;
  void add_mapping(uint64_t fpga_addr, uint64_t mapping_length, void *cpu_addr);
  void remove_mapping(uint64_t fpga_addr);
};


extern address_translator at;


#endif//BEETHOVEN_VERILATOR_DATA_SERVER_H
