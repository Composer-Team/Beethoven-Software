//
// Created by Chris Kjellqvist on 10/19/22.
//
#include "composer/response_handle.h"
#include "composer/rocc_cmd.h"
#include "composer/fpga_handle.h"

using namespace composer;


rocc_response response_getter::get() {
  if (can_wait) {
    if (has_recieved) {
      fprintf(stderr, "Attempted to wait on a return handle that has already received a response!\n");
      exit(1);
    } else {
      has_recieved = true;
      return h->get_response_from_handle(id);
    }
  } else {
    fprintf(stderr, "Attempting to wait on a return handle for a command that explicitly disallowed returns."
                    "All `addr` commands do not return. Start commands that specify `xd=0` will not return.\n");
    exit(1);
  }
}


std::ostream &operator<<(std::ostream &os, const composer::rocc_response &response) {
os << "data: " << response.data << " core_id: " << response.core_id << " system_id: " << response.system_id
<< " rd: " << response.rd;
return os;
}

template<> rocc_response response_handle<rocc_response>::get() {
  auto resp = rg.get();
  // memory segments on discrete targets need to get copied back if they are allocated to indicate that the FPGA writes
#ifndef Kria
  for (const remote_ptr &op : this->ops) {
    if (op.allocation_type == READWRITE || op.allocation_type == WRITE) {
      current_handle_context->copy_from_fpga(op);
    }
  }
#endif

  return resp;
}