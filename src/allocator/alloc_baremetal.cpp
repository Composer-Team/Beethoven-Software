//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "beethoven/allocator/alloc_baremetal.h"

using namespace beethoven;


remote_ptr::~remote_ptr() {
}

remote_ptr &remote_ptr::operator=(const remote_ptr &other) noexcept {
  fpga_addr = other.fpga_addr;
  host_addr = other.host_addr;
  return *this;
}

remote_ptr::remote_ptr(const intptr_t &faddr, void *haddr, const size_t &l, ptrdiff_t off) noexcept:
    fpga_addr(faddr),
    host_addr(haddr), len(l),
    offset(off) {}



remote_ptr::remote_ptr(intptr_t fpgaAddr,
                       void *hostAddr,
                       size_t len) :
    fpga_addr(fpgaAddr),
    host_addr(hostAddr),
    len(len) {
  offset = 0;
}

remote_ptr remote_ptr::operator+(int q) const {
  return remote_ptr(this->fpga_addr + q,
                    (char *) (this->host_addr) + q,
                    this->len - q,
                    offset + q

  );
}

remote_ptr::remote_ptr() :
    fpga_addr(0),
    host_addr(nullptr),
    len(0),
    offset(0) {}

remote_ptr::remote_ptr(const remote_ptr &other) noexcept:
    fpga_addr(other.fpga_addr),
    host_addr(other.host_addr),
    len(other.len),
    offset(other.offset) {
};

remote_ptr::remote_ptr(const intptr_t &faddr) noexcept:
    fpga_addr(faddr),
    host_addr(nullptr),
    len(0),
    offset(0) {
}

remote_ptr::remote_ptr(remote_ptr &&other) noexcept:
    fpga_addr(other.fpga_addr),
    host_addr(other.host_addr),
    len(other.len),
    offset(other.offset) {
};

remote_ptr remote_ptr::operator-(int q) const {
  return remote_ptr(this->fpga_addr - q,
                    (char *) (this->host_addr) - q,
                    this->len + q,
                    offset - q
  );
}
