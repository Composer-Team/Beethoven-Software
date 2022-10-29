//
// Created by Chris Kjellqvist on 9/27/22.
//

#ifndef COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
#define COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H

#include <pthread.h>
#include <cstdint>
#include "rocc_cmd.h"
#include <vector>
#include <tuple>
#include <unordered_map>
#include <pthread.h>
#include <cstdint>

#define MAX_CONCURRENT_COMMANDS 256
namespace composer {
  static const std::string cmd_server_file_name = "/tmp/composer_cmd_server";
  static const std::string data_server_file_name = "/tmp/composer_data_server";

  struct cmd_server_file {
    pthread_mutex_t server_mut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t cmd_send_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t cmd_recieve_server_resp_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t wait_for_response[MAX_CONCURRENT_COMMANDS]{PTHREAD_MUTEX_INITIALIZER};

    rocc_response responses[MAX_CONCURRENT_COMMANDS];
    pthread_mutex_t free_list_lock;
    uint16_t free_list[MAX_CONCURRENT_COMMANDS];
    uint16_t free_list_idx = 255;
    // server return values
    uint64_t pthread_wait_id = 0;
    // client request
    rocc_cmd cmd;
  };

  enum data_server_op {
    ALLOC = 0,
    FREE = 1,
    MOVE_FROM_FPGA = 2,
    MOVE_TO_FPGA = 3
  };

  struct data_server_file {
    pthread_mutex_t server_mut;
    pthread_mutex_t data_cmd_send_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t data_cmd_recieve_resp_lock = PTHREAD_MUTEX_INITIALIZER;
    // server return values
    char fname[512];
    // client request
    data_server_op operation;

    // when allocation, pass in the length argument here and return value is FPGA addr + fname
    // when free, pass in free address here and there is no return valued
    uint64_t op_argument;
    uint64_t op2_argument;
    uint64_t op3_argument;
  };
}

#endif //COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
