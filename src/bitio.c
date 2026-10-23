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

void
br_refill (BitReader *br)
{
  while (br->bits <= 56)
    {
      if (br->block_pos == br->block_len)
	{
	  br->block_len = fread (br->block, 1, sizeof (br->block), br->fp);
	  br->block_pos = 0;
	  if (br->block_len == 0)
	    return;
	}
      br->buf = (br->buf << 8) | br->block[br->block_pos++];
      br->bits += 8;
    }
}

void
bw_init (BitWriter *bw, FILE *fp)
{
  bw->fp = fp;
  bw->buf = 0;
  bw->bits = 0;
  bw->block_len = 0;
}

void
bw_drain (BitWriter *bw)
{
  while (bw->bits >= 8)
    {
      if (bw->block_len == sizeof (bw->block))
	{
	  fwrite (bw->block, 1, bw->block_len, bw->fp);
	  bw->block_len = 0;
	}
      bw->bits -= 8;
      bw->block[bw->block_len++] = (uint8_t) (bw->buf >> bw->bits);
    }
}

void
bw_flush (BitWriter *bw)
{
  bw_drain (bw);
  if (bw->block_len > 0)
    fwrite (bw->block, 1, bw->block_len, bw->fp);
  bw->block_len = 0;
}

void
bw_finish (BitWriter *bw)
{
  bw_drain (bw);
  if (bw->bits > 0)
    {
      bw->buf <<= 8 - bw->bits;
      bw->bits = 8;
      bw_drain (bw);
    }
  bw->buf = 0;
  bw->bits = 0;
  bw_flush (bw);
}
