//
// Created by Christopher Kjellqvist on 2/28/24.
//

// use smart pointers to do a basic merge sort

#include <iostream>
#include "composer/fpga_handle.h"
#include <random>

using namespace composer;

fpga_handle_t handle;

remote_ptr merge(const remote_ptr &a_ptr,
                 int a_size,
                 const remote_ptr &b_ptr,
                 int b_size) {
  auto c_handle = handle.malloc((a_size + b_size) * sizeof(int));
  int *c = (int*)c_handle.getHostAddr();
  int *a = (int*)a_ptr.getHostAddr();
  int *b = (int*)b_ptr.getHostAddr();
  int i = 0, j = 0, k = 0;
  while (i < a_size && j < b_size) {
    if (a[i] < b[j]) {
      c[k++] = a[i++];
    } else {
      c[k++] = b[j++];
    }
  }
  while (i < a_size) {
    c[k++] = a[i++];
  }
  while (j < b_size) {
    c[k++] = b[j++];
  }
  return c_handle;
}

remote_ptr merge_sort(const remote_ptr &arr_ptr, int size) {
  if (size <= 1) {
    return arr_ptr;
  }
  int mid = size / 2;
  auto left = merge_sort(arr_ptr, mid);
  auto right = merge_sort(arr_ptr + mid, size - mid);
  return merge(left, mid, right, size - mid);
}

int main() {
  // generate random array
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, 100);
  auto seed = rd();
  std::default_random_engine eng(seed);
  auto arr_handle = handle.malloc(10 * sizeof(int));
  int *arr = (int*)arr_handle.getHostAddr();
  for (int i = 0; i < 10; i++) {
    arr[i] = dist(eng);
  }
  // sort array
  auto sorted_handle = merge_sort(arr_handle, 10);
  int *sorted = (int*)sorted_handle.getHostAddr();
  for (int i = 0; i < 10; i++) {
    std::cout << sorted[i] << " ";
  }
}