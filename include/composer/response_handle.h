//
// Created by Chris Kjellqvist on 10/19/22.
//
#include <cinttypes>
#include "fpga_handle.h"

#ifndef COMPOSER_RESPONSE_HANDLE_H
#define COMPOSER_RESPONSE_HANDLE_H


namespace composer {

  class response_handle {
    bool can_wait, has_recieved = false;
    int id;
    const fpga_handle_t *h;
  public:
    explicit response_handle(bool cw, int id, const fpga_handle_t &h) : id(id), can_wait(cw), h(&h) {}

    rocc_response get();
  };
}

std::ostream &operator<<(std::ostream &os, const composer::rocc_response &response);

#endif //COMPOSER_RESPONSE_HANDLE_H
