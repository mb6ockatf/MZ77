#include "lz77.h"
#include "bitio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WINDOW_SIZE 4096
#define MAX_MATCH   258
#define MIN_MATCH   3
#define HT_SIZE     65536
#define MAX_CHAIN   256
#define NO_POS      UINT64_MAX

static inline uint16_t
hash3 (const uint8_t *bytes)
{
  return ((uint16_t) bytes[0] << 10) ^ (bytes[1] << 4) ^ bytes[2];
}

static inline uint8_t
byte_at (const uint8_t *window, const uint8_t *buf, uint64_t win_pos,
	 uint64_t pos)
{
  if (pos < win_pos)
    return window[pos % WINDOW_SIZE];
  return buf[pos - win_pos];
}

static inline void
ht_insert (uint64_t *ht_heads, uint64_t *ht_next, const uint8_t *bytes,
	   uint64_t pos)
{
  uint16_t hash = hash3 (bytes);
  ht_next[pos % WINDOW_SIZE] = ht_heads[hash];
  ht_heads[hash] = pos;
}

int
lz77_compress (const char *in_path, const char *out_path)
{
  FILE *fin = fopen (in_path, "rb");
  if (!fin)
    return -1;
  FILE *fout = fopen (out_path, "wb");
  if (!fout)
    {
      fclose (fin);
      return -1;
    }
  BitWriter bw;
  bw_init (&bw, fout);
  uint8_t window[WINDOW_SIZE] = { 0 };
  uint64_t *ht_heads = malloc (HT_SIZE * sizeof (uint64_t));
  uint64_t *ht_next = malloc (WINDOW_SIZE * sizeof (uint64_t));
  if (!ht_heads || !ht_next)
    {
      free (ht_heads);
      free (ht_next);
      fclose (fin);
      fclose (fout);
      return -1;
    }
  for (size_t slot = 0; slot < HT_SIZE; slot++)
    ht_heads[slot] = NO_POS;
  for (size_t slot = 0; slot < WINDOW_SIZE; slot++)
    ht_next[slot] = NO_POS;
  uint8_t buf[WINDOW_SIZE];
  size_t buf_len = fread (buf, 1, sizeof (buf), fin);
  uint64_t win_pos = 0;
  while (buf_len > 0)
    {
      size_t best_len = 0;
      uint64_t best_dist = 0;
      if (buf_len >= MIN_MATCH)
	{
	  uint64_t pos = ht_heads[hash3 (buf)];
	  int chain_left = MAX_CHAIN;
	  while (pos != NO_POS && pos < win_pos
		 && win_pos - pos <= WINDOW_SIZE && chain_left-- > 0)
	    {
	      size_t len = 0;
	      while (len < MAX_MATCH && len < buf_len
		     && byte_at (window, buf, win_pos, pos + len) == buf[len])
		len++;
	      if (len > best_len)
		{
		  best_len = len;
		  best_dist = win_pos - pos;
		  if (len == MAX_MATCH)
		    break;
		}
	      uint64_t next = ht_next[pos % WINDOW_SIZE];
	      if (next == NO_POS || next >= pos)
		break;
	      pos = next;
	    }
	}
      size_t advance;
      if (best_len >= MIN_MATCH)
	{
	  bw_write (&bw, 1, 1);
	  bw_write (&bw, (uint32_t) (best_dist - 1), 12);
	  bw_write (&bw, (uint32_t) (best_len - MIN_MATCH), 8);
	  advance = best_len;
	}
      else
	{
	  bw_write (&bw, 0, 1);
	  bw_write (&bw, buf[0], 8);
	  advance = 1;
	}
      for (size_t offset = 0; offset < advance; offset++)
	{
	  window[win_pos % WINDOW_SIZE] = buf[offset];
	  if (offset + MIN_MATCH <= buf_len)
	    ht_insert (ht_heads, ht_next, &buf[offset], win_pos);
	  win_pos++;
	}
      memmove (buf, buf + advance, buf_len - advance);
      buf_len -= advance;
      if (buf_len < MAX_MATCH)
	buf_len += fread (buf + buf_len, 1, sizeof (buf) - buf_len, fin);
    }
  bw_finish (&bw);
  free (ht_heads);
  free (ht_next);
  int status = (ferror (fin) || ferror (fout)) ? -1 : 0;
  fclose (fin);
  if (fclose (fout) != 0)
    status = -1;
  return status;
}

int
lz77_decompress (const char *in_path, const char *out_path)
{
  FILE *fin = fopen (in_path, "rb");
  if (!fin)
    return -1;
  FILE *fout = fopen (out_path, "wb");
  if (!fout)
    {
      fclose (fin);
      return -1;
    }
  BitReader br;
  br_init (&br, fin);
  uint8_t window[WINDOW_SIZE] = { 0 };
  uint64_t win_pos = 0;
  int status = 0;
  while (1)
    {
      int flag = br_read (&br, 1);
      if (flag == -1)
	break;
      if (flag == 0)
	{
	  int literal = br_read (&br, 8);
	  if (literal == -1)
	    break;
	  fputc ((uint8_t) literal, fout);
	  window[win_pos % WINDOW_SIZE] = (uint8_t) literal;
	  win_pos++;
	}
      else
	{
	  int dist_enc = br_read (&br, 12);
	  int len_enc = br_read (&br, 8);
	  if (dist_enc == -1 || len_enc == -1)
	    {
	      status = -1;
	      break;
	    }
	  uint32_t dist = (uint32_t) dist_enc + 1;
	  uint32_t len = (uint32_t) len_enc + MIN_MATCH;
	  if (dist > win_pos)
	    {
	      status = -1;
	      break;
	    }
	  uint64_t src = win_pos - dist;
	  for (uint32_t offset = 0; offset < len; offset++)
	    {
	      uint8_t byte = window[(src + offset) % WINDOW_SIZE];
	      fputc (byte, fout);
	      window[(win_pos + offset) % WINDOW_SIZE] = byte;
	    }
	  win_pos += len;
	}
    }
  if (ferror (fin) || ferror (fout))
    status = -1;
  fclose (fin);
  if (fclose (fout) != 0)
    status = -1;
  return status;
}
