#pragma once

#include <cstdint>
#include <cstdlib>

struct Buffer {
    Buffer(size_t max_size);
    ~Buffer();

    // Returns the current size of data in the buffer
    size_t data_size();

    // Returns available free space in the buffer
    size_t free_space();

    // Append new data to the buffer
    void append(uint8_t* new_data, size_t len);

    // Compact the buffer by moving data to the start
    void compact();

    // Consume (remove) data from the start of the buffer
    void consume(size_t len);

private:
    // Expand buffer to new size
    bool _expand(size_t new_max_size);

    uint8_t* start;      // pointer to start of the buffer
    uint8_t* end;        // pointer to end of the buffer
    uint8_t* data_start; // pointer to start of actual data
    uint8_t* data_end;   // pointer to end of actual data
    size_t max_size;     // maximum size of the buffer
};