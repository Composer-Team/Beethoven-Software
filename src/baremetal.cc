//
// Created by Christopher Kjellqvist on 2/19/24.
//


#include "beethoven/fpga_handle.h"
#include "beethoven/rocc_cmd.h"
#include "beethoven/rocc_response.h"

#include <beethoven_allocator_declaration.h>

using namespace beethoven;

#define MMIO_BASE ((intptr_t)BeethovenMMIOOffset)

uint32_t actives = 0;
uint32_t valid_resps = 0;
rocc_response resps[sizeof(int)*8];

beethoven::fpga_handle_t::~fpga_handle_t() {}

beethoven::fpga_handle_t::fpga_handle_t() {}

template<> rocc_response response_handle<rocc_response>::get() {
  auto resp = rg.get();
  // memory segments on discrete targets need to get copied back if they are allocated to indicate that the FPGA writes
  return resp;
}


response_handle<rocc_response> beethoven::fpga_handle_t::send(const beethoven::rocc_cmd &c) {
  return c.send();
}

extern "C" {
extern void print_str(const char *s);
extern void print_32bHex(uint32_t q);
}

rocc_response response_getter::get() { // NOLINT(*-no-recursion)
  uint32_t resp_flag = 1 << id;
  if ((valid_resps & resp_flag) == resp_flag) {
    valid_resps ^= resp_flag;
    actives ^= resp_flag;
    return resps[id];
  }
  // no fancy error handling or correctness checks here.
  while (peek_addr(MMIO_BASE + RESP_VALID) == 0) {}
  uint32_t buf[3];
  buf[0] = peek_addr(MMIO_BASE + RESP_BITS);
  poke_addr(MMIO_BASE + RESP_READY, 1);
  buf[1] = peek_addr(MMIO_BASE + RESP_BITS);
  poke_addr(MMIO_BASE + RESP_READY, 1);
  buf[2] = peek_addr(MMIO_BASE + RESP_BITS);
  poke_addr(MMIO_BASE + RESP_READY, 1);

  auto r = rocc_response(buf, pack_cfg);
  if (id == r.rd) {
    actives ^= resp_flag;
    return r;
  } else {
    resps[r.rd] = r;
    valid_resps |= resp_flag;
    return get();
  }
}
