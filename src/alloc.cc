//
// Created by Christopher Kjellqvist on 2/28/24.
//
#include "composer/alloc.h"
#include <sys/mman.h>
#include <cerrno>
#include <cstring>

composer::remote_ptr::~remote_ptr() {
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

composer::remote_ptr & composer::remote_ptr::operator=(const composer::remote_ptr &other) noexcept {
  if (this->mutex == nullptr && this != &other) {
    // assigning to default constructor
    fpga_addr = other.fpga_addr;
    host_addr = other.host_addr;
    len = other.len;
    count = other.count;
    mutex = other.mutex;
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
    if (mutex) {
      std::lock_guard<std::mutex> l2(*mutex);
      (*count)++;
    }
  }
  return *this;
}

composer::remote_ptr& composer::remote_ptr::operator=(composer::remote_ptr && other) noexcept {
  this->mutex = other.mutex;
  this->count = other.count;
  this->host_addr = other.host_addr;
  this->fpga_addr = other.fpga_addr;
  this->len = other.len;
  other.mutex = nullptr;
  other.count = nullptr;
  return *this;
}
