//
// Created by Christopher Kjellqvist on 2/2/24.
//

#ifndef BEETHOVENRUNTIME_TRACE_READ_H
#define BEETHOVENRUNTIME_TRACE_READ_H

#include <cinttypes>
#include <functional>
#include <queue>
#include <string>

enum TraceType {
  ReadConditionType,
  WriteType,
  Comment
};


struct TraceUnit {
  TraceType ty;
  uint64_t address;
  uint32_t payload;
  TraceUnit(TraceType ty, uint64_t address, uint32_t payload);
};


typedef std::queue<TraceUnit> Trace;

void init_trace(const std::string &fname);




#endif//BEETHOVENRUNTIME_TRACE_READ_H
