#ifndef SIM_DATAWRAPPER_H
#define SIM_DATAWRAPPER_H

#include "sim/DataWrapper.h"
#include <cstring>

#define default_get return *ptr
template<>
uint64_t GetSetWrapper<uint64_t>::get(int idx) const {
  if (idx == 0) return *ptr & 0xFFFFFFFF;
  else
    return *ptr >> 32;
}

template<>
uint32_t GetSetWrapper<uint32_t>::get(int idx) const { return *ptr; }

template<>
uint16_t GetSetWrapper<uint16_t>::get(int idx) const { return *ptr; }

template<>
uint8_t GetSetWrapper<uint8_t>::get(int idx) const { return *ptr; }

#endif