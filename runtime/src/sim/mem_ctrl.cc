//
// Created by Chris Kjellqvist on 8/9/23.
//


#include "sim/mem_ctrl.h"

#if NUM_DDR_CHANNELS >= 1
#ifdef VERILATOR
#include "verilated.h"
#include <verilated_fst_c.h>
#endif


int DDR_BUS_WIDTH_BITS = 64;
int DDR_BUS_WIDTH_BYTES = 8;
int axi_ddr_bus_multiplicity;
int DDR_ENQUEUE_SIZE_BYTES;
int TOTAL_BURST;
int writes_emitted = 0;
int reads_emitted = 0;
dramsim3::Config *dramsim3config = nullptr;

extern uint64_t main_time;
using namespace mem_ctrl;

#if NUM_DDR_CHANNELS >= 1
mem_intf_t axi4_mems[NUM_DDR_CHANNELS];
#endif
#ifdef BEETHOVEN_HAS_DMA
dma_intf_t dma;
int dma_txprogress = 0;
int dma_txlength = 0;
#endif

void with_dramsim3_support::init_dramsim3() {
  mem_sys = new dramsim3::JedecDRAMSystem(
          *dramsim3config, "",
          [this](uint64_t addr) {
            pthread_mutex_lock(&this->read_queue_lock);
            auto tx = in_flight_reads[addr]->front();
            tx->dram_tx_load_progress++;
            for (int i = 0; i < TOTAL_BURST; ++i) {
              tx->ddr_bus_beats_retrieved[int(addr - tx->fpga_addr) / DDR_BUS_WIDTH_BYTES + i] = true;
            }

            while (tx->dramsim_hasBeatReady()) {
              bool done = (tx->axi_bus_beats_progress == tx->axi_bus_beats_length() - 1);
              auto intermediate_tx = std::make_shared<mem_ctrl::memory_transaction>(tx->addr, tx->size, 1, 0, false,
                                                                                    tx->id, 0, true);
              tx->addr += tx->size;
              intermediate_tx->fpga_addr = tx->fpga_addr;
              intermediate_tx->can_be_last = done;
              tx->axi_bus_beats_progress++;
              enqueue_read(intermediate_tx);
            }
            in_flight_reads[addr]->pop();
            pthread_mutex_unlock(&this->read_queue_lock);
          },
          [this](uint64_t addr) {
            pthread_mutex_lock(&write_queue_lock);
            auto tx = in_flight_writes[addr]->front();
            tx->axi_bus_beats_progress--;
            if (tx->axi_bus_beats_progress == 0) {
              enqueue_response(tx->id);
            }
            in_flight_writes[addr]->pop();
            pthread_mutex_unlock(&write_queue_lock);
          });

}

