//
// Created by Christopher Kjellqvist on 8/5/24.
//

#ifndef BEETHOVENRUNTIME_RESPONSE_CHANNEL_H
#define BEETHOVENRUNTIME_RESPONSE_CHANNEL_H
#include <cinttypes>
#include <queue>

template<typename id_t, typename byte_t>
class response_channel {
  byte_t ready_field;
  byte_t valid_field;
  id_t id_field;

public:
  std::queue<uint32_t> send_ids;
  std::queue<uint32_t> to_enqueue;

  ~response_channel() = default;

  response_channel() = default;

  void init(byte_t ready,
            byte_t valid,
            id_t id) {
    ready_field = ready;
    valid_field = valid;
    id_field = id;
  }

  uint8_t getReady() const {
    return ready_field.get();
  }

  void setReady(uint8_t ready) {
    ready_field.set(ready);
  }

  uint8_t getValid() const {
    return valid_field.get();
  }

  void setValid(uint8_t valid) {
    valid_field.set(valid);
  }

  uint8_t getId() const {
    return id_field.get();
  }

  void setId(uint64_t id) {
    id_field.set(id);
  }

  const std::queue<id_t> &getSendIds() const {
    return send_ids;
  }

  void setSendIds(const std::queue<id_t> &sendIds) {
    send_ids = sendIds;
  }

  bool fire() {
    return ready_field.get() && valid_field.get();
  }
};

#endif //BEETHOVENRUNTIME_RESPONSE_CHANNEL_H
