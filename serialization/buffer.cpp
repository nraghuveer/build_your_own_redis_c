#include "buffer.hpp"
#include <cstring>
#include <stdio.h>

Buffer::Buffer(size_t max_size) : max_size(max_size) {
  start = new uint8_t[max_size];
  end = start + max_size;
  data_start = start;
  data_end = start;
}

Buffer::~Buffer() {
  delete[] start;
}

size_t Buffer::data_size() {
  if (!data_start || !data_end) {
    return 0;
  }
  return data_end - data_start;
}

size_t Buffer::free_space() {
  return (end - data_end) + (data_start - start);
}

void Buffer::append(uint8_t *new_data, size_t len) {
  if ((data_size() + len) > max_size) {
    size_t new_max_size = (data_size() + len) * 2;
    if (!_expand(new_max_size)) {
      fprintf(stderr, "[buffer] failed to expand to %zu\n", new_max_size);
      return;
    }
  }
  if (data_end + len > end) {
    compact();
  }

  std::memcpy(data_end, new_data, len);
  data_end += len;
}

void Buffer::compact() {
  auto current_data_size = data_size();
  if (data_start == start) {
    return;
  }
  std::memmove(start, data_start, current_data_size);
  data_start = start;
  data_end = data_start + current_data_size;
  return;
}

void Buffer::consume(size_t len) {
  if (len > data_size()) {
    return;
  }
  data_start += len;
}

bool Buffer::_expand(size_t new_max_size) {
  if (new_max_size < max_size) {
    fprintf(stderr, "[buffer] requested to expand to size < current size, something is very wrong");
    return false;
  }
  auto current_data_size = data_size();
  uint8_t *new_start = static_cast<uint8_t *>(malloc(new_max_size));

  if (new_start == nullptr) {
    return false;
  }

  std::memcpy(new_start, data_start, current_data_size);
  delete[] start;

  start = new_start;
  end = start + new_max_size;
  data_start = start;
  data_end = data_start + current_data_size;
  max_size = new_max_size;

  return true;
}