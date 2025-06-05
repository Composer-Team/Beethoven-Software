//
// Created by Christopher Kjellqvist on 7/8/24.
//

#ifndef BEETHOVENRUNTIME_STATE_MACHINE_H
#define BEETHOVENRUNTIME_STATE_MACHINE_H


#include <cinttypes>
#include <pthread.h>
#include <map>
#include <queue>
#include "sim/tick.h"
#include "cmd_server.h"
#include "sim/DataWrapper.h"
#include "sim/mem_ctrl.h"
#include "util.h"

extern pthread_mutex_t cmdserverlock;
extern std::queue<beethoven::rocc_cmd> cmds;
extern std::unordered_map<system_core_pair, std::queue<int> *> in_flight;
extern pthread_mutex_t main_lock;
extern uint64_t memory_transacted;
extern bool kill_sig;
extern uint64_t main_time;
extern int cmds_inflight;
#if NUM_DDR_CHANNELS >= 1
extern mem_intf_t axi4_mems[NUM_DDR_CHANNELS];
#endif
static int cmd_ctr = 0;

enum cmd_transfer_state {
  CMD_INACTIVE,
  CMD_BITS_WRITE_ADDR,
  CMD_BITS_WRITE_DAT,
  CMD_BITS_WRITE_B,
  CMD_VALID_ADDR,
  CMD_VALID_DAT,
  CMD_VALID_WRITE_B,
  CMD_RECHECK_READY_ADDR,
  CMD_RECHECK_READY_DAT,
};

enum resp_transfer_state {
  RESPT_INACTIVE,
  RESPT_BITS_ADDR,
  RESPT_BITS_READ,
  RESPT_READY_ADDR,
  RESPT_READY_WRITE,
  RESPT_READY_WRITE_B,
  RESPT_RECHECK_VALID_ADDR,
  RESPT_RECHECK_VALID_READ,
};

enum update_state {
  UPDATE_IDLE_RESP,
  UPDATE_IDLE_CMD,
  UPDATE_CMD_ADDR,
  UPDATE_CMD_WAIT,
  UPDATE_RESP_ADDR,
  UPDATE_RESP_WAIT
};


struct command_transaction {
  uint32_t cmdbuf[5] = {};
  int8_t progress = 0;
  int id = 0;
  cmd_transfer_state state = CMD_INACTIVE;
  bool ready_for_command = false;

  static const int payload_length = 5;
};

struct response_transaction {
  static const int payload_length = 3;
  uint32_t resbuf[payload_length]{};
  uint8_t progress = 0;

  resp_transfer_state state = RESPT_INACTIVE;
};

template<typename byte_t, typename addr_t, typename data_t>
struct AXIControlIntf : public ControlIntf {
  byte_t aw_valid, aw_ready, ar_ready, w_valid, w_ready, r_ready, ar_valid, b_ready, b_valid, r_valid;
  addr_t aw_addr, ar_addr;
  data_t w_data, r_data;

  std::map<std::tuple<int, int>, unsigned long long> start_times;

  void set_aw(byte_t valid, byte_t ready, addr_t addr) {
    aw_valid = valid;
    aw_ready = ready;
    aw_addr = addr;
  }

  void set_w(byte_t valid, byte_t ready, data_t data) {
    w_valid = valid;
    w_ready = ready;
    w_data = data;
  }

  void set_r(byte_t ready, byte_t valid, data_t data) {
    r_ready = ready;
    r_data = data;
    r_valid = valid;
  }

  void set_ar(byte_t valid, byte_t ready, addr_t addr) {
    ar_valid = valid;
    ar_ready = ready;
    ar_addr = addr;
  }

  void set_b(byte_t ready, byte_t valid) {
    b_ready = ready;
    b_valid = valid;
  }

  void tick() override {
    aw_valid.set(0);
    w_valid.set(0);
    ar_valid.set(0);
    b_ready.set(0);
    r_ready.set(0);

    update_command_state();
    update_resp_state();
    update_update_state();
  }

  const int check_freq = 50;
  int check_freq_ctr = 0;

  command_transaction ongoing_cmd;
  response_transaction ongoing_rsp;
  update_state ongoing_update = UPDATE_IDLE_CMD;
  unsigned long long time_last_command = 0;
  bool bus_occupied = false;
  
