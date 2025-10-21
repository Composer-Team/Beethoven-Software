//
// Created by Chris Kjellqvist on 9/27/22.
//

#include "../include/cmd_server.h"
#include "../include/data_server.h"
#include "fpga_utils.h"
#include "mmio.h"
#include <beethoven/verilator_server.h>
#include <beethoven_hardware.h>
#include <sys/stat.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <tuple>
#include <unistd.h>

#include "response_poller.h"

// for shared memory
#include "util.h"
#include <cmath>
#include <fcntl.h>
#ifdef USE_VCS
#include "vcs_vpi_user.h"
#endif

#ifdef FPGA

#include <semaphore.h>
#include <chrono>

#endif
#ifdef SIM
extern "C" {
extern bool kill_sig;
};
#endif

system_core_pair::system_core_pair(int system, int core) {
  this->system = system;
  this->core = core;
}

using namespace beethoven;

bool runtime_verbose = false;


cmd_server_file *csf;

pthread_mutex_t cmdserverlock = PTHREAD_MUTEX_INITIALIZER;
std::queue<beethoven::rocc_cmd> cmds;
std::unordered_map<system_core_pair, std::queue<int> *> in_flight;

constexpr int num_cmd_beats = 5;

static void *cmd_server_f(void *) {
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] Starting command server thread" << std::endl;
  }
  setup_mmio();
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] MMIO setup completed" << std::endl;
  }
  // map in the shared file
  int fd_beethoven = shm_open(cmd_server_file_name().c_str(), O_CREAT | O_RDWR, file_access_flags);
  if (fd_beethoven < 0) {
    printf("Failed to initialize cmd_file '%s'\n%s\n", cmd_server_file_name().c_str(), strerror(errno));
    exit(errno);
  } else {
    LOG(printf("Successfully intialized cmd_file at %s\n", cmd_server_file_name().c_str()));
    if (runtime_verbose) {
      std::cout << "[CMD_SERVER] Opened shared memory: " << cmd_server_file_name() << std::endl;
    }
  }
  // check the file size. It might already exist in which case we don't need to truncate it again
  struct stat shm_stats{};
  fstat(fd_beethoven, &shm_stats);
  if (shm_stats.st_size < sizeof(cmd_server_file)) {
    if (runtime_verbose) {
      std::cout << "[CMD_SERVER] Truncating shared memory to " << sizeof(cmd_server_file) << " bytes" << std::endl;
    }
    int tr_rc = ftruncate(fd_beethoven, sizeof(cmd_server_file));
    if (tr_rc) {
      std::cerr << "Failed to truncate cmd_server file" << std::endl;
      throw std::exception();
    }
  }

  auto &addr = *(cmd_server_file *) mmap(nullptr, sizeof(cmd_server_file), file_access_prots,
                                         MAP_SHARED, fd_beethoven, 0);
  csf = &addr;
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] Memory mapped successfully" << std::endl;
  }
  // we need to initialize it! This used to be a race condition, where the cmd_server thread was racing against the
  // poller thread to get to the file. The poller often won, found old dat anad mucked everything up :(
  cmd_server_file::init(addr);
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] Command server file initialized" << std::endl;
  }
#ifndef SIM
  response_poller::start_poller(&addr.processes_waiting);
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] Response poller started" << std::endl;
  }
