//
// Created by Chris Kjellqvist on 10/19/22.
//


#ifndef BEETHOVEN_RESPONSE_HANDLE_H
#define BEETHOVEN_RESPONSE_HANDLE_H

#include <cinttypes>
#ifndef BAREMETAL
#include <iostream>
#include <functional>
#include <optional>
#endif

#include "rocc_response.h"
//#include "include/beethoven/allocator/alloc.h"

namespace beethoven {

  class fpga_handle_t;

  class response_getter {
    // don't have room (or time) for this in baremetal. just handle it right
#ifndef BAREMETAL
    bool can_wait;
    bool has_recieved = false;
    const fpga_handle_t *h;
    int id;
#else
    uint8_t id;
#endif
  public:
#ifndef BAREMETAL
    explicit response_getter(bool cw, int id, const fpga_handle_t &h) : id(id), can_wait(cw), h(&h) {};
#else

    explicit response_getter(uint8_t id) : id(id) {};
#endif

    // copy constructor invalidates source
    response_getter(const response_getter &mv) {
#ifndef BAREMETAL
      can_wait = mv.can_wait;
      has_recieved = mv.has_recieved;
      h = mv.h;
#endif
      id = mv.id;
    }

    response_getter() = default;

    rocc_response get();

#ifndef BAREMETAL
    std::optional<rocc_response> try_get();
#endif
  };

  template<typename t>
  class response_handle {
    template<class U> friend
    class response_handle;

  private:
    response_getter rg;

    template<class U>
    explicit response_handle(const response_handle<U> &other) : rg(other.rg) {}

  public:
#ifdef BAREMETAL

    explicit response_handle(uint8_t id) : rg(response_getter(id)) {}

#else
    explicit response_handle(bool cw, int id, const fpga_handle_t &h) :
            rg(cw, id, h) {}
#endif

    response_handle() = default;

    template<typename s>
    response_handle<s> to() {
      return response_handle<s>(*this);
    }

    t get();

#ifndef BAREMETAL
    std::optional<t> try_get();
#endif
  };

  template<>
  rocc_response response_handle<rocc_response>::get();

#ifndef BAREMETAL
  template<>
  std::optional<rocc_response> response_handle<rocc_response>::try_get();
#endif

}

#ifndef BAREMETAL
std::ostream &operator<<(std::ostream &os, const beethoven::rocc_response &response);
#endif

#endif //BEETHOVEN_RESPONSE_HANDLE_H
