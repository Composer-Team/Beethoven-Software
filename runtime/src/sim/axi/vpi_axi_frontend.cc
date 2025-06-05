//
// Created by Christopher Kjellqvist on 8/6/24.
//

//#include "vcs_vpi_user.h"
//#include "sv_vpi_user.h"
#include "sim/mem_ctrl.h"
#include "sim/axi/state_machine.h"
#include "sim/tick.h"
#include "sim/axi/vpi_handle.h"
#include "cmd_server.h"
#include "data_server.h"
#include <pthread.h>

#include "beethoven_hardware.h"

#include <cstring>
#include <vector>

std::vector<vpiHandle> inputs, outputs;
AXIControlIntf<VCSShortHandle, VCSLongHandle, VCSLongHandle> ctrl;
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

extern "C" {
PLI_INT32 init_input_signals_calltf(PLI_BYTE8 *user_data);

PLI_INT32 init_output_signals_calltf(PLI_BYTE8 *user_data);

PLI_INT32 init_structures_calltf(PLI_BYTE8 *user_data);

PLI_INT32 tick_calltf(PLI_BYTE8 *user_data);

void print_state(uint64_t mem, uint64_t time) {
  return;
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
#ifdef DRAMSIM_CONFIG
  std::cout << "trying to init from '" DRAMSIM_CONFIG "'" << std::endl;
  mem_ctrl::init( DRAMSIM_CONFIG );
#else
  std::cout << "trying to init from default" << std::endl;
  mem_ctrl::init("custom_dram_configs/DDR4_8Gb_x16_3200.ini");
#endif

  std::cout << "DRAMsim init'd" << std::endl;

  for (auto &axi4_mem: axi4_mems) {
    axi4_mem.init_dramsim3();
  }

  std::cout << "Mem structures init'd" << std::endl;

  ddr_clock_inc = (1000.0 / dramsim3config->tCK) / DEFAULT_PL_CLOCK;
  axi4_mems[0].ar.init(VCSShortHandle(getHandle("M00_AXI_arready")),
                       VCSShortHandle(getHandle("M00_AXI_arvalid")),
                       VCSShortHandle(getHandle("M00_AXI_arid")),
                       VCSShortHandle(getHandle("M00_AXI_arsize")),
                       VCSShortHandle(getHandle("M00_AXI_arburst")),
                       VCSLongHandle(getHandle("M00_AXI_araddr")),
                       VCSShortHandle(getHandle("M00_AXI_arlen")));
  axi4_mems[0].aw.init(VCSShortHandle(getHandle("M00_AXI_awready")),
                       VCSShortHandle(getHandle("M00_AXI_awvalid")),
                       VCSShortHandle(getHandle("M00_AXI_awid")),
                       VCSShortHandle(getHandle("M00_AXI_awsize")),
                       VCSShortHandle(getHandle("M00_AXI_awburst")),
                       VCSLongHandle(getHandle("M00_AXI_awaddr")),
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

#ifdef BEETHOVEN_HAS_DMA
  dma.ar.init(VCSShortHandle(getHandle("dma_arready")),
              VCSShortHandle(getHandle("dma_arvalid")),
              VCSShortHandle(getHandle("dma_arid")),
              VCSShortHandle(getHandle("dma_arsize")),
              VCSShortHandle(getHandle("dma_arburst")),
              VCSLongHandle(getHandle("dma_araddr")),
              VCSShortHandle(getHandle("dma_arlen")));
  dma.aw.init(VCSShortHandle(getHandle("dma_awready")),
              VCSShortHandle(getHandle("dma_awvalid")),
              VCSShortHandle(getHandle("dma_awid")),
              VCSShortHandle(getHandle("dma_awsize")),
              VCSShortHandle(getHandle("dma_awburst")),
              VCSLongHandle(getHandle("dma_awaddr")),
              VCSShortHandle(getHandle("dma_awlen")));
  dma.w.init(VCSShortHandle(getHandle("dma_wready")),
             VCSShortHandle(getHandle("dma_wvalid")),
             VCSShortHandle(getHandle("dma_wlast")),
             dummy,
             VCSLongHandle(getHandle("dma_wstrb")),
             VCSLongHandle(getHandle("dma_wdata")));
  dma.r.init(VCSShortHandle(getHandle("dma_rready")),
             VCSShortHandle(getHandle("dma_rvalid")),
             VCSShortHandle(getHandle("dma_rlast")),
             VCSShortHandle(getHandle("dma_rid")),
             dummy,
             VCSLongHandle(getHandle("dma_rdata")));
  dma.b.init(VCSShortHandle(getHandle("dma_bready")),
             VCSShortHandle(getHandle("dma_bvalid")),
             VCSShortHandle(getHandle("dma_bid")));
#endif

  // initialize the unused fields (e.g., ID)

  auto aw_id_handle = getHandle("S00_AXI_awid");
  auto ar_id_handle = getHandle("S00_AXI_arid");
  s_vpi_value value;
  value.format = vpiIntVal;
  value.value.integer = 0;
  vpi_put_value(getHandle("S00_AXI_arburst"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arcache"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arid"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arlen"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arlock"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arprot"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arqos"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_arregion"), &value, nullptr, vpiNoDelay);
  value.value.integer = 2;
  vpi_put_value(getHandle("S00_AXI_arsize"), &value, nullptr, vpiNoDelay);
  value.value.integer = 0;
  vpi_put_value(getHandle("S00_AXI_awburst"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awcache"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awid"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awlen"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awlock"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awprot"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awqos"), &value, nullptr, vpiNoDelay);
  vpi_put_value(getHandle("S00_AXI_awregion"), &value, nullptr, vpiNoDelay);
  value.value.integer = 2;
  vpi_put_value(getHandle("S00_AXI_awsize"), &value, nullptr, vpiNoDelay);
  value.value.integer = 1;
  vpi_put_value(getHandle("S00_AXI_wlast"), &value, nullptr, vpiNoDelay);
  value.value.integer = 0xF;
  vpi_put_value(getHandle("S00_AXI_wstrb"), &value, nullptr, vpiNoDelay);

  ctrl.set_ar(
    VCSShortHandle(getHandle("S00_AXI_arvalid")),
    VCSShortHandle(getHandle("S00_AXI_arready")),
    VCSLongHandle(getHandle("S00_AXI_araddr")));
  ctrl.set_aw(
    VCSShortHandle(getHandle("S00_AXI_awvalid")),
    VCSShortHandle(getHandle("S00_AXI_awready")),
    VCSLongHandle(getHandle("S00_AXI_awaddr")));
  ctrl.set_w(
    VCSShortHandle(getHandle("S00_AXI_wvalid")),
    VCSShortHandle(getHandle("S00_AXI_wready")),
    VCSLongHandle(getHandle("S00_AXI_wdata")));
  ctrl.set_r(
    VCSShortHandle(getHandle("S00_AXI_rready")),
    VCSShortHandle(getHandle("S00_AXI_rvalid")),
    VCSLongHandle(getHandle("S00_AXI_rdata")));
  ctrl.set_b(
    VCSShortHandle(getHandle("S00_AXI_bready")),
    VCSShortHandle(getHandle("S00_AXI_bvalid")));


  std::cout << "start servers" << std::endl;

  cmd_server::start();
  data_server::start();

  std::cout << "finished init structures" << std::endl;
  return 0;
}

PLI_INT32 tick_calltf(PLI_BYTE8 * /*user_data*/) {
  main_time += fpga_clock_inc;;
  if (main_time % 10000 == 0) {
    print_state(memory_transacted, main_time);
  }
  if (main_time > 100000) {
    tick_signals(&ctrl);
  }
  return 0;
}


void tick_register(void) {
  s_vpi_systf_data tf_data;

  tf_data.type = vpiSysTask;
  tf_data.tfname = "$tick";
  tf_data.calltf = tick_calltf;
  tf_data.compiletf = 0;
  tf_data.sizetf = 0;
  tf_data.user_data = 0;
  vpi_register_systf(&tf_data);
}

void init_input_signals_register(void) {
  s_vpi_systf_data tf_data;

  tf_data.type = vpiSysTask;
  tf_data.tfname = "$init_input_signals";
  tf_data.calltf = init_input_signals_calltf;
  tf_data.compiletf = 0;
  tf_data.sizetf = 0;
  tf_data.user_data = 0;
  vpi_register_systf(&tf_data);
}

void init_output_signals_register(void) {
  s_vpi_systf_data tf_data;

  tf_data.type = vpiSysTask;
  tf_data.tfname = "$init_output_signals";
  tf_data.calltf = init_output_signals_calltf;
  tf_data.compiletf = 0;
  tf_data.sizetf = 0;
  tf_data.user_data = 0;
  vpi_register_systf(&tf_data);
}

void init_structures_register(void) {
  s_vpi_systf_data tf_data;

  tf_data.type = vpiSysTask;
  tf_data.tfname = "$init_structures";
  tf_data.calltf = init_structures_calltf;
  tf_data.compiletf = 0;
  tf_data.sizetf = 0;
  tf_data.user_data = 0;
  vpi_register_systf(&tf_data);
}

void (*vlog_startup_routines[])(void) = {
  tick_register,
  init_input_signals_register,
  init_output_signals_register,
  init_structures_register,
  0
};
} // end extern C
