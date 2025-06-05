//
// Created by Chris Kjellqvist on 2/23/23.
//

#ifndef BEETHOVENRUNTIME_UTIL_H
#define BEETHOVENRUNTIME_UTIL_H

constexpr int roundUp(float q) {
  float d = q - (int) q;
  if (q < 0) {
    return q - d;
  } else {
    return q + d;
  }
}


// if we're not running in verbose mode, just turn the print into a string and forget about it...
#ifndef VERBOSE
#define LOG(x) (#x)
#else
#define LOG(x) x
#endif

#endif //BEETHOVENRUNTIME_UTIL_H
