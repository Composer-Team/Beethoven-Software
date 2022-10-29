//
// Created by Chris Kjellqvist on 10/19/22.
//


#ifndef COMPOSER_RESPONSE_HANDLE_H
#define COMPOSER_RESPONSE_HANDLE_H
#include <cinttypes>
#include <composer/rocc_response.h>
#include <iostream>

namespace composer {

  class fpga_handle_t;

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
