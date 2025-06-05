//
// Created by Christopher Kjellqvist on 11/14/24.
//

#include "fpga_utils.h"
#include <mmio.h>
#include <thread>
#include <iostream>
using namespace std::chrono_literals;
int main() {
  fpga_setup(0);
  auto ntxs = 64;
  poke_mmio(0, 64);
  std::this_thread::sleep_for(5s);
  uint64_t q = peek_mmio(0);
  auto n_cycles = q * 32;
  auto n_bytes = ntxs * 64 * 64;
  double s(n_cycles);
  s /= 250 * 1000 * 1000;
  double GB = double(n_bytes) / 1024 / 1024 / 1024;
  std::cout << n_bytes << " in " << n_cycles << "cycles: " << (GB / s) << "GB/s" << std::endl;
}