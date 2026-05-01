//
// Created by Chris Kjellqvist on 8/9/23.
//

#include "frontends/axi/front_bus_ctrl_axi.h"
#include "beethoven_hardware.h"
#include "core/mem_ctrl.h"
#include "core/util.h"
#include "core/cmd_server.h"
#include "core/state_machine.h"
#include <csignal>

#ifdef VERILATOR
#include <verilated_fst_c.h>
#include <verilated_vcd_c.h>
#endif

extern pthread_mutex_t cmdserverlock;
extern std::queue<beethoven::rocc_cmd> cmds;
extern std::unordered_map<system_core_pair, std::queue<int> *> in_flight;
extern uint64_t main_time;
extern uint64_t time_last_command;
int cmds_inflight = 0;
extern bool kill_sig;

extern uint64_t memory_transacted;



#ifndef DEFAULT_PL_CLOCK
#define FPGA_CLOCK 100
#else
#define FPGA_CLOCK DEFAULT_PL_CLOCK
#endif
