//
// Created by Christopher Kjellqvist on 8/25/24.
//

#ifndef BEETHOVENRUNTIME_STATE_MACHINE_H
#define BEETHOVENRUNTIME_STATE_MACHINE_H

#include "sim/tick.h"
#include "sim/DataWrapper.h"

const int baud_table[] = {1302, 217, 108, 54, 27, 22, 20, 19, 16, 15, 10, 8, 6, 5, 4, 2};
extern unsigned int baud_sel;
extern int baud_div;

template<typename byte_t>
struct ChipkitControlIntf : public ControlIntf {
  enum uart_state_t {
    IDLE, START_BAUD, START, BITS, BITS_BAUD, STOP
  };


  byte_t txd;
  byte_t rxd;

  ChipkitControlIntf(byte_t txd, byte_t rxd) : txd(txd), rxd(rxd) {}
  ChipkitControlIntf() = default;

  unsigned char out_byte = 0;

  uart_state_t in_state = IDLE;
  uart_state_t out_state = IDLE;
  int baud_count_in = 0;
  int baud_count_out = 0;

  int in_byte_progress = 0;
  int out_byte_progress = 0;

  static bool test(unsigned char q, int idx) {
    return q & (1 << idx);
  }

  void set(byte_t &target, int target_idx, unsigned char src, int src_idx) {
    target.set((target.get() & ~(1 << target_idx)) | (((src & (1 << src_idx)) >> src_idx) << target_idx));
  }

  void set_u(unsigned char &target, int target_idx, byte_t src, int src_idx) {
    target = target & ~(1 << target_idx) | (((src.get() & (1 << src_idx)) >> src_idx) << target_idx);
  }

// Set Baud Rates
//|  SEL | DIV |  10  |  20  |  25  |  30  |  40  |  50  |  60  |  70  |  75  |  80  |  90  | 100  |  1000  |
//| 4'd0 | 1302|      |      |      |      |      |  9.6K|      |      |      |      |      | 19.2K|  192K  |
//| 4'd1 |  217|      |      |      |      |      | 57.6K|      |      |      |      |      |115.2K|  1.15M |
//| 4'd2 |  108|      |      | 57.6K|      |      |115.2K|      |      |      |      |      |230.4K|  2.30M |
//| 4'd3 |   54|      |      |115.2K|      |      |230.4K|      |      |      |      |      |460.8K|  4.60M |
//| 4'd4 |   27|      |      |230.4K|      |      |460.8K|      |      |      |      |      |921.6K|  9.21M |
//| 4'd5 |   22|115.2K|230.4K|      |      |460.8K|      |      |      |      |921.6K|      |      |
//| 4'd6 |   20|      |      |      |      |      |      |      |      |921.6K|1.000M|      |      |
//| 4'd7 |   19|      |      |      |      |      |      |      |921.6K|1.000M|      |      |      |
//| 4'd8 |   16|      |      |      |460.8K|      |      |921.6K|      |      |1.250M|      |1.500M|
//| 4'd9 |   15|      |      |      |      |      |      |1.000M|      |1.250M|      |1.500M|      |
//| 4'd10|   10|      |      |      |      |1.000M|1.250M|1.500M|      |      |2.000M|      |      |
//| 4'd11|    8|      |      |      |      |1.250M|1.500M|      |      |      |      |      |3.000M|
//| 4'd12|    6|      |      |1.000M|1.250M|      |2.000M|      |3.000M|3.000M|      |      |      |
//| 4'd13|    5|      |      |1.250M|1.500M|2.000M|      |3.000M|      |      |      |      |      |
//| 4'd14|    4|      |1.250M|1.500M|      |      |3.000M|      |      |      |      |      |      |
//| 4'd15|    2|1.250M|      |3.000M|      |      |      |      |      |      |      |      |      |

  std::queue<unsigned char> in_stream;
  std::queue<unsigned char> out_stream;
  bool in_enable = true;
  bool out_enable = false;

  void tick() {
    switch (in_state) {
      case IDLE:
        if (in_stream.size() && in_enable) {
          rxd.set(0); // START
          in_byte_progress = 0;
          in_state = START;
          baud_count_in++;
        } else {
          rxd.set(1);
        }
        break;
      case START:
        if ((++baud_count_in) == baud_div) {
          printf("START: %02x\n", in_stream.front());
          set(rxd, 0, in_stream.front(), in_byte_progress);
          in_byte_progress++;
          baud_count_in = 0;
          in_state = BITS_BAUD;
        }
        break;
      case BITS:
        set(rxd, 0, in_stream.front(), in_byte_progress);
        in_byte_progress++;
        baud_count_in = 0;
        if (baud_div > 1)
          in_state = BITS_BAUD;
        else {
          if (in_byte_progress == 8) {
            in_state = STOP;
            in_stream.pop();
            baud_count_in = 0;
          } else {
            in_state = BITS;
          }
        }
        break;
      case BITS_BAUD:
        if (++baud_count_in == baud_div) {
          baud_count_in = 0;
          in_state = BITS;
          if (in_byte_progress == 8) {
            in_state = STOP;
            in_stream.pop();
            baud_count_in = 0;
          }
        }
        break;
      case STOP:
        rxd.set(1);
        if(++baud_count_in == 2 * baud_div) {
          in_state = IDLE;
          baud_count_in = 0;
        }
        break;
    }
    switch (out_state) {
      case IDLE:
        if (txd.get() == 0 && out_enable) {
          out_byte=0;
          if (baud_div == 1) {
            out_state = BITS;
            out_byte_progress = 0;
          } else {
            baud_count_out = 1;
            out_state = START_BAUD;
          }
        }
        break;
      case START_BAUD:
        if (++baud_count_out == baud_div) {
          out_state = BITS;
          out_byte_progress = 0;
          baud_count_out = 0;
        }
        break;
      case BITS:
        set_u(out_byte, out_byte_progress, txd, 0);
        out_byte_progress++;
        if (baud_div == 1) {
          if (out_byte_progress == 8) {
            baud_count_out = 0;
            out_state = STOP;
          }
        } else {
          baud_count_out = 1;
          out_state = BITS_BAUD;
        }
        break;
      case BITS_BAUD:
        if (++baud_count_out == baud_div) {
          if (out_byte_progress == 8) {
            baud_count_out = 0;
            out_state = STOP;
          } else {
            out_state = BITS;
          }
        }
        baud_count_out = 0;
        break;
      case STOP:
        if (++baud_count_out == 2 * baud_div) {
          out_state = IDLE;
          out_stream.push(out_byte);
          out_byte = 0;
        }
        break;

    }
  }
};

#endif //BEETHOVENRUNTIME_STATE_MACHINE_H
