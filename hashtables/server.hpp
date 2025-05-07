#include "hashtable.hpp"
#include "vector"

constexpr size_t k_max_msg = 32 << 20;

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

struct Conn {
  int fd = -1;
  bool want_to_read = false;
  bool want_to_write = false;
  bool want_to_close = false;
  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
};

struct Entry {
  struct HNode node;
  std::string key;
  std::string val;
};

static bool entry_eq(HNode *x, HNode *y) {
  Entry *ex = container_of(x, struct Entry, node);
  Entry *ey = container_of(y, struct Entry, node);
  return ex->key == ey->key;
}

// server
enum {
  RES_OK = 0,
  RES_ERR = 1, // error
  RES_NX = 2,  // key not found
};

struct Response {
  uint32_t status = 0;
  std::vector<uint8_t> data;
};

static void do_get(std::vector<std::string> &cmd, Response &out);
static void do_set(std::vector<std::string> &cmd, Response &);
static void do_del(std::vector<std::string> &cmd, Response &);
static void do_request(std::vector<std::string> &cmd, Response &out);

// Utils
static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }
static void msg_errno(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
}

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}
static uint64_t str_hash(const uint8_t *data, size_t len) {
  uint32_t h = 0x811C9DC5;
  for (size_t i = 0; i < len; i++) {
    h = (h + data[i]) * 0x01000193;
  }
  return h;
}
