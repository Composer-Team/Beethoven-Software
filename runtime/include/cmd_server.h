//
// Created by Chris Kjellqvist on 9/29/22.
//
#ifndef BEETHOVEN_VERILATOR_CMD_SERVER_H
#define BEETHOVEN_VERILATOR_CMD_SERVER_H

#include <queue>
#include <unordered_map>
#include <beethoven/verilator_server.h>

extern beethoven::cmd_server_file *csf;

struct system_core_pair {
  int system;
  int core;
  system_core_pair(int system, int core);
  bool operator==(const system_core_pair &other) const {
    return system == other.system and core == other.core;
  }
};

template <>
struct std::hash<system_core_pair> {
  std::size_t operator()(const system_core_pair& k) const
  {
    return k.core << 8 | k.system;
  }

};

struct cmd_server {
  static void start();
  ~cmd_server();
private:
  pthread_t thread;
};

void register_reponse(uint32_t *r_buffer);

#endif //BEETHOVEN_VERILATOR_CMD_SERVER_H
