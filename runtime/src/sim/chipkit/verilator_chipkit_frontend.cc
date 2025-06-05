#include <iostream>

#include "BeethovenTop.h"
#include "cmd_server.h"
#include <csignal>
#include <pthread.h>
#include <queue>
#include <verilated.h>

#include "sim/front_bus_ctrl_uart.h"
#include "sim/mem_ctrl.h"
#include "sim/verilator.h"
#include "sim/chipkit/state_machine.h"

#include "util.h"
#include <beethoven_allocator_declaration.h>

#include "sim/chipkit/tick.h"
#include "sim/tick.h"
#include "sim/chipkit/util.h"

uint64_t main_time = 0;

bool active_reset = true;

extern std::queue<beethoven::rocc_cmd> cmds;
extern std::unordered_map<system_core_pair, std::queue<int> *> in_flight;

pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;

waveTrace *tfp;

float ddr_clock_inc;
uint64_t memory_transacted = 0;


void sig_handle(int sig) {
  tfp->close();
  fprintf(stderr, "FST written!\n");
  fflush(stderr);
  exit(sig);
}

void tick(BeethovenTop *top) {
  try {
    top->eval();
  } catch (std::exception &e) {
    tfp->dump(main_time);
    tfp->close();
    std::cerr << "Emergency dump!" << std::endl;
    throw e;
  }
}



bool kill_sig = false;

std::vector<char> memory_array;

size_t smallestpow2over(size_t n) {
  size_t p = 1;
  while (p <= n) {
    p <<= 1;
  }
  return p;
}

static char mem_get(uintptr_t addr) {
  // if array is not big enough size it up to the first power of 2 over addr length
  if (addr >= memory_array.size()) {
    memory_array.resize(smallestpow2over(addr + 1));
  }
  return memory_array[addr];
}

static void mem_set(uintptr_t addr, char val) {
  if (addr >= memory_array.size()) {
    memory_array.resize(smallestpow2over(addr + 1));
  }
  memory_array[addr] = val;
}

