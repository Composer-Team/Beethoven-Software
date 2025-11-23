//
// Created by Christopher Kjellqvist on 8/5/24.
//

#ifndef BEETHOVENRUNTIME_ADDRESS_CHANNEL_H
#define BEETHOVENRUNTIME_ADDRESS_CHANNEL_H
#include <cinttypes>

template<typename id_t,
        typename axisize_t,
        typename burst_t,
        typename addr_t,
        typename len_t,
        typename byte_t>
class address_channel {
  byte_t ready_field;
  byte_t valid_field;
  id_t id_field;
  axisize_t size_field;
  burst_t burst_field;
  addr_t addr_field;
  len_t len_field;
public:

  ~address_channel() = default;

  address_channel() = default;

  void init(byte_t ready,
            byte_t valid,
            id_t id,
            axisize_t size,
            burst_t burst,
            addr_t addr,
            len_t len) {
    ready_field = ready;
    valid_field = valid;
    id_field = id;
    size_field = size;
    burst_field = burst;
    addr_field = addr;
    len_field = len;
  }

  [[nodiscard]] bool getReady() const {
    return ready_field.get();
  }

  void setReady(bool ready) {
    address_channel::ready_field.set(ready);
  }

  bool getValid() const {
    return valid_field.get();
  }

  void setValid(bool valid) {
    valid_field.set(valid);
  }

  [[nodiscard]] uint64_t getId() const {
    return id_field.get();
  }

  void setId(uint64_t id) {
    id_field.set(id);
  }

  [[nodiscard]] uint64_t getSize() const {
    return size_field.get();
  }

  void setSize(uint64_t size) {
    size_field.set(size);
  }

  [[nodiscard]] uint64_t getBurst() const {
   return  burst_field.get();
  }

  void setBurst(uint64_t burst) {
    burst_field.set(burst);
  }

  [[nodiscard]] uint64_t getAddr() const {
    // this may cause problems if we need address to be a full 64 bits in simulation
    return addr_field.get(0);
  }

  void setAddr(uint64_t addr) {
    addr_field.set(addr);
  }

  [[nodiscard]] uint64_t getLen() const {
    return len_field.get();
  }

  void setLen(uint64_t len) {
    len_field.set(len);
  }

  bool fire() {
    return ready_field.get() && valid_field.get();
  }
};

#endif //BEETHOVENRUNTIME_ADDRESS_CHANNEL_H
