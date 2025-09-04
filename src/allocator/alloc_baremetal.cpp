//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "beethoven/allocator/alloc_baremetal.h"

using namespace beethoven;

remote_ptr remote_ptr::operator+(int q) const {
  return remote_ptr(this->fpga_addr + q);
}

remote_ptr remote_ptr::operator-(int q) const {
  return remote_ptr(this->fpga_addr - q);
}
