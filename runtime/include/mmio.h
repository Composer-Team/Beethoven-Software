//
// Created by Chris Kjellqvist on 12/2/22.
//

#ifndef BEETHOVEN_VERILATOR_MMIO_H
#define BEETHOVEN_VERILATOR_MMIO_H

#include <beethoven_hardware.h>
#include <cinttypes>

#ifdef Kria
#if AXIL_BUS_WIDTH == 64
using devmem_ptr_t = uint64_t;
#elif AXIL_BUS_WIDTH == 32
using devmem_ptr_t = uint32_t;
#else
using devmem_ptr_t = uint8_t;
#endif
using peek_t = devmem_ptr_t;
extern devmem_ptr_t *devmem_map;
#else
using peek_t = uint32_t;
#endif

void setup_mmio();


void poke_mmio(uint64_t off, uint32_t val);
peek_t peek_mmio(uint32_t addr);

#endif //BEETHOVEN_VERILATOR_MMIO_H
