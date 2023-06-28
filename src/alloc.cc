//
// Created by Chris Kjellqvist on 10/17/22.
//

#include "composer/alloc.h"
#include <cstring>

using namespace composer;

std::string remote_ptr::printError() {
  if (host_addr != nullptr) return "No Error";
  if (len == composer::ERR_ALLOC_TOO_BIG) {
    return "Allocation is too big. Largest supported for Kria is 1GB.";
  } else {
    return "Allocation Failure in mmap() call. Likely due to existing fragmentation in physical memory.\n\t" + std::string(strerror(errno));
  }
}