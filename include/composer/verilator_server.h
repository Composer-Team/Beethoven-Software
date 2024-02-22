//
// Created by Chris Kjellqvist on 9/27/22.
//

#ifndef COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
#define COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H

#ifndef BAREMETAL

#include <composer/rocc_response.h>
#include <composer/rocc_cmd.h>
#include <string>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

#define MAX_CONCURRENT_COMMANDS 256
namespace composer {

  const extern std::string cmd_server_file_name;
  const extern std::string data_server_file_name;

  const int file_access_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  const int file_access_prots = PROT_READ | PROT_WRITE;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

  struct cmd_server_file { // NOLINT(cppcoreguidelines-pro-type-member-init)
    // lingo: process = client, server = composer runtime
    // lock to wake up server
    pthread_mutex_t server_mut = PTHREAD_MUTEX_INITIALIZER;
    // lock for processes to arbitrate who's sending command
    pthread_mutex_t cmd_send_lock = PTHREAD_MUTEX_INITIALIZER;
    // lock to notify process of command send result
    pthread_mutex_t cmd_recieve_server_resp_lock = PTHREAD_MUTEX_INITIALIZER;
    // locks to stall processes until result is ready
    pthread_mutex_t wait_for_response[MAX_CONCURRENT_COMMANDS]{PTHREAD_MUTEX_INITIALIZER};
    rocc_response responses[MAX_CONCURRENT_COMMANDS];

    sem_t processes_waiting = {};

    // once we've got a result back, need to free up response slot
    pthread_mutex_t free_list_lock; // needs pshared flag so can't initialize inline
    // stack of free response slots
    uint16_t free_list[MAX_CONCURRENT_COMMANDS];
    uint16_t free_list_idx = 255;
    // server return values - which response to use for process
    uint64_t pthread_wait_id = 0;
    uint64_t quit;

    // client request
    composer::rocc_cmd cmd;

    static void init(cmd_server_file &csf);
  };

#pragma clang diagnostic pop

  enum data_server_op {
    ALLOC = 0,
    FREE = 1,
    MOVE_FROM_FPGA = 2,
    MOVE_TO_FPGA = 3,
    ADD_TO_COHERENCE_MANAGER = 4,
    INVALIDATE_REGION = 5,
    CLEAN_INVALIDATE_REGION = 6,
    RELEASE_COHERENCE_BARRIER = 7,
  };

  struct data_server_file {
    pthread_mutex_t server_mut{};
    pthread_mutex_t data_cmd_send_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t data_cmd_recieve_resp_lock = PTHREAD_MUTEX_INITIALIZER;
    // server return values
    char fname[512]{};
    int16_t resp_id;
    // client request
    data_server_op operation = ALLOC;
    // when allocation, pass in the length argument here and return value is FPGA addr + fname
    // when free, pass in free address here and there is no return valued
    uint64_t op_argument{};
    uint64_t op2_argument{};
    uint64_t op3_argument{};

    static void init(data_server_file &dsf);
  };

}

#endif

#endif //COMPOSER_VERILATOR_COMPOSER_VERILATOR_SERVER_H
