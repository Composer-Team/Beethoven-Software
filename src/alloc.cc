//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "beethoven/alloc.h"
#ifndef BAREMETAL
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#endif

beethoven::remote_ptr::~remote_ptr() {
#ifndef BAREMETAL
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
#endif
}

beethoven::remote_ptr & beethoven::remote_ptr::operator=(const beethoven::remote_ptr &other) noexcept {
#ifndef BAREMETAL
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
#endif
    fpga_addr = other.fpga_addr;
    host_addr = other.host_addr;
#ifndef BAREMETAL
    len = other.len;
    count = other.count;
    mutex = other.mutex;
    offset = other.offset;
    if (mutex) {
      std::lock_guard<std::mutex> l2(*mutex);
      (*count)++;
    }
  }
#endif
  return *this;
}

#ifndef BAREMETAL
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
#endif