void run_verilator(const std::string &dram_config_file) {
#if 500000 % FPGA_CLOCK != 0
  fprintf(stderr, "Provided FPGA clock rate (%d MHz) does not evenly divide 500. This may result in some inaccuracies in precise simulation measurements.", FPGA_CLOCK);
#endif

  mem_ctrl::init(dram_config_file);
  std::queue<unsigned char> stdout_strm;
  const float DDR_CLOCK = 1000.0 / dramsim3config->tCK;// NOLINT
  auto fpga_clock_inc = 500000 / FPGA_CLOCK;
  ddr_clock_inc = DDR_CLOCK / FPGA_CLOCK;// NOLINT
  float ddr_acc = 0;

  // 0 is the slowest, 14 is the fastest. For whatever reason, 15 isn't working
  set_baud(14);

  // start servers to communicate with user programs
  const char *v[1] = {""};
  Verilated::commandArgs(0, v);
  BeethovenTop top;
  Verilated::traceEverOn(true);
  tfp = new waveTrace;
  top.trace(tfp, 30);
  tfp->open("trace" TRACE_FILE_ENDING);
  std::cout << "Tracing!" << std::endl;

  for (int i = 0; i < NUM_DDR_CHANNELS; ++i) {
    axi4_mems[i].id = i;
  }

#if defined(BEETHOVEN_HAS_DMA)

  mem_ctrl::mem_interface<BeethovenDMAIDtype> dma;
  int dma_txprogress = 0;
  int dma_txlength = 0;
  dma.aw = new mem_ctrl::v_address_channel<BeethovenDMAIDtype>(top.dma_awready, top.dma_awvalid, top.dma_awid,
                                                              top.dma_awsize, top.dma_awburst,
                                                              top.dma_awaddr, top.dma_awlen);
  dma.ar = new mem_ctrl::v_address_channel<BeethovenDMAIDtype>(top.dma_arready, top.dma_arvalid, top.dma_arid,
                                                              top.dma_arsize, top.dma_arburst,
                                                              top.dma_araddr, top.dma_arlen);
  dma.w = new mem_ctrl::data_channel<BeethovenDMAIDtype>(top.dma_wready, top.dma_wvalid,
                                                        &top.dma_wstrb, top.dma_wlast, nullptr);
  dma.r = new mem_ctrl::data_channel<BeethovenDMAIDtype>(top.dma_rready, top.dma_rvalid,
                                                        nullptr, top.dma_rlast, &top.dma_rid);
  dma.w->setData((char *) top.dma_rdata.m_storage);
  dma.r->setData((char *) top.dma_wdata.m_storage);
  dma.b = new mem_ctrl::response_channel<BeethovenDMAIDtype>(top.dma_bready, top.dma_bvalid, top.dma_bid);
#endif
  for (auto &axi4_mem: axi4_mems) {
    axi4_mem.init_dramsim3();
  }

  uint8_t dummy;
  axi4_mems[0].ar.init(GetSetWrapper(top.M00_AXI_arready),
                       GetSetWrapper(top.M00_AXI_arvalid),
                       GetSetWrapper(top.M00_AXI_arid),
                       GetSetWrapper(top.M00_AXI_arsize),
                       GetSetWrapper(top.M00_AXI_arburst),
                       GetSetWrapper(top.M00_AXI_araddr),
                       GetSetWrapper(top.M00_AXI_arlen));
  axi4_mems[0].aw.init(GetSetWrapper(top.M00_AXI_awready),
                       GetSetWrapper(top.M00_AXI_awvalid),
                       GetSetWrapper(top.M00_AXI_awid),
                       GetSetWrapper(top.M00_AXI_awsize),
                       GetSetWrapper(top.M00_AXI_awburst),
                       GetSetWrapper(top.M00_AXI_awaddr),
                       GetSetWrapper(top.M00_AXI_awlen));
  axi4_mems[0].w.init(GetSetWrapper(top.M00_AXI_wready),
                      GetSetWrapper(top.M00_AXI_wvalid),
                      GetSetWrapper(top.M00_AXI_wlast),
                      GetSetWrapper(dummy),
                      GetSetWrapper(top.M00_AXI_wstrb),
                      GetSetDataWrapper<uint8_t, DATA_BUS_WIDTH / 8>((uint8_t * ) & top.M00_AXI_wdata));
  axi4_mems[0].r.init(GetSetWrapper(top.M00_AXI_rready),
                      GetSetWrapper(top.M00_AXI_rvalid),
                      GetSetWrapper(top.M00_AXI_rlast),
                      GetSetWrapper(top.M00_AXI_rid),
                      GetSetWrapper(dummy),
                      GetSetDataWrapper<uint8_t, DATA_BUS_WIDTH / 8>((uint8_t * ) & top.M00_AXI_rdata));
  axi4_mems[0].b.init(GetSetWrapper(top.M00_AXI_bready),
                      GetSetWrapper(top.M00_AXI_bvalid),
                      GetSetWrapper(top.M00_AXI_bid));
  // reset circuit
  top.reset = active_reset;

  ChipkitControlIntf<GetSetWrapper<uint8_t>> uart_stdfront(
          GetSetWrapper<uint8_t>(top.STDUART_uart_txd),
          GetSetWrapper<uint8_t>(top.STDUART_uart_rxd));
  ChipkitControlIntf<GetSetWrapper<uint8_t>> uart_chipfront(
          GetSetWrapper<uint8_t>(top.CHIP_UART_M_TXD),
          GetSetWrapper<uint8_t>(top.CHIP_UART_M_RXD));

  top.CHIP_UART_M_RXD = 1;
  top.CHIP_UART_M_CTS = 0; // clear to send
  // DO NOT MESS WITH THIS
  top.CHIP_UART_M_BAUD_SEL = get_baud_sel();
  top.CHIP_FESEL = 0; // uart
  // disable scan
  top.CHIP_SCEN = 0;
  top.CHIP_SCLK1 = top.CHIP_SCLK2 = top.CHIP_SHIFTIN = top.CHIP_SHIFTOUT = 0;

  for (auto &mem: axi4_mems) {
    mem.r.setValid(0);
    mem.b.setValid(0);
    mem.aw.setReady(1);
  }
#ifdef BEETHOVEN_HAS_DMA
  dma.b->setReady(0);
  dma.ar->setValid(0);
  dma.aw->setValid(0);
  dma.w->setValid(0);
  dma.r->setReady(0);
#endif


  printf("initial reset\n");
  for (int i = 0; i < 50; ++i) {
    top.clock = 0;
    tick(&top);
    tfp->dump(main_time);
    main_time += fpga_clock_inc;
    top.clock = 1;
    tick(&top);
    tfp->dump(main_time);
    main_time += fpga_clock_inc;
  }
  top.reset = !active_reset;
  top.clock = 0;

  LOG(printf("main time %lld\n", main_time));
  unsigned long ms = 1;
  int loops = 1000;

  printf("steady1\n");
  while (loops > 0) {
    loops--;
    top.clock = 1;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);

    top.clock = 0;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);
  }

  if (dma_file.has_value()) {
    printf("memory contents\n");
    readMemFile2ChipkitDMA(uart_chipfront.in_stream, *dma_file);
    top.CHIP_ASPSEL = 0;
    auto dma_last = uart_chipfront.in_stream.size();

    while (!uart_chipfront.in_stream.empty() || top.CHIP_UART_M_RXD == 0) {
      if (dma_last != uart_chipfront.in_stream.size() && (uart_chipfront.in_stream.size() % 100 == 0)) {
        printf("\r%d %zu %llu", loops, uart_chipfront.in_stream.size(), main_time);
        fflush(stdout);
        dma_last = uart_chipfront.in_stream.size();
      }

      top.clock = 1;
      main_time += fpga_clock_inc;
      tick(&top);
      tfp->dump(main_time);
      top.clock = 0;
      main_time += fpga_clock_inc;
      tick(&top);
      tfp->dump(main_time);
      uart_chipfront.tick();
    }
  }

  readMemFile2ChipkitDMA(uart_chipfront.in_stream, *trace_file);
  top.CHIP_ASPSEL = 1;
  printf("loading program\n\n");
  auto last = uart_chipfront.in_stream.size();
  while (!uart_chipfront.in_stream.empty() || top.CHIP_UART_M_RXD == 0) {
    loops++;
    if (last != uart_chipfront.in_stream.size() && (uart_chipfront.in_stream.size() % 100 == 0)) {
      printf("\r%d %zu %llu", loops, uart_chipfront.in_stream.size(), main_time);
      fflush(stdout);
      last = uart_chipfront.in_stream.size();
    }
    top.clock = 1;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);

    top.clock = 0;
    main_time += fpga_clock_inc;
    tick(&top);
    uart_chipfront.tick();
    tfp->dump(main_time);
  }
  printf("--program written--\n\n");
  fflush(stdout);

  loops = 1000;
  printf("steady2\n");
  while (loops > 0) {
    loops--;
    top.clock = 1;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);

    top.clock = 0;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);
  }

  printf("reset2\n");

  top.reset = active_reset;
  for (int i = 0; i < 6; ++i) {
    top.clock = 1;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);
    top.clock = 0;
    main_time += fpga_clock_inc;
    tick(&top);
    tfp->dump(main_time);
  }
  top.reset = !active_reset;

  printf("run program\n");
  int last_size = stdout_strm.size();

  int count = 10000;
  while (not kill_sig
         && (--count > 0)
          ) {
    // clock is high after posedge - changes now are taking place after posedge,
    // and will take effect on negedge
    if (main_time > ms * 1000 * 1000 * 1000) {
      printf("main time: %lu ms\n", ms);
      fflush(stdout);
      ++ms;
    }

#ifdef KILL_SIM
    if (ms >= KILL_SIM) {
      kill_sig = true;
      printf("killing\n");
      fflush(stdout);
    }
#endif
    top.clock = 1;// posedge
    main_time += fpga_clock_inc;
    // ------------ HANDLE COMMAND INTERFACE ----------------
//    assert(program.empty());
//    queue_uart(program, stdout_strm, top.CHIP_UART_M_RXD, top.STDUART_uart_txd);
//    queue_uart(program, stdout_strm, top.STDUART_uart_rxd, top.STDUART_uart_txd, 1);
    tick_signals(&uart_stdfront);
    if (last_size != stdout_strm.size()) {
      printf("%c", stdout_strm.back());
      last_size = stdout_strm.size();
      if (stdout_strm.back() == 0x4) {
        // this is the kill signal defined by the arm source (see uart_stdout.h)
        kill_sig = true;
      }
    }
    tick(&top);
    tfp->dump(main_time);
    top.clock = 0;// negedge
    tick(&top);
    main_time += fpga_clock_inc;
    tfp->dump(main_time);
  }
  printf("Final stdout print:\n");
  fflush(stdout);
  LOG(printf("printing traces\n"));
  fflush(stdout);
  tfp->close();
  for (auto &axi_mem: axi4_mems) {
    axi_mem.mem_sys->PrintStats();
  }
  sig_handle(0);
}


