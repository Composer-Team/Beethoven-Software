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
#include <optional>


namespace composer {

  class fpga_handle_t;

  class response_getter {
    // don't have room (or time) for this in baremetal. just handle it right
#ifndef BAREMETAL
    bool can_wait;
    bool has_recieved = false;
    const fpga_handle_t *h;
#endif
    int id;
  public:
    explicit response_getter(bool cw, int id, const fpga_handle_t &h) : id(id)
#ifndef BAREMETAL
                              , can_wait(cw), h(&h)
#endif
    { };

    // copy constructor invalidates source
    response_getter(response_getter &mv) {
#ifndef BAREMETAL
      can_wait = mv.can_wait;
      mv.can_wait = false;
      has_recieved = mv.has_recieved;
      h = mv.h;
#endif
      id = mv.id;
    }

    rocc_response get();

    std::optional<rocc_response> try_get();
  };

  template<typename t>
  class response_handle {
    template<class U> friend
    class response_handle;

  private:
    response_getter rg;

    template<class U>
    explicit response_handle(response_handle<U> &other) : rg(other.rg) {}

  public:
    explicit response_handle(bool cw, int id, const fpga_handle_t &h) :
            rg(cw, id, h) {}

    template<typename s>
    response_handle<s> to() {
      return response_handle<s>(*this);
    }

    t get();

    std::optional<t> try_get();
  };

  template<>
  rocc_response response_handle<rocc_response>::get();

  template<>
  std::optional<rocc_response> response_handle<rocc_response>::try_get();
}

std::ostream &operator<<(std::ostream &os, const composer::rocc_response &response);

#endif //COMPOSER_RESPONSE_HANDLE_H
