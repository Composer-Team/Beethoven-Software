//
// Created by Christopher Kjellqvist on 8/27/24.
//

#ifndef BEETHOVENRUNTIME_CHIP_TICK_H
#define BEETHOVENRUNTIME_CHIP_TICK_H

#ifndef DEFAULT_PL_CLOCK
#define FPGA_CLOCK 100
#else
#define FPGA_CLOCK DEFAULT_PL_CLOCK
#endif

#include "sim/chipkit/util.h"
#include "sim/tick.h"

bool has_moved_memory = false;
bool has_moved_program = false;
bool has_reset = false;
bool loading_program = false;
std::optional<std::string> dma_file;
std::optional<std::string> trace_file;
int static_steps = 0;

extern bool active_reset;

template<typename t>
void tick_chip(
        ChipkitControlIntf<t> stdfront,
        ChipkitControlIntf<t> chipfront,
        t chip_aspsel,
        t reset) {
  if (dma_file.has_value() && !has_moved_memory) {
    printf("memory contents\n");
    ChipKit::readMemFile2ChipkitDMA(chipfront.in_stream, *dma_file);
    chip_aspsel.set(0);
    has_moved_memory = true;
  } else if (chipfront.in_stream.empty() && !has_moved_program) {
    printf("loading program\n");
    ChipKit::readMemFile2ChipkitDMA(chipfront.in_stream, *trace_file);
    chip_aspsel.set(1);
    has_moved_program = true;
  }

  if (has_moved_memory && !has_reset && chipfront.rxd.get() == 1) {
    static_steps++;
    if (static_steps == 20) {
      printf("resetting\n");
      reset.set(active_reset);
      has_reset = true;
    }
  } else {
    reset.set(!active_reset);
  }

  chipfront.tick();
  // have to reset chip after program is loaded
  if (has_reset) {
    tick_signals(&stdfront);
  }
}

#endif //BEETHOVENRUNTIME_TICK_H
