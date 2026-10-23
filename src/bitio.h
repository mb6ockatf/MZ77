#ifndef BITIO
#define BITIO 1
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#define BITIO_BLOCK_SIZE 65536

typedef struct {
    FILE *fp;
    uint64_t buf;
    int bits;
    size_t block_pos;
    size_t block_len;
    uint8_t block[BITIO_BLOCK_SIZE];
} BitReader;

typedef struct {
    FILE *fp;
    uint64_t buf;
    int bits;
    size_t block_len;
    uint8_t block[BITIO_BLOCK_SIZE];
} BitWriter;

void br_init(BitReader *br, FILE *fp);
int  br_read(BitReader *br, int width);

void bw_init(BitWriter *bw, FILE *fp);
void bw_write(BitWriter *bw, uint32_t val, int width);
void bw_flush(BitWriter *bw);
void bw_finish(BitWriter *bw);
#endif
