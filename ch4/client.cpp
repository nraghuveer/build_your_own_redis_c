#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

static void die(const char *msg)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

const size_t k_max_msg = 4096;

static int32_t write_all(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        size_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        size_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            msg(rv == 0 ? "EOF" : "read() error");
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t
query(int fd, const char *text)
{
    // write text to the server
    // and read from the server
    // follow the protocol

    // create a write buffer
    char wbuf[4 + k_max_msg] = {};
    // write the len of text to this
    uint32_t len = (int32_t)strlen(text);
    memcpy(wbuf, &len, 4); // write to first 4 bytes

    memcpy(&wbuf[4], text, len); // write the text

    if (int err = write_all(fd, wbuf, 4 + len))
    {
        return err;
    }

    // now read from server
    char rbuf[4 + k_max_msg] = {};
    errno = 0;
    if (int err = read_full(fd, rbuf, 4))
    {
        return err;
    }
    memcpy(&len, rbuf, 4);
    if (read_full(fd, &rbuf[4], len))
    {
        die("read_full()");
    }

    // do something
    printf("server says: %.*s\n", len, &rbuf[4]);
    return 0;
}

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv)
    {
        die("connect");
    }

    char msg[] = "hello0";
    if (query(fd, msg))
    {
        die("query()");
    }
    if (query(fd, "hello1"))
    {
        die("query()");
    }
    close(fd);
    return 0;
}
