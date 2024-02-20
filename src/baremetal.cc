//
// Created by Christopher Kjellqvist on 2/19/24.
//


/**
 * @brief send a command to the FPGA
 * @return handle referring to response that the command will return. Allows for blocking on the response.
 */
#include "composer/fpga_handle.h"
#include "composer/rocc_cmd.h"
#include "composer/rocc_response.h"

#include <composer_allocator_declaration.h>

using namespace composer;

#define MMIO_BASE ((intptr_t)ComposerMMIOOffset)

int actives = 0;
int valid_resps = 0;
rocc_response resps[sizeof(int)*8];

composer::fpga_handle_t::~fpga_handle_t() {}

composer::fpga_handle_t::fpga_handle_t() {}

response_handle<rocc_response> composer::fpga_handle_t::send(const composer::rocc_cmd &c) {
  return c.send();
}

rocc_response response_getter::get() { // NOLINT(*-no-recursion)
  if (!(valid_resps & (1 << id))) {
    int mask = ~(1 << id);
    valid_resps &= mask;
    actives &= mask;
    return resps[id];
  }
  // no fancy error handling or correctness checks here.
  while (peek_addr(MMIO_BASE + RESP_VALID) == 0) {}
  uint32_t buf[2];
  buf[0] = peek_addr(MMIO_BASE + RESP_BITS);
  poke_addr(MMIO_BASE + RESP_READY, 1);
  buf[1] = peek_addr(MMIO_BASE + RESP_BITS);
  poke_addr(MMIO_BASE + RESP_READY, 1);

  auto r = rocc_response(buf, pack_cfg);
  if (id == r.rd) {
    valid_resps &= ~(1 << id);
    return r;
  } else {
    resps[r.rd] = r;
    valid_resps |= (1 << r.rd);
    return get();
  }
}
