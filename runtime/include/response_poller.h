//
// Created by Chris Kjellqvist on 11/6/22.
//

#ifndef BEETHOVEN_VERILATOR_RESPONSE_POLLER_H
#define BEETHOVEN_VERILATOR_RESPONSE_POLLER_H

#ifndef SIM

#include <pthread.h>
#include <semaphore.h>
#include <beethoven/verilator_server.h>
#include <thread>

extern beethoven::cmd_server_file *csf;

struct response_poller {
  static void start_poller(sem_t *t);
};

#endif

#endif //BEETHOVEN_VERILATOR_RESPONSE_POLLER_H
