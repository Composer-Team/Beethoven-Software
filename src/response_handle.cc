//
// Created by Chris Kjellqvist on 10/19/22.
//
#include "beethoven/response_handle.h"
#include "beethoven/rocc_cmd.h"
#include "beethoven/fpga_handle.h"

using namespace beethoven;


rocc_response response_getter::get() {
  if (can_wait) {
    if (has_recieved) {
      throw new std::runtime_error("Attempted to wait on a return handle that has already received a response!\n");
    } else {
      has_recieved = true;
      return h->get_response_from_handle(id);
    }
  } else {
    throw new std::runtime_error("Attempting to wait on a return handle for a command that explicitly disallowed returns."
                    "All `addr` commands do not return. Start commands that specify `xd=0` will not return.\n");
    exit(1);
  }
}

std::optional<rocc_response> response_getter::try_get() {
  if (can_wait) {
    if (has_recieved) {
      fprintf(stderr, "Attempted to wait on a return handle that has already received a response!\n");
      exit(1);
    } else {
      auto q = h->try_get_response_from_handle(id);
      if (q.has_value()) {
        has_recieved = true;
      }
      return q;
    }
  } else {
    fprintf(stderr, "Attempting to wait on a return handle for a command that explicitly disallowed returns."
                    "All `addr` commands do not return. Start commands that specify `xd=0` will not return.\n");
    exit(1);
  }
}

std::ostream &operator<<(std::ostream &os, const beethoven::rocc_response &response) {
os << "data: " << response.data << " core_id: " << response.core_id << " system_id: " << response.system_id
<< " rd: " << response.rd;
return os;
}

template<> std::optional<rocc_response> response_handle<rocc_response>::try_get() {
  return rg.try_get();
}


template<> rocc_response response_handle<rocc_response>::get() {
  return rg.get();
}
