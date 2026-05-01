//
// Created by Christopher Kjellqvist on 8/5/24.
//

#ifndef BEETHOVENRUNTIME_TICK_H
#define BEETHOVENRUNTIME_TICK_H

struct ControlIntf {
  virtual void tick() = 0;
};

void tick_signals(ControlIntf *ctrl);

#endif //BEETHOVENRUNTIME_TICK_H
