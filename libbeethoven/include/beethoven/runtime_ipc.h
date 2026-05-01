//
// Created by Chris Kjellqvist on 9/27/22.
//

#ifndef BEETHOVEN_RUNTIME_IPC_H
#define BEETHOVEN_RUNTIME_IPC_H

#ifndef BAREMETAL

#include "rocc_response.h"
#include "rocc_cmd.h"
#include <string>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>

#define MAX_CONCURRENT_COMMANDS 256
namespace beethoven {

  std::string cmd_server_file_name();
  std::string data_server_file_name();

  const int file_access_flags = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  const int file_access_prots = PROT_READ | PROT_WRITE;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

  struct cmd_server_file { // NOLINT(cppcoreguidelines-pro-type-member-init)
    // lingo: process = client, server = beethoven runtime
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
    int pthread_wait_id = 0;
    uint64_t quit;

    // client request
    beethoven::rocc_cmd cmd;

    static void init(cmd_server_file &csf);
  };

#pragma clang diagnostic pop

  enum data_server_op {
    ALLOC = 0,
    FREE = 1,
    MOVE_FROM_FPGA = 2,
    MOVE_TO_FPGA = 3,
//    ADD_TO_COHERENCE_MANAGER = 4,
//    INVALIDATE_REGION = 5,
//    CLEAN_INVALIDATE_REGION = 6,
//    RELEASE_COHERENCE_BARRIER = 7,
  };

  struct data_server_file {
    pthread_mutex_t server_mut{};
    pthread_mutex_t data_cmd_send_lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t data_cmd_recieve_resp_lock = PTHREAD_MUTEX_INITIALIZER;
    // server return values
    char fname[512]{};
    int16_t resp_id = 0;
    // client request
    data_server_op operation = ALLOC;
    // when allocation, pass in the length argument here and return value is FPGA addr + fname
    // when free, pass in free address here and there is no return valued
    uint64_t op_argument{};
    uint64_t op2_argument{};
    uint64_t op3_argument{};

    // Set by the runtime daemon at startup. libbeethoven is built once
    // globally (no SIM/non-SIM split), so platform-specific code paths that
    // need to behave differently in sim vs real silicon — notably
    // fpga_handle_zynq.cc::malloc(), which uses host mmap+vtop on real
    // hardware but must go through the data_server in sim — branch on this
    // flag at runtime. Placed at the end of the struct so the shmem ABI
    // stays stable for any client built against an older header.
    bool is_simulation = false;

    static void init(data_server_file &dsf);
  };

}

#endif

#endif //BEETHOVEN_RUNTIME_IPC_H
