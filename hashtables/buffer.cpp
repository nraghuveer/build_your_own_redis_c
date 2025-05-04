#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

// Expandable buffer
struct Buffer {
  uint8_t *start; // pointer to start of the buffer
  uint8_t *end;   // poniter to end of the buffer
  uint8_t *data_start;
  uint8_t *data_end;
  size_t max_size;

  Buffer(size_t max_size) : max_size(max_size) {
    // start = static_cast<uint8_t *>(malloc(max_size));
    start = new uint8_t[max_size];
    end = start + max_size;
    data_start = start;
    data_end = start;
  }

  ~Buffer() {
    // free(start);
    delete[] start;
  }

  size_t data_size() {
    if (!data_start || !data_end) {
      return 0;
    }
    return data_end - data_start;
  }

  size_t free_space() { return (end - data_end) + (data_start - start); }

  void append(uint8_t *new_data, size_t len) {
    // expand the buffer
    if ((data_size() + len) > max_size) {
      // realloc => not possible now
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

  void compact() {
    auto current_data_size = data_size();
    if (data_start == start) {
      return;
    }
    // now move the data_start to start
    std::memmove(start, data_start, current_data_size);
    data_start = start;
    data_end = data_start + current_data_size;
    return;
  }

  void consume(size_t len) {
    if (len > data_size()) {
      return;
    }
    // move data_start
    data_start += len;
  }

private:
  bool _expand(size_t new_max_size) {
    if (new_max_size < max_size) {
      fprintf(
          stderr,
          "[buffer] requested to expand to size < current size, something is "
          "very wrong");
      return false;
    }
    auto current_data_size = data_size();
    uint8_t *new_start = static_cast<uint8_t *>(malloc(new_max_size));

    if (new_start == nullptr) {
      return false;
    }

    // copy the memory
    std::memcpy(new_start, data_start, current_data_size);

    // free(start);
    delete[] start;

    // update pointers
    start = new_start;
    end = start + new_max_size;
    data_start = start;
    data_end = data_start + current_data_size;
    max_size = new_max_size;

    return true;
  }
};