int main(int argc, char **argv) {
  signal(SIGTERM, sig_handle);
  signal(SIGABRT, sig_handle);
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);

  std::optional<std::string> dram_file = {};
  std::optional<std::string> trace_file = {};
  std::optional<std::string> dma_file = {};
  for (int i = 1; i < argc; ++i) {
    assert(argv[i][0] == '-');
    if (strcmp(argv[i] + 1, "dramconfig") == 0) {
      dram_file = std::string(argv[i + 1]);
    } else if (strcmp(argv[i] + 1, "tracefile") == 0) {
      trace_file = std::string(argv[i + 1]);
    } else if (strcmp(argv[i] + 1, "dmafile") == 0) {
      dma_file = std::string(argv[i + 1]);
    }
    ++i;
  }

  if (!dram_file.has_value()) {
    dram_file = std::string("../custom_dram_configs/DDR4_8Gb_x16_3200.ini");
  }
  assert(trace_file.has_value());

  LOG(printf("Entering verilator\n"));
  try {
    run_verilator(*dram_file);
    pthread_mutex_lock(&main_lock);
    pthread_mutex_lock(&main_lock);
    LOG(printf("Main thread exiting\n"));
  } catch (std::exception &e) {
    tfp->close();
    throw (e);
  }
  sig_handle(0);
}
