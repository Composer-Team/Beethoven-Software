//
// Created by Chris Kjellqvist on 10/29/22.
//

#ifndef BEETHOVEN_VERILATOR_VERILATOR_H
#define BEETHOVEN_VERILATOR_VERILATOR_H
#include "dram_system.h"
#include <beethoven_hardware.h>
#include <verilated.h>
#include <optional>

#ifdef USE_VCD
#include "verilated_vcd_c.h"
using waveTrace = VerilatedVcdC;
#define TRACE_FILE_ENDING ".vcd"
#else
#include "verilated_fst_c.h"
using waveTrace = VerilatedFstC;
#define TRACE_FILE_ENDING ".fst"
#endif

extern waveTrace *tfp;

void run_verilator(const std::string &dram_config_file);

void sig_handle(int sig);

extern bool active_reset;

#endif//BEETHOVEN_VERILATOR_VERILATOR_H
