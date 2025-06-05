//
// Created by Christopher Kjellqvist on 8/5/24.
//

#ifndef BEETHOVENRUNTIME_VCS_HANDLE_H
#define BEETHOVENRUNTIME_VCS_HANDLE_H

#include "vpi_user.h"

class VCSShortHandle {
  vpiHandle handle;
public:
  VCSShortHandle(vpiHandle handle) {
    this->handle = handle;
    int size = vpi_get(vpiSize, handle);
    if ((size >> 5) > 1) {
      std::cerr << "value is too big, quitting " << size << std::endl;
      exit(1);
    }
  }

  VCSShortHandle() = default;

  [[nodiscard]] uint32_t get() const {
    s_vpi_value value;
    value.format = vpiIntVal;
    vpi_get_value(handle, &value);
    return value.value.integer;
  }

  [[nodiscard]] uint32_t get(int chunk) const {
    s_vpi_value value;
    value.format = vpiVectorVal;
    vpi_get_value(handle, &value);
    return value.value.vector[chunk].aval;
  }

  void set(int32_t val) const {
    s_vpi_value value;
    value.format = vpiIntVal;
    value.value.integer = val;
    vpi_put_value(handle, &value, nullptr, vpiNoDelay);
  }

  void set(const uint8_t &payload, uint32_t idx) const {
    // first, get the payload
    s_vpi_value value;
    value.format = vpiIntVal;
    vpi_get_value(handle, &value);
    // then, set the payload
    value.value.integer = (value.value.integer & ~(0xFF << (idx * 8))) | ((uint32_t)(payload) << (idx * 8));
    vpi_put_value(handle, &value, nullptr, vpiNoDelay);
  }

};

class VCSLongHandle {
  vpiHandle handle;
  int nchunks;
public:
  VCSLongHandle(vpiHandle handle) {
    this->handle = handle;
    nchunks = vpi_get(vpiSize, handle);
  }

  VCSLongHandle() = default;

  [[nodiscard]] uint32_t get(int idx) const {
    if (nchunks == 1) {
      s_vpi_value value;
      value.format = vpiIntVal;
      vpi_get_value(handle, &value);
      return value.value.integer;
    } else {
      // get the full payload and only return the requested chunk
      s_vpi_value value;
      value.format = vpiVectorVal;
      vpi_get_value(handle, &value);
      return value.value.vector[idx].aval;
    }
  }

  [[nodiscard]] std::unique_ptr<uint8_t[]> get() const {
    s_vpi_value value;
    value.format = vpiVectorVal;
    vpi_get_value(handle, &value);
    std::unique_ptr<uint8_t[]> ret(new uint8_t[nchunks]);
    for (int idx = 0; idx < nchunks / 4; idx++) {
      for (int off = 0; off < 4; off++) {
        int i = idx * 4 + off;
        uint32_t payload = value.value.vector[idx].aval;
        uint32_t mask = 0xFF << (off*8);
        ret.get()[i] = (payload & mask) >> (off*8);
        // printf("[%d]: PAYLOAD: %08x\tMASK: %08x\tresult: %02x\n", i, payload, mask, ret.get()[i]);
      }
    }
    return ret;

  }

  void set(int32_t val) const {
    s_vpi_value value;
    value.format = vpiIntVal;
    value.value.integer = val;
    vpi_put_value(handle, &value, nullptr, vpiNoDelay);
  }

  void set(const uint32_t *val) const {
    s_vpi_value value;
    value.format = vpiVectorVal;
    printf("trying to write %d chunks\n", nchunks);
    auto vec = new s_vpi_vecval[nchunks];
    for (int i = 0; i < nchunks; i++) {
      vec[i].aval = val[i];
      vec[i].bval = 0;
    }
    value.value.vector = vec;
    vpi_put_value(handle, &value, nullptr, vpiNoDelay);
    delete[] vec;
  }

  void set(const uint32_t &payload, uint32_t chunk) const {
    //printf("called set with (%d) <- %x\n", chunk, payload); fflush(stdout);
    // first, get the payload
    s_vpi_value value;
    value.format = vpiVectorVal;
    vpi_get_value(handle, &value);
    // then, set the payload inside the correct chunk
    int chunkVal = value.value.vector[chunk].aval;
    value.value.vector[chunk].aval = payload; 
    value.value.vector[chunk].bval = 0;
    // now write back
    vpi_put_value(handle, &value, nullptr, vpiNoDelay);
  }
};

#endif //BEETHOVENRUNTIME_VCS_HANDLE_H
