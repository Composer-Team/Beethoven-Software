//
// Created by Chris Kjellqvist on 8/9/23.
//

#include "beethoven_allocator_declaration.h"
#include "sim/axi/front_bus_ctrl_axi.h"
#include "sim/mem_ctrl.h"
#include "sim/chipkit/state_machine.h"

unsigned int baud_sel = 14;
int baud_div = baud_table[baud_sel]*4;


void set_baud(unsigned int baud_flag) {
  assert(baud_flag != 15 && baud_flag >= 0);
  baud_sel = baud_flag;
  baud_div = baud_table[baud_sel]*4;
}

unsigned int get_baud_sel() {
  return baud_sel;
}


#if VERILATOR

#ifdef USE_VCD
#include <verilated_vcd_c.h>
extern VerilatedVcdC *tfp;
#else
#include <verilated_fst_c.h>
extern VerilatedFstC *tfp;
#endif

static void sig_handle(int sig) {
  for (auto q: axi4_mems) {
    q.mem_sys->PrintEpochStats();
  }
  tfp->close();
  fprintf(stderr, "FST written!\n");
  fflush(stderr);
  exit(sig);
}
#endif

