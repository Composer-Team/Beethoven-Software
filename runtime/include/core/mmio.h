//
// Created by Chris Kjellqvist on 12/2/22.
//

#ifndef BEETHOVEN_VERILATOR_MMIO_H
#define BEETHOVEN_VERILATOR_MMIO_H

#include <beethoven_hardware.h>
#include <cinttypes>


void setup_mmio();

void poke_mmio(uint64_t addr, uint32_t val);

uint32_t peek_mmio(uint32_t addr);

#endif //BEETHOVEN_VERILATOR_MMIO_H
