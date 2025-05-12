#include "server.hpp"
#include "hashtable.hpp"
#include <assert.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static struct
{
  HMap db;
} g_data;

static void do_request(std::vector<std::string> &cmd, Response &out)
{
  if (cmd.size() == 2 && cmd[0] == "get")
  {
    do_get(cmd, out);
  }
  else if (cmd.size() == 3 && cmd[0] == "set")
  {
    do_set(cmd, out);
  }
  else if (cmd.size() == 2 && cmd[0] == "del")
  {
    do_del(cmd, out);
  }
  else
  {
    out.status = RES_ERR;
  }
  return;
}

static void do_del(std::vector<std::string> &cmd, Response &out)
{
  assert(cmd.size() == 2);
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  HNode *node = h_lookup(&g_data.db, &key.node, entry_eq);
  if (node)
  {
    delete container_of(node, Entry, node);
  }
  out.status = R_OK;
  return;
}

static void do_set(std::vector<std::string> &cmd, Response &out)
{
  assert(cmd.size() == 3);
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  HNode *node = h_lookup(&g_data.db, &key.node, entry_eq);
  if (node)
  {
    container_of(node, Entry, node)->val.swap(cmd[2]);
  }
  else
  {
    Entry *ent = new Entry();
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->val.swap(cmd[2]);
    hm_insert(&g_data.db, &ent->node);
  }
  out.status = RES_OK;
  return;
}

static void do_get(std::vector<std::string> &cmd, Response &out)
{
  assert(cmd.size() == 2);
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
  const HNode *lookup_node = h_lookup(&g_data.db, &key.node, entry_eq);
  if (!lookup_node)
  {
    out.status = RES_NX;
    return;
  }
  // copy the value
  const auto &val = container_of(lookup_node, Entry, node)->val;
  assert(val.size() < k_max_msg);
  out.data.assign(val.begin(), val.end());
}

void fd_set_nb(int fd)
{
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  if (errno)
  {
    die("fcntl error");
    return;
  }

  // set the nonblocking flag
  flags |= O_NONBLOCK;
  errno = 0;
  (void)fcntl(fd, F_SETFL, flags);
  if (errno)
  {
    die("fcntl error");
  }
}

static Conn *handle_accept(int fd)
{
  struct sockaddr_in client_addr = {};
  socklen_t addrlen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
  if (connfd < 0)
  {
    // accept the request and process it
    return NULL;
  }

  fd_set_nb(connfd);
  Conn *conn = new Conn();
  conn->fd = connfd;
  conn->want_to_read = true;
  return conn;
}

static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data,
                       size_t len)
{
  buf.insert(buf.end(), data, data + len);
}

static void buf_consume(std::vector<uint8_t> &buf, size_t n)
{
  buf.erase(buf.begin(), buf.begin() + n);
}

static void make_response(const Response &resp, std::vector<uint8_t> &out)
{
  uint32_t resp_len = 4 + (uint32_t)resp.data.size();
  buf_append(out, (const uint8_t *)&resp_len, 4);
  buf_append(out, (const uint8_t *)&resp.status, 4);
  buf_append(out, resp.data.data(), resp.data.size());
}

// process 1 request if there is enough data
static bool try_one_request_og(Conn *conn)
{
  // try to parse the protocol: message header
  if (conn->incoming.size() < 4)
  {
    return false; // want read
  }
  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg)
  {
    msg("too long");
    conn->want_to_close = true;
    return false; // want close
  }
  // message body
  if (4 + len > conn->incoming.size())
  {
    return false; // want read
  }
  const uint8_t *request = &conn->incoming[4];

  // got one request, do some application logic
  printf("client says: len:%d data:%.*s\n", len, len < 100 ? len : 100,
         request);

  // generate the response (echo)
  buf_append(conn->outgoing, (const uint8_t *)&len, 4);
  buf_append(conn->outgoing, request, len);

  // application logic done! remove the request message.
  buf_consume(conn->incoming, 4 + len);
  // Q: Why not just empty the buffer? See the explanation of "pipelining".
  return true; // success
}

static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out)
{
  if (cur + 4 > end)
  {
    return -1;
  }

  memcpy(&out, cur, 4);
  cur += 4;
  return 0;
}

static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n,
                     std::string &out)
{
  if (cur + n > end)
  {
    return -1;
  }
  out.assign(cur, cur + n);
  cur += n;
  return 0;
}

static int32_t parse_request(const uint8_t *data, size_t size,
                             std::vector<std::string> &out)
{

  const uint8_t *end = data + size; // advance by size bytes

  uint32_t nstr = 0;
  int result;
  if ((result = read_u32(data, end, nstr)) != 0)
  {
    return -1;
  }

  if (nstr > k_max_msg)
  {
    return -1;
  }

  // append until the out is of size nstr
  // other way of saying, run the while loop for nstr times
  while (out.size() < nstr)
  {
    uint32_t len = 0;
    if (read_u32(data, end, len))
    {
      return -1;
    }
    if (len > k_max_msg)
    {
      return -1;
    }
    out.push_back(std::string());
    int read_str_result = read_str(data, end, len, out.back());
    if (read_str_result)
    {
      return -1;
    }
  }

  if (data != end)
  {
    return -1;
  }

  return 0;
}

