//
// Created by Christopher Kjellqvist on 8/5/24.
//
#include "sim/tick.h"
#include "beethoven_hardware.h"
#include "sim/mem_ctrl.h"

#ifdef VERILATOR
#ifdef USE_VCD
#include <verilated_vcd_c.h>
extern VerilatedVcdC *tfp;
#else
#include <verilated_fst_c.h>
extern VerilatedFstC *tfp;
#endif
#endif

float ddr_acc = 0;
int strobe_width;
extern float ddr_clock_inc;
extern uint64_t memory_transacted;
int dma_wait = 50;
int id1, id2;
#if NUM_DDR_CHANNELS >=1
extern mem_intf_t axi4_mems[NUM_DDR_CHANNELS];
#endif

void tick_signals(ControlIntf *ctrl) {

// ------------ HANDLE COMMAND INTERFACE ----------------
// start queueing up a new command if one is available

  ctrl->tick();

#if NUM_DDR_CHANNELS >= 1
// ------------ HANDLE MEMORY INTERFACES ----------------
  // approx clock diff
  ddr_acc += ddr_clock_inc;
  while (ddr_acc >= 1) {
    for (auto &axi4_mem: axi4_mems) {
      axi4_mem.mem_sys->ClockTick();
      try_to_enqueue_ddr(axi4_mem);
    }
    ddr_acc -= 1;
  }


  for (auto &axi4_mem: axi4_mems) {
    if (axi4_mem.r.getValid() && axi4_mem.r.getReady()) {
      memory_transacted += (DATA_BUS_WIDTH >> 3);
      RLOCK
      auto tx = axi4_mem.read_transactions.front();
      tx->axi_bus_beats_progress++;
      if (not tx->fixed) {
        tx->addr += tx->size;
      }
      if (axi4_mem.r.getLast() || !tx->can_be_last) {
        axi4_mem.read_transactions.pop();
      }
      RUNLOCK
    }

    if (axi4_mem.ar.getReady() && axi4_mem.ar.getValid()) {
      uint64_t addr = axi4_mem.ar.getAddr();
      char *ad = (char *) at.translate(addr);
      if (ad == nullptr) return;
      auto txsize = (int) 1 << axi4_mem.ar.getSize();
      auto txlen = (int) (axi4_mem.ar.getLen()) + 1;
      auto tx = std::make_shared<mem_ctrl::memory_transaction>((uintptr_t) ad, txsize, txlen, 0, false,
                                                               axi4_mem.ar.getId(), addr, false);
      RLOCK
      axi4_mem.ddr_read_q.push_back(tx);
      RUNLOCK
    }
  }

  for (mem_intf_t &axi4_mem: axi4_mems) {
    if (axi4_mem.b.getReady() && axi4_mem.b.getValid()) {
      axi4_mem.b.send_ids.pop();
      axi4_mem.num_in_flight_writes--;
    }
    RLOCK
    if (not axi4_mem.read_transactions.empty()) {
#if DATA_BUS_WIDTH < 32
#error "Handling the data bus gets much more difficult with tiny data buses so the simulator doesn't account for it. Let me know if you _need_ this."
#endif
      auto tx = axi4_mem.read_transactions.front();
      int start = ((tx->axi_bus_beats_progress * tx->size) % (DATA_BUS_WIDTH / 8))/4;
      uint32_t *src = reinterpret_cast<uint32_t*>(tx->addr);
      for (int i = 0; i < tx->size / 4; ++i) {
        axi4_mem.r.setData(src[i], i+start);
      }
      bool am_done = tx->len == (tx->axi_bus_beats_progress + 1);
      axi4_mem.r.setValid(1);
      axi4_mem.r.setLast(am_done && tx->can_be_last);
      axi4_mem.r.setId(tx->id);
    } else {
      axi4_mem.r.setValid(0);
      axi4_mem.r.setLast(false);
    }
    RUNLOCK

    WLOCK
    // update all channels with new information now that we've updated states
    if (not axi4_mem.b.send_ids.empty()) {
      axi4_mem.b.setValid(1);
      axi4_mem.b.setId(axi4_mem.b.send_ids.front());
    } else {
      axi4_mem.b.setValid(0);
    }

    if (!axi4_mem.b.to_enqueue.empty()) {
      axi4_mem.b.send_ids.push(axi4_mem.b.to_enqueue.front());
      axi4_mem.b.to_enqueue.pop();
    }

    if (axi4_mem.aw.getValid() && axi4_mem.aw.getReady()) {
      try {
#ifdef BAREMETAL_RUNTIME
        uintptr_t addr = axi4_mem.aw.getAddr();
#else
        char *addr = (char *) at.translate(axi4_mem.aw.getAddr());
        if (addr == nullptr) return;
#endif
        int sz = 1 << axi4_mem.aw.getSize();
        int len = 1 + int(axi4_mem.aw.getLen());// per axi
        bool is_fixed = axi4_mem.aw.getBurst() == 0;
        int id = axi4_mem.aw.getId();
        uint64_t fpga_addr = axi4_mem.aw.getAddr();
        auto tx = std::make_shared<mem_ctrl::memory_transaction>(uintptr_t(addr), sz, len, 0, is_fixed, id,
                                                                 fpga_addr, false);
        axi4_mem.write_transactions.push(tx);
        axi4_mem.num_in_flight_writes++;
      } catch (std::exception &e) {
#ifdef VERILATOR
        tfp->dump(main_time);
        tfp->close();
        throw e;
#endif
      }
    }

    if (not axi4_mem.write_transactions.empty()) {
      if (axi4_mem.w.getValid() && axi4_mem.w.getReady()) {
        memory_transacted += (DATA_BUS_WIDTH >> 3);
        auto trans = axi4_mem.write_transactions.front();
        // refer to https://developer.arm.com/documentation/ihi0022/e/AMBA-AXI3-and-AXI4-Protocol-Specification/Single-Interface-Requirements/Transaction-structure/Data-read-and-write-structure?lang=en#CIHIJFAF
        int off = 0;
        auto addr = trans->addr;
        auto data = axi4_mem.w.getData();
        while (off < DATA_BUS_WIDTH / 8) {
          if (axi4_mem.w.getStrb(off)) {
            reinterpret_cast<uint8_t *>(addr)[off] = data.get()[off];
            memory_transacted++;
          }
          off += 1;
        }
        trans->axi_bus_beats_progress++;

        if (not trans->fixed) {
          trans->addr += trans->size;
        }

        if (axi4_mem.w.getLast()) {
          axi4_mem.write_transactions.pop();
          trans->dram_tx_axi_enqueue_progress = 0;
          trans->axi_bus_beats_progress = 1;
          axi4_mem.ddr_write_q.push_back(trans);
          axi4_mem.w.setReady(!axi4_mem.write_transactions.empty() ||
                              axi4_mem.num_in_flight_writes < mem_intf_t::max_in_flight_writes);
        } else {
          axi4_mem.w.setReady(1);
        }
      }
    } else {
      axi4_mem.w.setReady(axi4_mem.num_in_flight_writes < mem_intf_t::max_in_flight_writes);
    }
    WUNLOCK

    RLOCK
    // to signify that the AXI4 DDR Controller is busy enqueueing another transaction in the DRAM
    axi4_mem.ar.setReady(axi4_mem.can_accept_read());
    RUNLOCK

    WLOCK

    axi4_mem.aw.setReady(axi4_mem.num_in_flight_writes < mem_intf_t::max_in_flight_writes);
    WUNLOCK
  }

#ifdef BEETHOVEN_HAS_DMA
  pthread_mutex_lock(&dma_lock);
  // enqueue dma transaction into dma axi interface
  dma.aw.setValid(0);
  dma.ar.setValid(0);
  dma.b.setReady(0);
  dma.w.setValid(0);
  dma.r.setReady(0);
  if (dma_valid && not dma_in_progress) {
    dma_txprogress = 0;
    dma_txlength = int(dma_len >> 6);
    if (dma_write) {
      dma.aw.setValid(1);
      dma.aw.setAddr(dma_fpga_addr);
      dma.aw.setId(id1 = rand() % 16);
      dma.aw.setSize(6);
      dma.aw.setLen((dma_len / 64) - 1);
//      std::cout << "STARTING TXLEN: " << std::hex << (dma_len/64 - 1) << std::endl;
      dma.aw.setBurst(1);
      if (dma.aw.fire()) {
        dma_in_progress = true;
      }
    } else {
      dma.ar.setValid(1);
      dma.ar.setAddr(dma_fpga_addr);
      dma.ar.setId(id2 = rand() % 16);
      dma.ar.setSize(6);
      dma.ar.setLen((dma_len / 64) - 1);
      dma.ar.setBurst(1);
      if (dma.ar.fire()) {
        dma_in_progress = true;
      }
    }
  }

  if (dma_in_progress) {
    if (dma_write) {
      if (dma_txprogress < dma_txlength) {
        dma.w.setValid(1);
        for (int i = 0; i < DATA_BUS_WIDTH / 32; ++i) {
          auto data = *((uint32_t*)(dma_ptr)+i+(DATA_BUS_WIDTH/32)*dma_txprogress);
          dma.w.setData(data, i);
        }
        dma.w.setLast(dma_txprogress + 1 == dma_txlength);
//            dma.w.setStrobe(0xFFFFFFFFFFFFFFFFL);
        if (dma.w.getReady()) {
          dma_txprogress++;
        }
      } else {
        dma.b.setReady(1);
        if (dma.b.fire()) {
          if (dma.b.getId() != id1) {
            printf("Huh! %d != %d\n", dma.b.getId(), id1);
          }
          dma_valid = false;
          dma_in_progress = false;
          pthread_mutex_unlock(&dma_wait_lock);
        }
      }
    } else {
      dma.r.setReady(1);
      if (dma.r.fire()) {
        auto rval = dma.r.getData();
        unsigned char *src_val = dma_ptr + 64 * dma_txprogress;
        for (int i = 0; i < 64; ++i) {
          if (rval[i] != src_val[i]) {
            int q = src_val[i];
            std::cerr << "Got an unexpected value from the DMA... " << std::hex << i << " " << dma_txprogress << " " << rval[i]
                      << " " << std::hex << q << std::endl;
            exit(1);
          }
        }
        dma_txprogress++;
        if (dma_txprogress == dma_txlength) {
          dma_valid = false;
          dma_in_progress = false;
          pthread_mutex_unlock(&dma_wait_lock);
        }
      }
    }
  }
  pthread_mutex_unlock(&dma_lock);
#endif
#endif
}
