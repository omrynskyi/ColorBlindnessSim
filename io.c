#include "io.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct buffer {
    int fd; // file descriptor from open() or creat()
    int offset; // offset into buffer a[]
    // next valid byte (reading)
    // next empty location (writing)
    int num_remaining; // number of bytes remaining in buffer (reading)
    uint8_t a[BUFFER_SIZE]; // buffer
};

Buffer *read_open(const char *filename) {
    int file_descriptor = open(filename, O_RDONLY);
    if (file_descriptor < 0) {
        return NULL;
    }
    Buffer *buffer = (Buffer *) malloc(sizeof(Buffer));
    buffer->fd = file_descriptor;
    buffer->offset = 0;
    buffer->num_remaining = 0;

    return buffer;
}

void read_close(Buffer **pbuf) {
    if (pbuf == NULL || *pbuf == NULL) {
        return;
    }
    close((*pbuf)->fd);
    free(*pbuf);
    *pbuf = NULL;
}
bool read_uint8(Buffer *buf, uint8_t *x) {

    if (buf->num_remaining == 0) {
        ssize_t rc = read(buf->fd, buf->a, sizeof(buf->a));
        if (rc < 0) {
            fprintf(stderr, "error reading form file");
            return false;
        }
        if (rc == 0) {
            return false;
        }
        buf->num_remaining = rc;
        buf->offset = 0;
    }

    *x = buf->a[buf->offset];

    buf->offset++;
    buf->num_remaining--;

    return true;
}
bool read_uint16(Buffer *buf, uint16_t *x) {
    uint8_t byte1, byte2;

    if (!read_uint8(buf, &byte1)) {
        return false;
    }

    if (!read_uint8(buf, &byte2)) {
        return false;
    }

    *x = (uint16_t) (byte2 << 8) | byte1;

    return true;
}

bool read_uint32(Buffer *buf, uint32_t *x) {
    uint16_t value1, value2;

    if (!read_uint16(buf, &value1)) {
        return false;
    }

    if (!read_uint16(buf, &value2)) {
        return false;
    }
    *x = (uint32_t) (value2 << 16) | value1;

    return true;
}

Buffer *write_open(const char *filename) {
    int file_descriptor = creat(filename, 0664);

    // Check if opening the file was unsuccessful
    if (file_descriptor < 0) {
        return NULL;
    }
    Buffer *buffer = (Buffer *) malloc(sizeof(Buffer));

    buffer->fd = file_descriptor;
    buffer->offset = 0;
    buffer->num_remaining = 0;

    return buffer;
}

void write_close(Buffer **pbuf) {

    if (*pbuf == NULL) {
        return;
    }

    Buffer *buf = *pbuf;
    uint8_t *start = buf->a;
    int num_bytes = buf->offset;
    do {
        ssize_t rc = write(buf->fd, start, num_bytes);
        if (rc < 0) {
            fprintf(stderr, "error writing file");
            return;
        }
        start += rc;
        num_bytes -= rc;
    } while (num_bytes > 0);

    close(buf->fd);
    free(buf);
    *pbuf = NULL;
}

void write_uint8(Buffer *buf, uint8_t x) {
    if (buf->offset == BUFFER_SIZE) {
        uint8_t *start = buf->a;
        int num_bytes = buf->offset;
        do {
            ssize_t rc = write(buf->fd, start, num_bytes);
            if (rc < 0) {
                fprintf(stderr, "error writing file");
                return;
            }
            start += rc;
            num_bytes -= rc;
        } while (num_bytes > 0);

        buf->offset = 0;
    }

    buf->a[buf->offset] = x;
    buf->offset++;
}

void write_uint16(Buffer *buf, uint16_t x) {
    write_uint8(buf, x & 0xFF);
    write_uint8(buf, x >> 8);
}

void write_uint32(Buffer *buf, uint32_t x) {

    write_uint16(buf, x & 0xFFFF);
    write_uint16(buf, x >> 16);
}
