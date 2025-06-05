//
// Created by Christopher Kjellqvist on 8/27/24.
//

#include "sim/chipkit/util.h"
#include <cassert>

static void push_val(const char buf[4], std::queue<unsigned char> &vec) {
  for (int i = 0; i < 4; ++i) {
    vec.push(buf[i]);
  }
}


static void push_val(const uint32_t val, std::queue<unsigned char> &vec) {
  auto buf = (uint8_t * ) & val;
  for (int i = 0; i < 4; ++i) {
    vec.push(buf[i]);
  }
}

void ChipKit::readMemFile2ChipkitDMA(std::queue<unsigned char> &vec, const std::string &fname) {
  FILE *f = fopen(fname.c_str(), "r");
  char buf[256];
  char c;
  int32_t addr = 0;

  char inst_buf[4];
  char inst_idx = 0;
  while ((c = getc(f)) != EOF) {
    if (isspace(c)) while (isspace(c = getc(f)));
    if (c != '@') ungetc(c, f);
    if (c == EOF) break;
    if (c == -1) break;
    switch (c) {
      case '@':
        fscanf(f, "%s", buf);
        addr = strtol(buf, nullptr, 16);
        break;
      default: {
        auto q = getc(f);
        ungetc(q, f);
        if (q == '@') continue;
        fscanf(f, "%s", buf);
        inst_buf[inst_idx] = (char) strtol(buf, nullptr, 16);
        if (++inst_idx == 4) {
          vec.push('w');
          push_val(addr, vec);
          push_val(inst_buf, vec);

          printf("pushing %x %02x %02x %02x %02x\n", addr, (char) inst_buf[0], (char) inst_buf[1], (char) inst_buf[2],
                 (char) inst_buf[3]);
          vec.push(0xa);
          inst_idx = 0;
          addr += 4;
        }
        break;
      }
    }
  }
  assert(inst_idx == 0);
}