  void update_command_state() {
    switch (ongoing_cmd.state) {
      // tell the beethoven that we're going to send 32-bits of a command over the PCIE bus
      case CMD_BITS_WRITE_ADDR:
        ongoing_cmd.ready_for_command = false;
        aw_valid.set(1);
        aw_addr.set(CMD_BITS);
        if (aw_ready.get(0)) {
          ongoing_cmd.state = CMD_BITS_WRITE_DAT;
          //          printf("to write dat\n");
        }
        break;
        // send the command over the PCIE bus
      case CMD_BITS_WRITE_DAT:
        w_valid.set(1);
        w_data.set(ongoing_cmd.cmdbuf[ongoing_cmd.progress]);
        if (w_ready.get(0)) {
          ongoing_cmd.state = CMD_BITS_WRITE_B;
        }
        break;
      case CMD_BITS_WRITE_B:
        b_ready.set(1);
        if (b_valid.get(0)) {
          ongoing_cmd.state = CMD_VALID_ADDR;
        }
        break;
        // We just send the 32-bits, now "simulate" the decoupled interface by toggling to the CMD_VALID bit to 1
        // This bit is visible from the CMD_VALID bit, so we need to perform an AXI transaction
      case CMD_VALID_ADDR:
        aw_valid.set(1);
        aw_addr.set(CMD_VALID);
        if (aw_ready.get(0)) {
          ongoing_cmd.state = CMD_VALID_DAT;
        }
        break;
        // send the CMD_VALID bit
      case CMD_VALID_DAT:
        w_valid.set(1);
        w_data.set(1);
        if (w_ready.get(0)) {
          ongoing_cmd.state = CMD_VALID_WRITE_B;
        }
        break;
      case CMD_VALID_WRITE_B:
        b_ready.set(1);
        if (b_valid.get(0)) {
          ongoing_cmd.progress++;
          // send last thing, yield bus
          if (ongoing_cmd.progress == command_transaction::payload_length) {
#if NUM_DDR_CHANNELS >= 1
            for (auto &axi_mem: axi4_mems) {
              axi_mem.mem_sys->ResetStats();
              time_last_command = main_time;
              memory_transacted = 0;
            }
#endif
            ongoing_cmd.state = CMD_INACTIVE;
            bus_occupied = false;
          } else {
            // else, need to send the next 32-bit chunk and see that the channel is "ready"
            ongoing_cmd.state = CMD_RECHECK_READY_ADDR;
          }
        }
        break;
        // We just send 32-bits over the interface, check if it's ready for another 32b by requesting ready from the
        // CMD_READY bit
      case CMD_RECHECK_READY_ADDR:
        ar_valid.set(1);
        ar_addr.set(CMD_READY);
        if (ar_ready.get(0)) {
          ongoing_cmd.state = CMD_RECHECK_READY_DAT;
        }
        break;
        // read the CMD_READY bit
      case CMD_RECHECK_READY_DAT:
        r_ready.set(1);
        if (r_valid.get(0)) {
          // if it's ready for another command
          if (r_data.get(0)) {
            ongoing_cmd.state = CMD_BITS_WRITE_ADDR;
          } else {
            ongoing_cmd.state = CMD_RECHECK_READY_ADDR;
          }
        }
        break;
      case CMD_INACTIVE:
        if (ongoing_cmd.ready_for_command &&
            !bus_occupied &&
            (ongoing_update == UPDATE_IDLE_CMD || ongoing_update == UPDATE_IDLE_RESP)) {
          pthread_mutex_lock(&cmdserverlock);
          if (not cmds.empty()) {
            printf("enqueueing command: %d\n", cmd_ctr++);
            bus_occupied = true;
            ongoing_cmd.state = CMD_BITS_WRITE_ADDR;
            if (cmds.front().getXd()) {
              auto id = std::tuple<int, int>(cmds.front().getSystemId(), cmds.front().getCoreId());
              start_times[id] = main_time;
            }
            cmds.front().pack(pack_cfg, ongoing_cmd.cmdbuf);
            kill_sig = cmds.front().getOpcode() == ROCC_OP_FLUSH;
            ongoing_cmd.progress = 0;
            if (cmds.front().getXd() == 1)
              cmds_inflight++;
            cmds.pop();
          } else {
            fflush(stdout);
          }
          pthread_mutex_unlock(&cmdserverlock);
        }
        break;
    }

  }

