//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "beethoven/allocator/alloc.h"

using namespace beethoven;

#include <sys/mman.h>
#include <cerrno>
#include <cstring>

remote_ptr::~remote_ptr() {
  if (mutex) {
    mutex->lock();
    if (--(*count) == 0) {
      delete count;
      delete mutex;
      if(munmap((char *) host_addr - offset, len + offset)) {
        printf("munmap failed: %llx %ld %ld %s\n", host_addr, offset, len, strerror(errno));
        exit(1);
      }
    } else {
      mutex->unlock();
    }
  }
}

remote_ptr &remote_ptr::operator=(const remote_ptr &other) noexcept {
  if (this->mutex == nullptr && this != &other) {
    // assigning to default constructor
    fpga_addr = other.fpga_addr;
    host_addr = other.host_addr;
    len = other.len;
    count = other.count;
    mutex = other.mutex;
    offset = other.offset;
    if (mutex) {
      std::lock_guard<std::mutex> l2(*mutex);
      (*count)++;
    }
    return *this;
  }
  if (this != &other) {
    if (mutex) {
      mutex->lock();
      if (--(*count) == 0) {
        delete count;
        delete mutex;
        if (host_addr)
          if(munmap((char *) host_addr - offset, len + offset)) {
            printf("munmap failed: %llx %ld %ld %s\n", host_addr, offset, len, strerror(errno));
            exit(1);
          }
      } else {
        mutex->unlock();
      }
    }
  fpga_addr = other.fpga_addr;
  host_addr = other.host_addr;
  len = other.len;
  count = other.count;
  mutex = other.mutex;
  offset = other.offset;
  if (mutex) {
    std::lock_guard<std::mutex> l2(*mutex);
    (*count)++;
  }
}
  return *this;
}

beethoven::remote_ptr& beethoven::remote_ptr::operator=(beethoven::remote_ptr && other) noexcept {
  this->mutex = other.mutex;
  this->count = other.count;
  this->host_addr = other.host_addr;
  this->fpga_addr = other.fpga_addr;
  this->len = other.len;
  this->offset = other.offset;
  other.mutex = nullptr;
  other.count = nullptr;
  return *this;
}

remote_ptr::remote_ptr(const intptr_t &faddr, void *haddr, const size_t &l,
                       uint16_t *c,
                       std::mutex *m,
                       ptrdiff_t off
) noexcept:
        fpga_addr(faddr),
        host_addr(haddr), len(l),
        count(c),
        mutex(m),
        offset(off) {
  if (mutex) {
    std::lock_guard<std::mutex> lock(*mutex);
    (*count)++;
  }
}

remote_ptr::remote_ptr(const intptr_t &faddr, void *haddr, const size_t &l, ptrdiff_t off) noexcept:
        fpga_addr(faddr),
        host_addr(haddr), len(l),
        offset(off) {}



remote_ptr::remote_ptr(intptr_t fpgaAddr,
                       void *hostAddr,
                       size_t len) :
        fpga_addr(fpgaAddr),
        host_addr(hostAddr),
        len(len) {
  offset = 0;
  mutex = new std::mutex();
  count = new uint16_t(1);
}

remote_ptr remote_ptr::operator+(int q) const {
  return remote_ptr(this->fpga_addr + q,
                    (char *) (this->host_addr) + q,
                    this->len - q,
          this->count, this->mutex,
                    offset + q

  );
}

remote_ptr::remote_ptr() :
        fpga_addr(0),
        host_addr(nullptr),
        len(0),
        mutex(nullptr),
        count(nullptr),
        offset(0) {}

remote_ptr::remote_ptr(const remote_ptr &other) noexcept:
        fpga_addr(other.fpga_addr),
        host_addr(other.host_addr),
        len(other.len),
        count(other.count),
        mutex(other.mutex),
        offset(other.offset) {
  if (mutex) {
    std::lock_guard<std::mutex> lock(*mutex);
    (*count)++;
  }
};

remote_ptr::remote_ptr(const intptr_t &faddr) noexcept:
        fpga_addr(faddr),
        host_addr(nullptr),
        len(0),
        count(nullptr),
        mutex(nullptr),
        offset(0) {
}

remote_ptr::remote_ptr(remote_ptr &&other) noexcept:
        fpga_addr(other.fpga_addr),
        host_addr(other.host_addr),
        len(other.len),
        count(other.count),
        mutex(other.mutex),
        offset(other.offset) {
};

remote_ptr remote_ptr::operator-(int q) const {
  return remote_ptr(this->fpga_addr - q,
                    (char *) (this->host_addr) - q,
                    this->len + q,
                    this->count, this->mutex,
                    offset - q
  );
}
