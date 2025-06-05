//
// Created by Chris Kjellqvist on 8/9/23.
//

#ifndef BEETHOVENRUNTIME_MEM_CTRL_H
#define BEETHOVENRUNTIME_MEM_CTRL_H

#include "beethoven_hardware.h"

#if NUM_DDR_CHANNELS >= 1

#include "data_server.h"
#include "ddr_macros.h"
#include "dram_system.h"
#include <dramsim3.h>
#include <memory>
#include <queue>
#include "sim/axi/address_channel.h"
#include "sim/axi/data_channel.h"
#include "sim/axi/response_channel.h"

extern uint64_t main_time;

#include "sim/axi/vpi_handle.h"

#define RLOCK pthread_mutex_lock(&axi4_mem.read_queue_lock);
#define WLOCK pthread_mutex_lock(&axi4_mem.write_queue_lock);
#define RUNLOCK pthread_mutex_unlock(&axi4_mem.read_queue_lock);
#define WUNLOCK pthread_mutex_unlock(&axi4_mem.write_queue_lock);

extern int axi_ddr_bus_multiplicity;
extern int DDR_ENQUEUE_SIZE_BYTES;
extern int TOTAL_BURST;
extern address_translator at;
extern dramsim3::Config *dramsim3config;

namespace mem_ctrl {
  void init(const std::string &dram_ini_file);

  // = (DATA_BUS_WIDTH / 8) / DDR_BUS_WIDTH_BYTES
  struct memory_transaction {
    uintptr_t addr;
    int size;
    int len;
    int axi_bus_beats_progress;
    int id;
    bool fixed;
    uint64_t fpga_addr;
    int dram_tx_n_enqueues;
    int dram_tx_axi_enqueue_progress = 0;
    int dram_tx_load_progress = 0;
    bool can_be_last = true;

    bool is_intermediate;

    std::vector<bool> ddr_bus_beats_retrieved;

    memory_transaction(uintptr_t addr,
                       int size,
                       int len,
                       int progress,
                       bool fixed,
                       int id,
                       uint64_t fpga_addr,
                       bool is_intermediate) : addr(addr),
                                               size(size),
                                               len(len),
                                               axi_bus_beats_progress(progress),
                                               id(id),
                                               fixed(fixed),
                                               fpga_addr(fpga_addr),
                                               is_intermediate(is_intermediate) {
      dram_tx_n_enqueues = (len * size) / DDR_ENQUEUE_SIZE_BYTES;
      if (!is_intermediate) {
//        printf("Transaction of size %d, len %d, n_enqueues %d\n\n", size, len, dram_tx_n_enqueues);
      }
      if (dram_tx_n_enqueues == 32) {
        printf("");
      }
//      fflush(stdout);
      if (dram_tx_n_enqueues == 0) dram_tx_n_enqueues = 1;
      for (int i = 0; i < dram_tx_n_enqueues * TOTAL_BURST; ++i) {
        ddr_bus_beats_retrieved.emplace_back(false);
      }
    }

    memory_transaction() = delete;

    int dramsim_hasBeatReady() {
      if (axi_bus_beats_progress == axi_bus_beats_length()) return false;
      for (int i = 0; i < axi_ddr_bus_multiplicity; ++i) {
        if (!ddr_bus_beats_retrieved[axi_bus_beats_progress * axi_ddr_bus_multiplicity + i]) return false;
      }
      return ddr_bus_beats_retrieved[axi_bus_beats_progress * axi_ddr_bus_multiplicity];
    }

    [[nodiscard]] int bankId() const {
      return int(fpga_addr >> 12);
    }

    [[nodiscard]] bool dramsim_tx_finished() const {
      return dram_tx_axi_enqueue_progress >= dram_tx_n_enqueues;
    }

    [[nodiscard]] int axi_bus_beats_length() const {
      return len * size / (DATA_BUS_WIDTH / 8);
    }
  };


  struct with_dramsim3_support {

    virtual void enqueue_read(std::shared_ptr<mem_ctrl::memory_transaction> &tx) = 0;

    std::map<uint64_t, std::queue<std::shared_ptr<memory_transaction>> *> in_flight_reads;
    std::map<uint64_t, std::queue<std::shared_ptr<memory_transaction>> *> in_flight_writes;
    dramsim3::JedecDRAMSystem *mem_sys;
    pthread_mutex_t read_queue_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t write_queue_lock = PTHREAD_MUTEX_INITIALIZER;