  void update_resp_state() {
    switch (ongoing_rsp.state) {
      case RESPT_INACTIVE:
        break;
      case RESPT_BITS_ADDR:
        ar_addr.set(RESP_BITS);
        ar_valid.set(1);
        if (ar_ready.get(0)) {
          ongoing_rsp.state = RESPT_BITS_READ;
        }
        break;
      case RESPT_BITS_READ:
        r_ready.set(1);
        if (r_valid.get(0)) {
          ongoing_rsp.resbuf[ongoing_rsp.progress++] = r_data.get(0);
          ongoing_rsp.state = RESPT_READY_ADDR;
        }
        break;
      case RESPT_READY_ADDR:
        aw_valid.set(1);
        aw_addr.set(RESP_READY);

        if (aw_ready.get(0)) {
          ongoing_rsp.state = RESPT_READY_WRITE;
        }
        break;
      case RESPT_READY_WRITE:
        w_valid.set(1);
#if AXIL_BUS_WIDTH > 64
#error "AXIL_BUS_WIDTH > 64 not supported"
#endif
        w_data.set(1);
        if (w_ready.get(0)) {
          ongoing_rsp.state = RESPT_READY_WRITE_B;
        }
      case RESPT_READY_WRITE_B:
        b_ready.set(1);
        if (b_valid.get(0)) {
          if (ongoing_rsp.progress == response_transaction::payload_length) {
            beethoven::rocc_response r(ongoing_rsp.resbuf, pack_cfg);
            auto id = std::tuple<int, int>(r.system_id, r.core_id);
            auto start = start_times[id];
            LOG(printf("Command took %f ms\n", float((main_time - start)) / 1000 / 1000 / 1000));
            register_reponse(ongoing_rsp.resbuf);
            cmds_inflight--;
            bus_occupied = false;
            //              printf("respt-ready-b -> respt-inactive\n");
            ongoing_rsp.state = RESPT_INACTIVE;
          } else {
            //              printf("Not done yet %d/%d. respt-ready-b -> respt-recheck-valid-address\n", ongoing_rsp.progress,
            //                     response_transaction::payload_length);
            ongoing_rsp.state = RESPT_RECHECK_VALID_ADDR;
          }
        }
        break;
      case RESPT_RECHECK_VALID_ADDR:
        ar_valid.set(1);
        ar_addr.set(RESP_VALID);
        if (ar_ready.get(0)) {
          ongoing_rsp.state = RESPT_RECHECK_VALID_READ;
        }
        break;
      case RESPT_RECHECK_VALID_READ:
        r_ready.set(1);
        if (r_valid.get(0)) {
          if (r_data.get(0)) {
            ongoing_rsp.state = RESPT_BITS_ADDR;
          } else {
            ongoing_rsp.state = RESPT_RECHECK_VALID_ADDR;
          }
        }
        break;
    }

  }

  void update_update_state() {
    switch (ongoing_update) {
      case UPDATE_IDLE_RESP:
        if (!bus_occupied && check_freq_ctr > check_freq) {
          check_freq_ctr = 0;
          bus_occupied = true;
          ongoing_update = UPDATE_RESP_ADDR;
        } else {
          check_freq_ctr++;
        }
        break;
      case UPDATE_RESP_ADDR:
        ar_valid.set(1);
        ar_addr.set(RESP_VALID);
        if (ar_ready.get(0)) {
          ongoing_update = UPDATE_RESP_WAIT;
        }
        break;
      case UPDATE_RESP_WAIT:
        r_ready.set(1);
        if (r_valid.get(0)) {
          ongoing_rsp.progress = 0;
          if (r_data.get(0)) {
            LOG(printf("Found valid response on cycle %lu!!!\n", main_time));
            ongoing_rsp.state = RESPT_BITS_ADDR;
          } else {
            bus_occupied = false;
          }
          ongoing_update = UPDATE_IDLE_CMD;
        }
        break;
      case UPDATE_IDLE_CMD:
        if (!bus_occupied && check_freq_ctr > check_freq) {
          check_freq_ctr = 0;
          bus_occupied = true;
          ongoing_update = UPDATE_CMD_ADDR;
        } else {
          check_freq_ctr++;
        }
        break;
      case UPDATE_CMD_ADDR:
        ar_valid.set(1);
        ar_addr.set(CMD_READY);
        if (ar_ready.get(0)) {
          ongoing_update = UPDATE_CMD_WAIT;
        }
        break;
      case UPDATE_CMD_WAIT:
        r_ready.set(1);
        if (r_valid.get(0)) {
          ongoing_cmd.ready_for_command = r_data.get(0);
          if (cmds_inflight > 0) {
            ongoing_update = UPDATE_IDLE_RESP;
          } else {
            ongoing_update = UPDATE_IDLE_CMD;
          }
        }
        bus_occupied = false;
        break;
    }
  }
};



#endif //BEETHOVENRUNTIME_STATE_MACHINE_H
