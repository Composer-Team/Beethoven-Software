//
// Created by Chris Kjellqvist on 10/29/22.
//
#include "../include/cmd_server.h"
#include "../include/data_server.h"
#include <pthread.h>
#include "fpga_utils.h"
#include <cstring>
#include <iostream>

pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;
bool runtime_verbose = false;

#ifdef VSIM
extern "C" {
#include <sh_dpi_tasks.h>
};
extern "C" void test_main_hook(uint32_t *exit_code)
#else

int main(int argc, char *argv[])
#endif
{
#ifndef VSIM
  // Parse command-line arguments
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
      runtime_verbose = true;
      std::cout << "[RUNTIME] Verbose mode enabled" << std::endl;
    }
  }
#endif

#if AWS
  fpga_setup(0);
  if (runtime_verbose) {
    std::cout << "[RUNTIME] AWS FPGA setup completed" << std::endl;
  }
#endif
  // Kria does local allocations only
  if (runtime_verbose) {
    std::cout << "[RUNTIME] Starting command server..." << std::endl;
  }
  cmd_server::start();
  if (runtime_verbose) {
    std::cout << "[RUNTIME] Starting data server..." << std::endl;
  }
  data_server::start();
  if (runtime_verbose) {
    std::cout << "[RUNTIME] Servers started, entering wait state" << std::endl;
  }
  pthread_mutex_lock(&main_lock);
  pthread_mutex_lock(&main_lock);
#if AWS
  fpga_shutdown();
  if (runtime_verbose) {
    std::cout << "[RUNTIME] AWS FPGA shutdown completed" << std::endl;
  }
#endif
#ifdef VSIM
  *exit_code = 0;
#endif
}
