//
// Created by Christopher Kjellqvist on 8/6/24.
//

//#include "vcs_vpi_user.h"
//#include "sv_vpi_user.h"
#include "sim/mem_ctrl.h"
#include "sim/chipkit/state_machine.h"
#include "sim/tick.h"
#include "sim/axi/vcs_handle.h"
#include "cmd_server.h"
#include "data_server.h"
#include <pthread.h>
#include "sim/chipkit/tick.h"
#include "beethoven_allocator_declaration.h"
#include "sim/front_bus_ctrl_uart.h"

#include <cstring>
#include <vector>

bool active_reset = 1;

ChipkitControlIntf<VCSShortHandle> uart_chipfront;
ChipkitControlIntf<VCSShortHandle> uart_stdfront;
vpiHandle reset;
vpiHandle aspsel;

std::vector<vpiHandle> inputs, outputs;
uint64_t memory_transacted = 0;
#if NUM_DDR_CHANNELS >= 1
extern int writes_emitted;
extern int reads_emitted;
#endif
uint64_t main_time = 0;
pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;
float ddr_clock_inc;
auto fpga_clock_inc = 500000 / DEFAULT_PL_CLOCK;
bool kill_sig;
extern "C" PLI_INT32 init_input_signals_calltf(PLI_BYTE8 *user_data);
extern "C" PLI_INT32 init_output_signals_calltf(PLI_BYTE8 *user_data);
extern "C" PLI_INT32 init_structures_calltf(PLI_BYTE8 *user_data);
extern "C" PLI_INT32 tick_calltf(PLI_BYTE8 *user_data);

void print_state(uint64_t mem, uint64_t time) {
  std::string time_string;
  // in seconds
  if (time < 1000 * 1000 * 1000) {
    time_string = std::to_string(double(time) / 1000 / 1000) + "µs";
  } else {
    time_string = std::to_string(double(time) / 1000 / 1000 / 1000) + "ms";
  }

  std::string mem_unit;
  double mem_norm;
  if (mem < 1024) {
    mem_unit = "B";
    mem_norm = (double) mem;
  } else if (mem < 1024 * 1024) {
    mem_unit = "KB";
    mem_norm = (double) mem / 1024;
  } else if (mem < 1024 * 1024 * 1024) {
    mem_unit = "MB";
    mem_norm = (double) mem / 1024 / 1024;
  } else {
    mem_unit = "GB";
    mem_norm = (double) mem / 1024 / 1024 / 1024;
  }

#if NUM_DDR_CHANNELS >= 1
  std::string mem_rate;
  // compute bytes per second and normalize
  double time_d = (double) time / 1e12;
  double mem_rate_norm = (double) mem / time_d;
  if (mem_rate_norm < 1024) {
    mem_rate = std::to_string(mem_rate_norm) + "B/s";
  } else if (mem_rate_norm < 1024 * 1024) {
    mem_rate = std::to_string(mem_rate_norm / 1024) + "KB/s";
  } else if (mem_rate_norm < 1024 * 1024 * 1024) {
    mem_rate = std::to_string(mem_rate_norm / 1024 / 1024) + "MB/s";
  } else {
    mem_rate = std::to_string(mem_rate_norm / 1024 / 1024 / 1024) + "GB/s";
  }

  std::cout << "\rTime: " << time_string << " | Memory: " << mem_norm << mem_unit << " | Rate: " << mem_rate << " | w("
            << writes_emitted << ") r(" << reads_emitted << ")";
#else
  std::cout << "\rTime: " << time_string;
#endif
}

PLI_INT32 init_input_signals_calltf(PLI_BYTE8 * /*user_data*/) {
  std::cout << "init inputs" << std::endl;
  vpiHandle syscall_handle = vpi_handle(vpiSysTfCall, nullptr);
  vpiHandle arg_iter = vpi_iterate(vpiArgument, syscall_handle);
  // Cache Inputs
  if (arg_iter != nullptr) {
    while (vpiHandle arg_handle = vpi_scan(arg_iter)) {
      inputs.push_back(arg_handle);
    }
  }
  return 0;
}

PLI_INT32 init_output_signals_calltf(PLI_BYTE8 * /*user_data*/) {
  std::cout << "init outputs" << std::endl;
  vpiHandle syscall_handle = vpi_handle(vpiSysTfCall, nullptr);
  vpiHandle arg_iter = vpi_iterate(vpiArgument, syscall_handle);
  // Cache Inputs
  if (arg_iter != nullptr) {
    while (vpiHandle arg_handle = vpi_scan(arg_iter)) {
      outputs.push_back(arg_handle);
    }
  }
  return 0;
}