// conn.want_to_close = true => on any protocol error
// returns true if message is processed
static bool try_one_request(Conn *conn)
{
  if (conn->incoming.size() < 4)
  {
    return false; // want to read more
  }

  uint32_t len = 0;
  memcpy(&len, conn->incoming.data(), 4);
  if (len > k_max_msg) // error handling
  {
    msg("too long");
    conn->want_to_close = true;
    return false;
  }

  if (4 + len > conn->incoming.size())
  {
    return false;
  }

  const uint8_t *request = &conn->incoming[4];
  std::vector<std::string> cmd;
  if (parse_request(request, len, cmd) < 0)
  {
        msg("bad request");
        conn->want_to_close = true;
        return false;   // want close
  }
  Response resp;
  do_request(cmd, resp);
  make_response(resp, conn->outgoing);

  // application logic done! remove the request message.
  buf_consume(conn->incoming, 4 + len);

  return true; // success
}

static void handle_write(Conn *conn)
{
  assert(conn->outgoing.size() > 0);
  size_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());
  if (rv < 0 && rv == EAGAIN)
  {
    return;
  }
  if (rv < 0)
  {
    msg_errno("write() error");
    conn->want_to_close = true;
    return;
  }
  buf_consume(conn->outgoing, rv);
  if (conn->outgoing.size() == 0) // all data written
  {
    conn->want_to_write = false;
    conn->want_to_read = true;
  }
  return;
}

static void handle_read(Conn *conn)
{
  uint8_t buf[64 * 1024];
  int rv = read(conn->fd, buf, sizeof(buf));
  if (rv < 0 || rv == EAGAIN) // handle IO error -> err < 0 and err == 0 i.e EOF
  {
    return;
  }
  if (rv < 0) // handle IO error -> err < 0 and err == 0 i.e EOF
  {
    msg_errno("handle_read -> read() error");
    conn->want_to_close = true;
    return;
  }
  if (rv == 0)
  {
    if (conn->incoming.size() == 0)
    {
      msg("client closed");
    }
    else
    {
      msg("unexpected EOF");
    }
    conn->want_to_close = true;
    return; // want close
  }
  buf_append(conn->incoming, buf, (size_t)rv);

  // this is critical to the pipelined request handling
  while (try_one_request(conn))
  {
    printf("processed one request from conn %d\n", conn->fd);
  }

  if (conn->outgoing.size() > 0)
  {
    conn->want_to_write = true;
    conn->want_to_read = false;
    return handle_write(conn);
  }

  return;
}

int main()
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    die("socket()");
  }
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  // bind
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(1234);
  addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0
  int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
  if (rv)
  {
    die("bind()");
  }

  // set the listen fd to nonblocking mode
  fd_set_nb(fd);

  // listen
  fprintf(stderr, "trying to listen\n");
  rv = listen(fd, SOMAXCONN);
  if (rv)
  {
    die("listen()");
  }
  fprintf(stderr, "server FD = %d\n", fd);

  std::vector<Conn *> fd2conn;
  std::vector<struct pollfd> poll_args;

  fprintf(stderr, "started listening....\n");
  while (true)
  {
    poll_args.clear();
    struct pollfd pfd = {fd, POLLIN, 0};
    poll_args.push_back(pfd);

    for (Conn *conn : fd2conn)
    {
      if (!conn)
      {
        continue;
      }

      struct pollfd pfd = {conn->fd, POLLERR, 0};
      if (conn->want_to_read)
      {
        pfd.events |= POLLIN;
      }
      if (conn->want_to_write)
      {
        pfd.events |= POLLOUT;
      }
      poll_args.push_back(pfd);
    }

    int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
    if (rv < 0 && errno == EINTR)
    {
      fprintf(stderr, "poll returned - %d, errno - %d, trying again\n", rv,
              errno);
      continue;
    }
    if (rv < 0)
    {
      fprintf(stderr, "poll returned - %d", rv);
      die("poll");
    }

    // We set POLLIN for server fd, so now we have connections to process
    if (poll_args[0].revents)
    {
      if (Conn *conn = handle_accept(fd))
      {
        if (fd2conn.size() <= (size_t)conn->fd)
        {
          fd2conn.resize(conn->fd + 1);
        }
        fd2conn[conn->fd] = conn;
      }
    }

    // handle connection sockets
    // go over all known connections
    // and check if they have revents
    for (size_t i = 1; i < fd2conn.size(); ++i)
    {
      uint32_t ready = poll_args[i].revents;
      if (ready == 0)
      {
        continue;
      }

      Conn *conn = fd2conn[poll_args[i].fd];
      if (!conn)
      {
        die("connection is nil");
      }
      // check if they are POLLIN or POLLOUT or both
      if (ready & POLLIN)
      {
        assert(conn->want_to_read);
        handle_read(conn);
      }

      if (ready & POLLOUT)
      {
        assert(conn->want_to_write);
        handle_write(conn);
      }

      if ((ready & POLLERR) || conn->want_to_close)
      {
        (void)close(conn->fd);
        fd2conn[conn->fd] = NULL;
        delete conn;
        conn = nullptr;
      }
    }
  }
  return 0;
}

// static int32_t write_all(int fd, const char *buf, size_t n) {
//     while (n > 0) {
//         ssize_t rv = write(fd, buf, n);
//         if (rv <= 0) {
//             return -1;  // error
//         }
//         assert((size_t)rv <= n);
//         n -= (size_t)rv;
//         buf += rv;
//     }
//     return 0;
// }
