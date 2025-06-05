//
// Created by Chris Kjellqvist on 11/6/22.
//

#include "response_poller.h"
#include "fpga_utils.h"
#include "mmio.h"
#ifdef VSIM
#include "sh_dpi_tasks.h"
#endif

#include "cmd_server.h"
#include <beethoven_hardware.h>
#include <thread>
#include "util.h"

using namespace std::chrono_literals;

[[noreturn]] static void* poll_thread(void *in) {
  auto sem = (sem_t *) in;
  while (true) {
    sem_wait(sem);
    uint32_t buf[3];
    for (unsigned int &i: buf) {
      uint32_t resp_ready = 0;
      while (!resp_ready) {
        pthread_mutex_lock(&bus_lock);
        resp_ready = peek_mmio(RESP_VALID);
        pthread_mutex_unlock(&bus_lock);
        if (not resp_ready) {
          std::this_thread::sleep_for(10us);
        }
      }
      pthread_mutex_lock(&bus_lock);
      i = peek_mmio(RESP_BITS);
      poke_mmio(RESP_READY, 1);
      pthread_mutex_unlock(&bus_lock);

    }
    LOG(std::cerr << "Got response buffer" << std::endl);
    register_reponse(buf);
    LOG(std::cerr << "Successfully enqueued response" << std::endl);
  }
}

void response_poller::start_poller(sem_t *t) {
  pthread_t thread;
  pthread_create(&thread, nullptr, poll_thread, (void *) t);
}