    const static int max_q_length = 40;
    std::vector<std::shared_ptr<memory_transaction>> ddr_read_q;
    std::vector<std::shared_ptr<memory_transaction>> ddr_write_q;

    //  std::set<int> bank2tx;
    bool can_accept_write() {
      return ddr_write_q.size() < max_q_length;
    }

    bool can_accept_read() {
      return ddr_read_q.size() < max_q_length;
    }

    void init_dramsim3();

    virtual void enqueue_response(int id) = 0;
  };

  template<typename id_t,
          typename axisize_t,
          typename burst_t,
          typename addr_t,
          typename len_t,
          typename strb_t,
          typename byte_t,
          typename data_t>
  struct mem_interface : with_dramsim3_support {
    address_channel<id_t, axisize_t, burst_t, addr_t, len_t, byte_t> aw;
    address_channel<id_t, axisize_t, burst_t, addr_t, len_t, byte_t> ar;
    data_channel<byte_t, strb_t, byte_t, data_t> w;
    data_channel<id_t, byte_t, byte_t, data_t> r;
    response_channel<id_t, byte_t> b;
    std::queue<std::shared_ptr<memory_transaction>> write_transactions;
    std::queue<std::shared_ptr<memory_transaction>> read_transactions;

    ~mem_interface() = default;

    int num_in_flight_writes = 0;
    static const int max_in_flight_writes = 32;
    int id;

    void enqueue_read(std::shared_ptr<mem_ctrl::memory_transaction> &tx) override {
      read_transactions.push(tx);
    }

    void enqueue_response(int id) override {
      b.to_enqueue.push(id);
    }
  };

}

#define prep(x) std::decay_t<decltype(x)>

#ifdef VERILATOR
#include "BeethovenTop.h"
#include "sim/DataWrapper.h"

using mem_intf_t = mem_ctrl::mem_interface<
        GetSetWrapper<prep(BeethovenTop::M00_AXI_arid)>,
        GetSetWrapper<prep(BeethovenTop::M00_AXI_arsize)>,
        GetSetWrapper<prep(BeethovenTop::M00_AXI_arburst)>,
        GetSetWrapper<prep(BeethovenTop::M00_AXI_araddr)>,
        GetSetWrapper<prep(BeethovenTop::M00_AXI_arlen)>,
        GetSetWrapper<prep(BeethovenTop::M00_AXI_wstrb)>,
        GetSetWrapper<uint8_t>,
        GetSetDataWrapper<uint8_t, DATA_BUS_WIDTH/8>>;
#else
typedef mem_ctrl::mem_interface<VCSShortHandle,
        VCSShortHandle,
        VCSShortHandle,
        VCSLongHandle,
        VCSShortHandle,
        VCSLongHandle,
        VCSShortHandle,
        VCSLongHandle> mem_intf_t;
#endif

#ifdef BEETHOVEN_HAS_DMA
#ifdef VERILATOR
typedef mem_ctrl::mem_interface<
GetSetWrapper<prep(BeethovenTop::dma_arid)>,
  GetSetWrapper<prep(BeethovenTop::dma_arsize)>,
  GetSetWrapper<prep(BeethovenTop::dma_arburst)>,
  GetSetWrapper<prep(BeethovenTop::dma_araddr)>,
  GetSetWrapper<prep(BeethovenTop::dma_arlen)>,
  GetSetWrapper<prep(BeethovenTop::dma_wstrb)>,
  GetSetWrapper<uint8_t>,
  GetSetDataWrapper<uint8_t, DATA_BUS_WIDTH/8>> dma_intf_t;
#else
typedef mem_ctrl::mem_interface<VCSShortHandle,
        VCSShortHandle,
        VCSShortHandle,
        VCSShortHandle,
        VCSShortHandle,
        VCSLongHandle,
        VCSShortHandle,
        VCSLongHandle> dma_intf_t;
#endif
  extern dma_intf_t dma;
  extern int dma_txprogress;
  extern int dma_txlength;

#endif

void try_to_enqueue_ddr(mem_intf_t &);

#endif
#if NUM_DDR_CHANNELS >= 1
extern mem_intf_t axi4_mems[NUM_DDR_CHANNELS];
#endif
#endif//BEETHOVENRUNTIME_MEM_CTRL_H
