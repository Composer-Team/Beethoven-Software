//
// Created by Chris Kjellqvist on 10/19/22.
//


#ifndef COMPOSER_RESPONSE_HANDLE_H
#define COMPOSER_RESPONSE_HANDLE_H

#include <cinttypes>
#include <composer/rocc_response.h>
#include <iostream>
#include <functional>
#include <composer/alloc.h>

namespace composer {

  class fpga_handle_t;

  class response_getter {
    bool can_wait, has_recieved = false;
    uint64_t id;
    const fpga_handle_t *h;
  public:
    explicit response_getter(bool cw, int id, const fpga_handle_t &h) : id(id), can_wait(cw), h(&h) {}

    // copy constructor invalidates source
    response_getter(response_getter &mv) {
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
    template<class U> friend class response_handle;

  private:
    response_getter rg;
    std::vector<remote_ptr> ops;
    explicit response_handle(response_getter &other, const std::vector<remote_ptr> &mem_ops) : rg(other) {}

  public:
    explicit response_handle(bool cw, uint64_t id, const fpga_handle_t &h, const std::vector<remote_ptr> &mem_ops = {}) :
            rg(cw, id, h), ops(mem_ops) {}

    template<typename s>
    response_handle<s> to() {
      return response_handle<s>(rg, ops);
    }

    t get();
  };

  template<>
  rocc_response response_handle<rocc_response>::get();
}

std::ostream &operator<<(std::ostream &os, const composer::rocc_response &response);

#endif //COMPOSER_RESPONSE_HANDLE_H
