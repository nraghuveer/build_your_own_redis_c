#pragma once
#include <vector>

enum {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5
};
enum {
    ERR_UNKNOWN = 1,
    ERR_TOO_LONG = 2,
};

// Buffer
typedef std::vector<uint8_t> Buffer;

static void buf_append(Buffer& buf, const uint8_t *data, size_t size) {
    buf.insert(buf.end(), data, data + size);
}
static void buf_append_u8(Buffer& buf, uint8_t value) {
    buf.push_back(value);
}
static void buf_append_u32(Buffer &buf, uint32_t data) {
    buf_append(buf, (const uint8_t *)&data, 4);
}
static void buf_append_i64(Buffer &buf, int64_t data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}
static void buf_append_dbl(Buffer &buf, double data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}
// remove from the front
static void buf_consume(Buffer &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}


// End of Buffer

static void out_nil(Buffer &buf) {
    buf_append_u8(buf, TAG_NIL);
}

static void out_int(Buffer &buf, int64_t value) {
    buf_append_u8(buf, TAG_INT);
    buf_append_u8(buf, value);
}

static void out_str(Buffer &buf, const char *s, const size_t len) {
    buf_append_u8(buf, TAG_STR);
    buf_append_u32(buf, len);
    buf_append(buf, (const uint8_t *)s, len);
}

static void out_arr(Buffer &buf, uint32_t n) {
    buf_append_u8(buf, TAG_ARR);
    buf_append_u32(buf, n);
}

static void out_err(Buffer &buf, uint32_t err_code, const std::string &msg) {
    buf_append_u8(buf, TAG_ERR);
    buf_append_u32(buf, err_code);
    buf_append_u32(buf, msg.size());
    buf_append(buf, (const uint8_t *)msg.c_str(), msg.size());
}
