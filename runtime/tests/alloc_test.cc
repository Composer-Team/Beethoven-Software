//
// Created by Christopher Kjellqvist on 5/25/24.
//

#include <iostream>
#include "data_server.h"
#include <beethoven/fpga_handle.h>
#include <random>


using namespace beethoven;

int main() {
  fpga_handle_t handle;
  std::vector<unsigned long> sizes;
// for 4K up to 512MB by power of two, make allocations
  for (int i = 12; i <= 29; i++) {
    sizes.push_back(1 << i);
  }
  // shuffle sizes
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(sizes.begin(), sizes.end(), g);

  std::vector<remote_ptr> ptrs;

  for (auto size : sizes) {
    auto ptr = handle.malloc(size);
    ptrs.push_back(ptr);
  }

  // reshuffle ptrs
  std::shuffle(ptrs.begin(), ptrs.end(), g);
  // free them
  for (auto ptr : ptrs) {
    handle.free(ptr);
  }


  return 0;
}