#endif

  std::vector<std::pair<int, FILE *>> alloc;
  pthread_mutex_lock(&addr.server_mut);
  if (runtime_verbose) {
    std::cout << "[CMD_SERVER] Command server ready on file " << cmd_server_file_name() << std::endl;
  }
  pthread_mutex_lock(&addr.server_mut);
  while (true) {
#ifdef VERBOSE
    std::cerr << "Got Command in Server" << std::endl << std::endl;
#endif
    if (runtime_verbose) {
      std::cout << "[CMD_SERVER] Received command" << std::endl;
    }
    auto start = std::chrono::high_resolution_clock::now();
    // allocate space for response
    int id;
    // dont' process FLUSH commands on FPGA, they're only used
    // for simulation right now. Use in future maybe. But they
    // may be illegal if we consider the process isolation
    if (addr.cmd.getXd()) {
      pthread_mutex_lock(&addr.free_list_lock);
      id = addr.free_list[addr.free_list_idx];
      addr.free_list_idx--;
      pthread_mutex_unlock(&addr.free_list_lock);

      if (runtime_verbose) {
        std::cout << "[CMD_SERVER] Allocated response ID: " << id << std::endl;
      }
      // return response handle to client
      addr.pthread_wait_id = id;
      // end return response handle to client
    } else {
      addr.pthread_wait_id = id = 0xffff;
      if (runtime_verbose) {
        std::cout << "[CMD_SERVER] Command does not expect response (fire-and-forget)" << std::endl;
      }
    }
    pthread_mutex_lock(&cmdserverlock);
    if (addr.quit) {
      if (runtime_verbose) {
        std::cout << "[CMD_SERVER] Quit command received, shutting down" << std::endl;
      }
      pthread_mutex_unlock(&main_lock);
#ifdef SIM
      kill_sig = true;
#ifdef USE_VCS
      vpi_control(vpiFinish);
#endif
#endif
      return nullptr;
    }
#if defined(FPGA) || defined(VSIM)
#if AWS or defined(Kria)
      // wake up response poller if this command expects a response
      if (addr.cmd.getXd()) {
        sem_post(&addr.processes_waiting);
        if (runtime_verbose) {
          std::cout << "[CMD_SERVER] Waking response poller" << std::endl;
        }
      }
      pthread_mutex_lock(&bus_lock);
#endif
      uint32_t pack[5];
      addr.cmd.pack(pack_cfg, pack);
      if (runtime_verbose) {
	printf("[CMD_SERVER] Packed command: system_id=%d, core_id=%d\n", addr.cmd.getSystemId(), addr.cmd.getCoreId());
	fflush(stdout);
      }
      //    if (sizeof(pack[0]) > 64) {
      //      printf("FAILURE - cannot use peek-poke give the current ");
      //      exit(1);
      //    }
      for (int i = 0; i < num_cmd_beats; ++i) {// command is 5 32-bit payloads
        while (!peek_mmio(CMD_READY)) {}
        poke_mmio(CMD_BITS, pack[i]);
        poke_mmio(CMD_VALID, 1);
      }
      if (runtime_verbose) {
        std::cout << "[CMD_SERVER] Command sent to hardware (" << num_cmd_beats << " beats)" << std::endl;
      }
#endif
    LOG(std::cerr << "Successfully delivered command\n"
                  << std::endl);
#if AWS or defined(Kria)
    pthread_mutex_unlock(&bus_lock);
#else
    // sim only
    cmds.push(addr.cmd);
    if (runtime_verbose) {
      std::cout << "[CMD_SERVER] Command queued for simulation" << std::endl;
    }
#endif
    // let main thread know how to return result
    if (addr.cmd.getXd()) {
      const auto key = system_core_pair(addr.cmd.getSystemId(), addr.cmd.getCoreId());
      auto &m = in_flight;
      std::queue<int> *q;
      auto iterator = m.find(key);
      if (iterator == m.end()) {
        q = new std::queue<int>;
        m[key] = q;
      } else
        q = iterator->second;
      assert(id != 0xffff);
      q->push(id);
    }

    LOG(auto end = std::chrono::high_resolution_clock::now();
                std::cerr << "Command submission took "
                          << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "µs"
                          << std::endl);
    pthread_mutex_unlock(&addr.cmd_recieve_server_resp_lock);
    pthread_mutex_unlock(&cmdserverlock);
    // re-lock self to stall
    pthread_mutex_lock(&addr.server_mut);
  }
  munmap(&addr, sizeof(cmd_server_file));
  shm_unlink(cmd_server_file_name().c_str());

}


cmd_server::~cmd_server() {
  munmap(&csf, sizeof(cmd_server_file));
  shm_unlink(cmd_server_file_name().c_str());
}

void cmd_server::start() {
  pthread_t thread;
  pthread_create(&thread, nullptr, cmd_server_f, nullptr);
}

void register_reponse(uint32_t *r_buffer) {
  beethoven::rocc_response r(r_buffer, pack_cfg);
  system_core_pair pr(r.system_id, r.core_id);
  pthread_mutex_lock(&cmdserverlock);
  auto it = in_flight.find(pr);
  if (it == in_flight.end()) {
    fprintf(stderr, "Error: Got bad response from Sys(%d) Core(%d)\n", r.system_id, r.core_id);
    fflush(stderr);
#ifdef USE_VCS
#ifdef SIM
      vpi_control(vpiFinish);
#endif
#endif
    pthread_mutex_unlock(&cmdserverlock);
    pthread_mutex_unlock(&main_lock);
  } else {
    int id = in_flight[pr]->front();
    if (runtime_verbose) {
      std::cout << "[CMD_SERVER] Registered response: ID=" << id
                << ", Sys=" << r.system_id << ", Core=" << r.core_id
                << ", Data=0x" << std::hex << r.data << std::dec << std::endl;
    }
    csf->responses[id] = r;
    // allow client thread to access response
    pthread_mutex_unlock(&csf->wait_for_response[id]);
    in_flight[pr]->pop();
    pthread_mutex_unlock(&cmdserverlock);
  }
}
