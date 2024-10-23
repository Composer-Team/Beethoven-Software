//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "beethoven/allocator/alloc_baremetal.h"

using namespace beethoven;


remote_ptr::~remote_ptr() {
}

remote_ptr::remote_ptr() {}

remote_ptr &remote_ptr::operator=(const remote_ptr &other) noexcept {
  fpga_addr = other.fpga_addr;
  host_addr = other.host_addr;
  return *this;
}

remote_ptr::remote_ptr(const intptr_t &faddr, void *haddr) noexcept:
    fpga_addr(faddr),
    host_addr(haddr) {}


remote_ptr remote_ptr::operator+(int q) const {
  return remote_ptr(this->fpga_addr + q,
                    (char *) (this->host_addr) + q);
}

remote_ptr::remote_ptr(const remote_ptr &other) noexcept:
    fpga_addr(other.fpga_addr),
    host_addr(other.host_addr) {};

remote_ptr::remote_ptr(const intptr_t &faddr) noexcept:
    fpga_addr(faddr),
    host_addr(nullptr) {}

remote_ptr::remote_ptr(remote_ptr &&other) noexcept:
    fpga_addr(other.fpga_addr),
    host_addr(other.host_addr) {};

remote_ptr remote_ptr::operator-(int q) const {
  return remote_ptr(this->fpga_addr - q,
                    (char *) (this->host_addr) - q);
}