vpiHandle getHandle(const std::string &name) {
  for (const auto &arg: inputs) {
    // get the name of each signal and return the handle to it if the argument matches
    auto nm = vpi_get_str(vpiName, arg);
    if (strcmp(nm, name.c_str()) == 0) {
      return arg;
    }
  }

  for (const auto &arg: outputs) {
    // get the name of each signal and return the handle to it if the argument matches
    auto nm = vpi_get_str(vpiName, arg);
    if (strcmp(nm, name.c_str()) == 0) {
      return arg;
    }
  }
  printf("couldn't find binding for %s\n", name.c_str());
  exit(1);
}

PLI_INT32 init_structures_calltf(PLI_BYTE8 *) {
  std::cout << "init structures: " << std::endl;
  // at this point, we have all the inputs and outputs, and we have to tie them into the interfaces
#if NUM_DDR_CHANNELS >= 1
  mem_ctrl::init("custom_dram_configs/DDR4_8Gb_x16_3200.ini");

  for (auto &axi4_mem: axi4_mems) {
    axi4_mem.init_dramsim3();
  }

  ddr_clock_inc = (1000.0 / dramsim3config->tCK) / DEFAULT_PL_CLOCK;
  axi4_mems[0].ar.init(VCSShortHandle(getHandle("M00_AXI_arready")),
                       VCSShortHandle(getHandle("M00_AXI_arvalid")),
                       VCSShortHandle(getHandle("M00_AXI_arid")),
                       VCSShortHandle(getHandle("M00_AXI_arsize")),
                       VCSShortHandle(getHandle("M00_AXI_arburst")),
                       VCSShortHandle(getHandle("M00_AXI_araddr")),
                       VCSShortHandle(getHandle("M00_AXI_arlen")));
  axi4_mems[0].aw.init(VCSShortHandle(getHandle("M00_AXI_awready")),
                       VCSShortHandle(getHandle("M00_AXI_awvalid")),
                       VCSShortHandle(getHandle("M00_AXI_awid")),
                       VCSShortHandle(getHandle("M00_AXI_awsize")),
                       VCSShortHandle(getHandle("M00_AXI_awburst")),
                       VCSShortHandle(getHandle("M00_AXI_awaddr")),
                       VCSShortHandle(getHandle("M00_AXI_awlen")));
  VCSShortHandle dummy;
  axi4_mems[0].w.init(VCSShortHandle(getHandle("M00_AXI_wready")),
                      VCSShortHandle(getHandle("M00_AXI_wvalid")),
                      VCSShortHandle(getHandle("M00_AXI_wlast")),
                      dummy,
                      VCSLongHandle(getHandle("M00_AXI_wstrb")),
                      VCSLongHandle(getHandle("M00_AXI_wdata")));
  axi4_mems[0].r.init(VCSShortHandle(getHandle("M00_AXI_rready")),
                      VCSShortHandle(getHandle("M00_AXI_rvalid")),
                      VCSShortHandle(getHandle("M00_AXI_rlast")),
                      VCSShortHandle(getHandle("M00_AXI_rid")),
                      dummy,
                      VCSLongHandle(getHandle("M00_AXI_rdata")));
  axi4_mems[0].b.init(VCSShortHandle(getHandle("M00_AXI_bready")),
                      VCSShortHandle(getHandle("M00_AXI_bvalid")),
                      VCSShortHandle(getHandle("M00_AXI_bid")));
#if NUM_DDR_CHANNELS >= 2
#error "not implemented yet"
#endif
#endif

  // initialize the unused fields (e.g., ID)

  uart_chipfront.rxd = getHandle("CHIP_UART_M_RXD");
  uart_chipfront.txd = getHandle("CHIP_UART_M_TXD");
  uart_stdfront.rxd = getHandle("STDUART_uart_rxd");
  uart_stdfront.txd = getHandle("STDUART_uart_txd");

  reset = getHandle("reset");
  aspsel = getHandle("CHIP_ASPSEL");
  s_vpi_value value;
  value.format = vpiIntVal;
  value.value.integer = 0;
  vpi_put_value(aspsel, &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_SCLK1"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_SCLK2"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_SHIFTIN"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_SHIFTOUT"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_FESEL"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("CHIP_UART_M_CTS"), &value, nullptr, vpiNoDelay);
  value.value.integer = get_baud_sel();
  vpi_put_value(getHandle("CHIP_UART_M_CTS"), &value, nullptr, vpiNoDelay);
  value.value.integer = 1;
  vpi_put_value(getHandle("CHIP_UART_M_RXD"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("STDUART_uart_rxd"), &value, nullptr, vpiNoDelay);

  std::cout << "finished init structures" << std::endl;
  return 0;
}

#include "sim/chipkit/util.h"

PLI_INT32 tick_calltf(PLI_BYTE8 * /*user_data*/) {
  main_time += fpga_clock_inc;;
  if (main_time % 1000 == 0) {
    print_state(memory_transacted, main_time);
  }

  VCSShortHandle asp(aspsel);
  VCSShortHandle res(reset);

  tick_chip(uart_stdfront, uart_chipfront, asp, res);
  return 0;
}

