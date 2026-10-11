#include <config.h>
#include "bitio.h"

void
br_init (BitReader *br, FILE *fp)
{
  br->fp = fp;
  br->buf = 0;
  br->bits = 0;
}

int
br_read (BitReader *br, int n)
{
  if (n <= 0 || n > 32)
    return -1;
  while (br->bits < n)
    {
      int c = fgetc (br->fp);
      if (c == EOF)
	return -1;
      br->buf = (br->buf << 8) | (uint8_t) c;
      br->bits += 8;
    }
  int shift = br->bits - n;
  uint32_t val = (uint32_t) ((br->buf >> shift) & ((1ULL << n) - 1));
  br->bits -= n;
  br->buf &= ((1ULL << br->bits) - 1);
  return (int) val;
}

void
bw_init (BitWriter *bw, FILE *fp)
{
  bw->fp = fp;
  bw->buf = 0;
  bw->bits = 0;
}

void
bw_flush (BitWriter *bw)
{
  while (bw->bits >= 8)
    {
      uint8_t byte = (uint8_t) (bw->buf >> (bw->bits - 8));
      fputc (byte, bw->fp);
      bw->buf &= ((1ULL << (bw->bits - 8)) - 1);
      bw->bits -= 8;
    }
}

void
bw_write (BitWriter *bw, uint32_t val, int n)
{
  if (n <= 0 || n > 32)
    return;
  bw->buf = (bw->buf << n) | (val & ((1ULL << n) - 1));
  bw->bits += n;
  bw_flush (bw);
}

void
bw_finish (BitWriter *bw)
{
  if (bw->bits > 0)
    {
      uint8_t byte = (uint8_t) (bw->buf << (8 - bw->bits));
      fputc (byte, bw->fp);
      bw->buf = 0;
      bw->bits = 0;
    }
}
