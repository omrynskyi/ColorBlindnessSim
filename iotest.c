#include "io.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char filename[] = "/tmp/iotest.XXXXXX";
static int fd = -1;

// this is a bunch of macro nonsense that you don't have to understand, but talk to ben on discord
// if you want to!
// the short version is you can write:
// TEST(x == 5, "expected x to be 5 but it was %d", x)
// and that will exit with a friendly message if x != 5
#define TEST(condition, message, ...)                                                              \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fatal(0, "iotest.c line %u: \x1b[1;31mtest (%s) failed:\x1b[0m " message, __LINE__,    \
                #condition, __VA_ARGS__);                                                          \
        }                                                                                          \
    } while (0)

// print an error to stderr and exit the program
// if err is >0, strerror() is used to get a description of the error and this is printed after the
// error message. otherwise, only the message plus a newline is printed.
// also cleans up the temporary file if it exists
//
// err: numeric error code, or zero to print only message
// fmt: printf-style format string
// ...: remaining arguments to be included in output according to the format string
static void fatal(int err, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (err > 0) {
        fprintf(stderr, ": %s", strerror(err));
    }
    fprintf(stderr, "\n");

    if (fd >= 0) {
        // simple check that these function succeed
        // unlink will remove the file from the directory it is in, which means now the only way to
        // access it is via an open file descriptor. only our program has open file descriptors for
        // it, so the file will get deleted after we close them and/or exit
        assert(unlink(filename) == 0);
        assert(close(fd) == 0);
    }

    exit(1);
}

int main(int argc, char **argv) {
    // silence unused parameter warning
    (void) argc;
    // make a temporary file with a random filename
    // if it succeeds, mkstemp will alter our "template" to be the actual name of the file (it
    // replaces XXXXXX with random characters)
    fd = mkstemp(filename);
    if (fd < 0) {
        fatal(errno, "NOT YOUR FAULT: failed to create temporary file");
    }

    // write some data which we will then read with the IO functions to see if they read correctly
    uint8_t data[] = { 0x01, 0x02, 0x03, 0x04, 0x55, 0x66, 0x77, 0x88, 0xff };
    if (write(fd, data, sizeof(data)) != sizeof(data)) {
        fatal(errno, "NOT YOUR FAULT: failed to write all test data to the temporary file");
    }
    // write a lot more data so we end up having to buffer some more
    uint8_t big_chunk[3 * BUFFER_SIZE];
    memset(big_chunk, 0xaa, 3 * BUFFER_SIZE);
    if (write(fd, big_chunk, 3 * BUFFER_SIZE) != 3 * BUFFER_SIZE) {
        fatal(errno, "NOT YOUR FAULT: failed to write all test data to the temporary file");
    }

    printf("testing read_uint8...\n");

    // test reading a byte at a time, including all the big buffers
    Buffer *buf = read_open(filename);
    if (!buf) {
        fatal(errno, "failed creating buffer");
    }

    uint8_t byte;
    for (unsigned int i = 0; i < sizeof(data) + sizeof(big_chunk); i++) {
        TEST(read_uint8(buf, &byte) == true,
            "read_uint8 returned false after %u bytes even though there was more data in the file",
            i);
        uint8_t expected_byte = i < sizeof(data) ? data[i] : 0xaa;
        TEST(byte == expected_byte, "byte %u: expected to read %#04" PRIx8 ", got %#04" PRIx8, i,
            expected_byte, byte);
    }

    TEST(
        read_uint8(buf, &byte) == false, "read_uint8 returned true even at the end of the file", 0);
    read_close(&buf);
    TEST(buf == NULL, "read_close did not set the pointer to null", 0);

    printf("testing read_uint16...\n");
    // strip everything but the contents of data from the file (remove the big_chunk bytes)
    if (ftruncate(fd, sizeof(data)) != 0) {
        fatal(errno, "NOT YOUR FAULT: failed truncating test file");
    }

    buf = read_open(filename);
    if (!buf) {
        fatal(errno, "failed creating buffer");
    }

    uint16_t two_bytes;
    // little-endian
    uint16_t expected_16[] = { 0x0201, 0x0403, 0x6655, 0x8877 };
    for (unsigned int i = 0; i < sizeof(expected_16) / sizeof(uint16_t); i++) {
        TEST(read_uint16(buf, &two_bytes) == true,
            "read_uint16 returned false after %u calls even though there was more data in the file",
            i);
        TEST(two_bytes == expected_16[i],
            "read_uint16 call %u: expected to read %#06" PRIx16 ", got %#06" PRIx16, i,
            expected_16[i], two_bytes);
    }

    TEST(read_uint16(buf, &two_bytes) == false,
        "read_uint16 returned true with only 1 byte left in the file", 0);

    read_close(&buf);

    printf("testing read_uint32...\n");
    buf = read_open(filename);
    if (!buf) {
        fatal(errno, "failed creating buffer");
    }

    uint32_t four_bytes;
    uint32_t expected_32[] = { 0x04030201, 0x88776655 };
    for (unsigned int i = 0; i < sizeof(expected_32) / sizeof(uint32_t); i++) {
        TEST(read_uint32(buf, &four_bytes) == true,
            "read_uint32 returned false after %u calls even though there was more data in the file",
            i);
        TEST(four_bytes == expected_32[i],
            "read_uint16 call %u: expected to read %#010" PRIx32 ", got %#010" PRIx32, i,
            expected_32[i], four_bytes);
    }

    TEST(read_uint32(buf, &four_bytes) == false,
        "read_uint32 returned true with only 1 byte left in the file", 0);

    read_close(&buf);

    unlink(filename);
    close(fd);

    printf(
        "At this point all tests have passed. Congratulations! Additional things you can try:\n"
        "- run this program with valgrind to make sure you don't access invalid memory and you\n"
        "  free everything (the test calls read/write_close, so if there are leaks it's because\n"
        "  you don't free everything that you should)\n"
        "- run 'strace -e trace=read,write %s' and make sure you are reading and writing %u-byte\n"
        "  chunks (all reads/writes on file descriptor 4 are from your IO code)\n",
        argv[0], BUFFER_SIZE);
    return 0;
}
