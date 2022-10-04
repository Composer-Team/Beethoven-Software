//
// Created by Chris Kjellqvist on 9/27/22.
//

#ifndef COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
#define COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H

#include <pthread.h>
#include <cstdint>
#include <rocc.h>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <pthread.h>
#include <cstdint>

#define MAX_CONCURRENT_COMMANDS 256

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

struct comm_file {
  pthread_mutex_t server_mut;
  // server return values
  char fname[512];
  uint64_t addr;
  // client request
  size_t fsize;
  pthread_mutex_t client_mutex;
  pthread_mutex_t wait_for_request_process;
};

#endif //COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
