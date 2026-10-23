#include <config.h>
#include "bitio.h"

void
br_init (BitReader *br, FILE *fp)
{
  br->fp = fp;
  br->buf = 0;
  br->bits = 0;
  br->block_pos = 0;
  br->block_len = 0;
}

int
br_read (BitReader *br, int width)
{
  if (width <= 0 || width > 32)
    return -1;
  while (br->bits < width)
    {
      if (br->block_pos == br->block_len)
	{
	  br->block_len = fread (br->block, 1, sizeof (br->block), br->fp);
	  br->block_pos = 0;
	  if (br->block_len == 0)
	    return -1;
	}
      br->buf = (br->buf << 8) | br->block[br->block_pos++];
      br->bits += 8;
    }
  int shift = br->bits - width;
  uint32_t val = (uint32_t) ((br->buf >> shift) & ((1ULL << width) - 1));
  br->bits -= width;
  br->buf &= ((1ULL << br->bits) - 1);
  return (int) val;
}

void
bw_init (BitWriter *bw, FILE *fp)
{
  bw->fp = fp;
  bw->buf = 0;
  bw->bits = 0;
  bw->block_len = 0;
}

static void
bw_put_byte (BitWriter *bw, uint8_t byte)
{
  if (bw->block_len == sizeof (bw->block))
    {
      fwrite (bw->block, 1, bw->block_len, bw->fp);
      bw->block_len = 0;
    }
  bw->block[bw->block_len++] = byte;
}

void
bw_flush (BitWriter *bw)
{
  while (bw->bits >= 8)
    {
      bw_put_byte (bw, (uint8_t) (bw->buf >> (bw->bits - 8)));
      bw->buf &= ((1ULL << (bw->bits - 8)) - 1);
      bw->bits -= 8;
    }
}

void
bw_write (BitWriter *bw, uint32_t val, int width)
{
  if (width <= 0 || width > 32)
    return;
  bw->buf = (bw->buf << width) | (val & ((1ULL << width) - 1));
  bw->bits += width;
  bw_flush (bw);
}

void
bw_finish (BitWriter *bw)
{
  if (bw->bits > 0)
    {
      bw_put_byte (bw, (uint8_t) (bw->buf << (8 - bw->bits)));
      bw->buf = 0;
      bw->bits = 0;
    }
  if (bw->block_len > 0)
    fwrite (bw->block, 1, bw->block_len, bw->fp);
  bw->block_len = 0;
}
