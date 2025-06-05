//
// Created by Christopher Kjellqvist on 8/27/24.
//

#ifndef BEETHOVENRUNTIME_CHIPKIT_UTIL_H
#define BEETHOVENRUNTIME_CHIPKIT_UTIL_H

#include <queue>
#include <string>

namespace ChipKit {
  void readMemFile2ChipkitDMA(std::queue<unsigned char> &vec, const std::string &fname);
}

#endif //BEETHOVENRUNTIME_UTIL_H
