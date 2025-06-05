#ifndef DATAWRAPPER_H
#define DATAWRAPPER_H

template <typename T>
struct GetSetWrapper {
  T* ptr = nullptr;
  size_t l = sizeof(T);

  GetSetWrapper(T &v) {
    ptr = &v;
  }

  GetSetWrapper() = default;

  T get(int idx) const {
    return ptr[idx];
  }

  T get() const {
    return this->get(0);
  }

  void set(int64_t value) {
    if (sizeof(T) >= 8) {
      memcpy(ptr, &value, 8);
    } else {
      memcpy(ptr, &value, l);
    }
  }
};

template <typename T, int l>
struct GetSetDataWrapper {
  T* ptr = nullptr;
  int len = l;

  template <typename G>
  explicit GetSetDataWrapper(G *v) {
    ptr = (T*)v;
  }
  GetSetDataWrapper() = default;

  T get(int idx) const {
    return ptr[idx];
  }

  std::unique_ptr<uint8_t[]> get() const {
    std::unique_ptr<uint8_t[]> alloc(new uint8_t[l]);
    memcpy(alloc.get(), ptr, l);
    return alloc;
  }
  void set(uint64_t value) {
    if (l >= 8) {
      memcpy(ptr, &value, 8);
    } else {
      memcpy(ptr, &value, l);
    }
  }
  
  void set(uint32_t payload, uint32_t idx) {
    uint32_t *dst = (uint32_t*)ptr;
    dst[idx] = payload;
  }

  void set(int32_t *value) {
    memcpy(ptr, value, l);
  }
};

#endif
