//
// Created by Chris Kjellqvist on 10/19/22.
//

#include "composer/fpga_handle.h"
#include <iostream>

using namespace composer;

std::vector<fpga_handle_t*> composer::active_fpga_handles;

fpga_handle_t* composer::current_handle_context;

void composer::set_fpga_context(fpga_handle_t* handle) {
  current_handle_context = handle;
  if (std::find(active_fpga_handles.begin(), active_fpga_handles.end(), handle) == active_fpga_handles.end()) {
    std::cerr << "The provided handle appears to have not been properly constructed. Please use the provided"
                 " fpga_handle_t constructors." << std::endl;
    exit(1);
  }
}