void try_to_enqueue_ddr(mem_intf_t &axi4_mem) {
  RLOCK
  std::shared_ptr<mem_ctrl::memory_transaction> to_enqueue_read = {};
  // find next read we should send to DRAM. Prioritize older txs
  for (auto it = axi4_mem.ddr_read_q.begin(); it != axi4_mem.ddr_read_q.end(); ++it) {
    auto &mt = *it;
    if (axi4_mem.mem_sys->WillAcceptTransaction(mt->fpga_addr, false)) {
      // AXI stipulates that for multiple transactions on the same ID, the returned packets need to be serialized
      // Since this list is ordered old -> new, we need to search from the beginning of the list to the current iterator
      //  to see if there is a transaction with the same ID. If so, we need to skip and issue later.
      bool foundHigherPriorityInID = false;
      for (auto it2 = axi4_mem.ddr_read_q.begin(); it2 != it; ++it2) {
        if (mt->id == (*it2)->id) {
          foundHigherPriorityInID = true;
        }
      }
      if (foundHigherPriorityInID) continue;

      to_enqueue_read = mt;
      auto dimm_addr = to_enqueue_read->fpga_addr
                       + DDR_ENQUEUE_SIZE_BYTES * to_enqueue_read->dram_tx_axi_enqueue_progress;
      if (!axi4_mem.mem_sys->WillAcceptTransaction(dimm_addr, false)) {
        continue;
      }
      axi4_mem.mem_sys->AddTransaction(dimm_addr, false);
      reads_emitted++;

      // remember it as being in flight. Make a queue if necessary and store it there
      if (axi4_mem.in_flight_reads.find(dimm_addr) == axi4_mem.in_flight_reads.end())
        axi4_mem.in_flight_reads[dimm_addr] = new std::queue<std::shared_ptr<mem_ctrl::memory_transaction>>();

      auto &q = *axi4_mem.in_flight_reads[dimm_addr];
      q.push(to_enqueue_read);
      to_enqueue_read->dram_tx_axi_enqueue_progress++;

      if (to_enqueue_read->dramsim_tx_finished()) {
        axi4_mem.ddr_read_q.erase(std::find(axi4_mem.ddr_read_q.begin(), axi4_mem.ddr_read_q.end(), to_enqueue_read));
      }

      break;
    }
  }
  RUNLOCK

  WLOCK
  std::shared_ptr<mem_ctrl::memory_transaction> to_enqueue_write = nullptr;
  for (const auto &mt: axi4_mem.ddr_write_q) {
    if (axi4_mem.mem_sys->WillAcceptTransaction(
            mt->fpga_addr + DDR_ENQUEUE_SIZE_BYTES * mt->dram_tx_axi_enqueue_progress, true)) {
      to_enqueue_write = mt;
      break;
    }
  }

  if (to_enqueue_write != nullptr) {
    auto dimm_addr =
            to_enqueue_write->fpga_addr + to_enqueue_write->dram_tx_axi_enqueue_progress * DDR_ENQUEUE_SIZE_BYTES;
    to_enqueue_write->dram_tx_axi_enqueue_progress++;
    axi4_mem.mem_sys->AddTransaction(dimm_addr, true);
    writes_emitted++;
    if (axi4_mem.in_flight_writes.find(dimm_addr) == axi4_mem.in_flight_writes.end())
      axi4_mem.in_flight_writes[dimm_addr] = new std::queue<std::shared_ptr<mem_ctrl::memory_transaction>>;
    axi4_mem.in_flight_writes[dimm_addr]->push(to_enqueue_write);
    if (to_enqueue_write->dramsim_tx_finished()) {
      axi4_mem.ddr_write_q.erase(std::find(axi4_mem.ddr_write_q.begin(), axi4_mem.ddr_write_q.end(), to_enqueue_write));
    }
//    fprintf(stderr, "Starting write tx %d\n", to_enqueue_write->id);
  }
  WUNLOCK
}

#include "sys/stat.h"

void mem_ctrl::init(const std::string &dram_ini_file) {
  struct stat st = {};
  if (stat(dram_ini_file.c_str(), &st) == -1) {
    std::cout << "Input DDR Config not found: " << dram_ini_file << std::endl;
    exit(-1);
  } else {
    std::cout << "Found DDR" << std::endl;
  }
  dramsim3config = new dramsim3::Config(dram_ini_file, "dramsim3log");
  // KRIA has much slower memory!
  // Config dramsim3config("../DRAMsim3/configs/Kria.ini", "./");
  DDR_BUS_WIDTH_BITS = dramsim3config->bus_width;
  DDR_BUS_WIDTH_BYTES = DDR_BUS_WIDTH_BITS / 8;
  axi_ddr_bus_multiplicity = (DATA_BUS_WIDTH / 8) / DDR_BUS_WIDTH_BYTES;
  TOTAL_BURST = dramsim3config->BL;
  DDR_ENQUEUE_SIZE_BYTES = DDR_BUS_WIDTH_BYTES * TOTAL_BURST;

  if (DDR_BUS_WIDTH_BYTES > (DATA_BUS_WIDTH / 8)) {
    std::cerr << DDR_BUS_WIDTH_BYTES << "</= " << (DATA_BUS_WIDTH / 8) << std::endl;
    std::cerr << "This is an unsupported configuration of DDR and AXI bus.\n"
                 "It's also quite rare to happen as well. The AXI bus is \n"
                 "much more likely to be greater than or equal to the DDR\n"
                 "bus width. This is a limitation of the current impl.\n"
                 "To fix this, select a memory configuration that has a\n"
                 "smaller device bus width or, if you genuinely have a \n"
                 "system with such a configuration, please contact the\n"
                 "maintainer of this project." << std::endl;
    assert(DDR_BUS_WIDTH_BYTES <= (DATA_BUS_WIDTH / 8));
  }
}

#endif
