//
// Created by Chris Kjellqvist on 10/19/22.
//

//
// Created by Christopher Kjellqvist on 2/19/24.
//


#include "beethoven/fpga_handle.h"
#include "beethoven/rocc_cmd.h"
#include "beethoven/rocc_response.h"

#include "beethoven/fpga_handle.h"
#include <cstring>
#include "beethoven/alloc.h"
#include "beethoven_allocator_declaration.h"
#include "beethoven/allocator/device_allocator.h"

device_allocator<1 << 24> allocator; // 16MB right now

using namespace beethoven;

/*
 * Copyright (c) 2019,
 * The University of California, Berkeley and Duke University.
 * All Rights Reserved.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "beethoven/verilator_server.h"
#include "beethoven/response_handle.h"
#include <unistd.h>

#include <fcntl.h>

using namespace beethoven;

static int cacheLineSz;
static int logCacheLineSz;


#pragma clang diagnostic push
#pragma ide diagnostic ignored "VirtualCallInCtorOrDtor"

#pragma clang diagnostic pop

fpga_handle_t::~fpga_handle_t() {}
fpga_handle_t::fpga_handle_t() {}

remote_ptr fpga_handle_t::malloc(size_t len) {
  auto ptr = allocator.malloc(len);
  return remote_ptr(ptr, reinterpret_cast<void*>(ptr), len);
}

[[maybe_unused]] void fpga_handle_t::copy_to_fpga(const remote_ptr &dst) {}

[[maybe_unused]] void fpga_handle_t::copy_from_fpga(const remote_ptr &src) {}

[[maybe_unused]] void fpga_handle_t::free(remote_ptr ptr) {
  allocator.free(ptr.fpga_addr);
}

using namespace beethoven;

#define MMIO_BASE ((intptr_t)BeethovenMMIOOffset)

uint32_t actives = 0;
uint32_t valid_resps = 0;
rocc_response resps[sizeof(int)*8];

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

