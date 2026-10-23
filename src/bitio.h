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
void br_refill(BitReader *br);

void bw_init(BitWriter *bw, FILE *fp);
void bw_drain(BitWriter *bw);
void bw_flush(BitWriter *bw);
void bw_finish(BitWriter *bw);

static inline int
br_read (BitReader *br, int width)
{
  if (br->bits < width)
    {
      br_refill (br);
      if (br->bits < width)
	return -1;
    }
  br->bits -= width;
  return (int) ((br->buf >> br->bits) & ((1ULL << width) - 1));
}

static inline void
bw_write (BitWriter *bw, uint32_t val, int width)
{
  bw->buf = (bw->buf << width) | (val & ((1ULL << width) - 1));
  bw->bits += width;
  if (bw->bits >= 32)
    bw_drain (bw);
}
#endif
