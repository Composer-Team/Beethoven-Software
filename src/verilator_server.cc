//
// Created by Chris Kjellqvist on 11/14/22.
//

#include "beethoven/verilator_server.h"
#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

using namespace beethoven;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
std::string whoami() {
  // call to get the identity of the current process
  // get stdout of whoami
  // return the string
  if (geteuid() == 0) return "ROOT";
  int pid = fork();
  if (pid == 0) {
    // child
    int fd = open("/tmp/whoami", O_CREAT | O_RDWR, 0666);
    dup2(fd, 1);
    close(fd);
    execlp("whoami", "whoami", nullptr);
  } else {
    // parent
    waitpid(pid, nullptr, 0);
    int fd = open("/tmp/whoami", O_RDONLY);
    char buf[1024];
    memset(buf, 0, 1024);
    read(fd, buf, 1024);
    close(fd);
    return std::string(buf);
  }
}
#pragma clang diagnostic pop

static const std::string USER_NAME = whoami();

std::string beethoven::cmd_server_file_name() {
   return "/compo_c_" + USER_NAME;
}
std::string beethoven::data_server_file_name() {
   return "/compo_d_" + USER_NAME;
}

void cmd_server_file::init(cmd_server_file &csf) {

  pthread_mutexattr_t attrs;
  pthread_mutexattr_init(&attrs);
  pthread_mutexattr_setpshared(&attrs, PTHREAD_PROCESS_SHARED);

  pthread_mutex_init(&csf.server_mut, &attrs);
  pthread_mutex_init(&csf.cmd_recieve_server_resp_lock, &attrs);
  pthread_mutex_init(&csf.cmd_send_lock, &attrs);
  for (unsigned i = 0; i < MAX_CONCURRENT_COMMANDS; ++i) { // NOLINT(modernize-loop-convert)
    pthread_mutex_init(&csf.wait_for_response[i], &attrs);
    pthread_mutex_lock(&csf.wait_for_response[i]);
  }
  pthread_mutex_init(&csf.free_list_lock, &attrs);
  pthread_mutex_lock(&csf.cmd_recieve_server_resp_lock);

// wait_responses are all initially available
  for (int i = 0; i < MAX_CONCURRENT_COMMANDS; ++i) csf.free_list[i] = i;
  csf.free_list_idx = 255;
  csf.quit = false;

  sem_init(&csf.processes_waiting, 0, 0);
}

void data_server_file::init(data_server_file &dsf) {
  pthread_mutexattr_t attrs;
  pthread_mutexattr_init(&attrs);
  pthread_mutexattr_setpshared(&attrs, PTHREAD_PROCESS_SHARED);

  pthread_mutex_init(&dsf.server_mut, &attrs);
  pthread_mutex_init(&dsf.data_cmd_recieve_resp_lock, &attrs);
  pthread_mutex_lock(&dsf.data_cmd_recieve_resp_lock);
  pthread_mutex_init(&dsf.data_cmd_send_lock, &attrs);
  memset(dsf.fname, 0, 512);
  dsf.op_argument = 0;
}
