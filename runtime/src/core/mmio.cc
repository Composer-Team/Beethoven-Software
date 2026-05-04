//
// Created by Chris Kjellqvist on 12/2/22.
//

#include "fpga/fpga_utils.h"
#include "core/mmio.h"
#include <iostream>

#if (defined(ZYNQ) || AWS) && !defined(SIM)
#include <sys/mman.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
int devmem_fd;
volatile char *devmem_map;

#endif

void setup_mmio() {
  // Kria specific initialization
#if (defined(ZYNQ) || AWS) && !defined(SIM)
#ifdef ZYNQ
  unsigned long long devmem_off = 0x2000000000ULL;
#elif AWS
  unsigned long long devmem_off = 0x5004c000000ULL; 
#endif
  devmem_fd = open("/dev/mem", O_SYNC | O_RDWR);
  if (devmem_fd < 0) {
    std::cerr << "Error opening /dev/mem. Errno: " << strerror(errno) << std::endl;
    exit(errno);
  }
  void *dvm = mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, devmem_fd, devmem_off);
  if (dvm == MAP_FAILED) {
    std::cerr << "Failed to map devmem using mmap. Errno: " << strerror(errno) << std::endl;
    exit(errno);
  }
  devmem_map = (char *) dvm;
#endif
}


void poke_mmio(uint64_t addr, uint32_t val) {
#if (defined(ZYNQ) || AWS) && !defined(SIM)
  *((volatile uint32_t *)(devmem_map + addr)) = val;
#else
  throw std::runtime_error("Poking inside unexpected platform");
#endif
}


uint32_t peek_mmio(uint32_t addr) {
#if (defined(ZYNQ) || AWS) && !defined(SIM)
  return *((volatile uint32_t *)(devmem_map + addr));
#else
  throw std::runtime_error("Poking inside unexpected platform");
  return 0;
#endif
}
