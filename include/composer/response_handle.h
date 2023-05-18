//
// Created by Chris Kjellqvist on 10/19/22.
//


#ifndef COMPOSER_RESPONSE_HANDLE_H
#define COMPOSER_RESPONSE_HANDLE_H

#include <cinttypes>
#include <composer/rocc_response.h>
#include <iostream>
#include <functional>

namespace composer {

  class fpga_handle_t;

  class response_getter {
    bool can_wait, has_recieved = false;
    int id;
    const fpga_handle_t *h;
  public:
    explicit response_getter(bool cw, int id, const fpga_handle_t &h) : id(id), can_wait(cw), h(&h) {}
    // copy constructor invalidates source
    response_getter (response_getter &mv) {
      can_wait = mv.can_wait;
      mv.can_wait = false;
      has_recieved = mv.has_recieved;
      id = mv.id;
      h = mv.h;
    }

    rocc_response get();
  };

  template<typename t>
  class response_handle {
    template <class U>
    friend class response_handle;
  private:
    response_getter rg;
    explicit response_handle(response_getter &other) : rg(other) {}
  public:
    explicit response_handle(bool cw, int id, const fpga_handle_t &h) : rg(cw, id, h) {}
    template<typename s>
    response_handle<s> to() {
      return response_handle<s>(rg);
    }
    t get();
  };

  template<> rocc_response response_handle<rocc_response>::get();
}

std::ostream &operator<<(std::ostream &os, const composer::rocc_response &response);

#endif //COMPOSER_RESPONSE_HANDLE_H
