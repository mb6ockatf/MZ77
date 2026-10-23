#include <config.h>
#include "lz77.h"
#include "bitio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined __x86_64__ || defined __i386__
#define MZ77_X86 1
#include <immintrin.h>
#endif
#define WINDOW_SIZE 4096
#define MAX_MATCH   258
#define MIN_MATCH   3
#define HT_SIZE     65536
#define MAX_CHAIN   256
#define NO_POS      UINT64_MAX
#define DATA_SIZE   65536

typedef struct
{
  FILE *stream;
  size_t used;
  uint8_t bytes[BITIO_BLOCK_SIZE];
} OutputBlock;

static void
output_flush (OutputBlock *output)
{
  if (output->used > 0)
    fwrite (output->bytes, 1, output->used, output->stream);
  output->used = 0;
}

static inline void
output_byte (OutputBlock *output, uint8_t byte)
{
  if (output->used == sizeof (output->bytes))
    output_flush (output);
  output->bytes[output->used++] = byte;
}

static inline uint16_t
hash3 (const uint8_t *bytes)
{
  return ((uint16_t) bytes[0] << 10) ^ (bytes[1] << 4) ^ bytes[2];
}

static inline size_t
match_length (const uint8_t *candidate, const uint8_t *cursor, size_t limit)
{
  size_t len = 0;
  while (len + sizeof (uint64_t) <= limit)
    {
      uint64_t candidate_word, cursor_word;
      memcpy (&candidate_word, candidate + len, sizeof (candidate_word));
      memcpy (&cursor_word, cursor + len, sizeof (cursor_word));
      uint64_t difference = candidate_word ^ cursor_word;
      if (difference != 0)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	  return len + (size_t) __builtin_ctzll (difference) / 8;
#else
	  return len + (size_t) __builtin_clzll (difference) / 8;
#endif
	}
      len += sizeof (uint64_t);
    }
  while (len < limit && candidate[len] == cursor[len])
    len++;
  return len;
}

#ifdef MZ77_X86
__attribute__ ((target ("avx512bw,avx512vl")))
static inline size_t
match_length_avx512 (const uint8_t *candidate, const uint8_t *cursor,
		     size_t limit)
{
  size_t len = 0;
  while (len + 64 <= limit)
    {
      __m512i candidate_bytes = _mm512_loadu_si512 (candidate + len);
      __m512i cursor_bytes = _mm512_loadu_si512 (cursor + len);
      __mmask64 unequal = _mm512_cmpneq_epi8_mask (candidate_bytes,
						   cursor_bytes);
      if (unequal != 0)
	return len + (size_t) __builtin_ctzll (unequal);
      len += 64;
    }
  if (len < limit)
    {
      __mmask64 tail = (1ULL << (limit - len)) - 1;
      __m512i candidate_bytes = _mm512_maskz_loadu_epi8 (tail, candidate + len);
      __m512i cursor_bytes = _mm512_maskz_loadu_epi8 (tail, cursor + len);
      __mmask64 unequal = _mm512_mask_cmpneq_epi8_mask (tail, candidate_bytes,
							cursor_bytes);
      if (unequal != 0)
	return len + (size_t) __builtin_ctzll (unequal);
    }
  return limit;
}
#endif

#ifdef MZ77_X86
__attribute__ ((target ("avx2,bmi")))
static inline size_t
match_length_avx2 (const uint8_t *candidate, const uint8_t *cursor,
		   size_t limit)
{
  size_t len = 0;
  while (len + 32 <= limit)
    {
      __m256i candidate_bytes =
	_mm256_loadu_si256 ((const __m256i *) (candidate + len));
      __m256i cursor_bytes =
	_mm256_loadu_si256 ((const __m256i *) (cursor + len));
      uint32_t equal = (uint32_t)
	_mm256_movemask_epi8 (_mm256_cmpeq_epi8 (candidate_bytes,
						 cursor_bytes));
      if (equal != UINT32_MAX)
	return len + (size_t) _tzcnt_u32 (~equal);
      len += 32;
    }
  return len + match_length (candidate + len, cursor + len, limit - len);
}
#endif

enum MatchKind
{
  MATCH_WORD,
  MATCH_AVX2,
  MATCH_AVX512
};

static inline void
ht_insert (uint64_t *ht_heads, uint64_t *ht_next, const uint8_t *bytes,
	   uint64_t pos)
{
  uint16_t hash = hash3 (bytes);
  ht_next[pos % WINDOW_SIZE] = ht_heads[hash];
  ht_heads[hash] = pos;
}

__attribute__ ((always_inline))
static inline int
compress_stream (FILE *fin, FILE *fout, enum MatchKind kind)
{
  BitWriter bw;
  bw_init (&bw, fout);
  uint64_t *ht_heads = malloc (HT_SIZE * sizeof (uint64_t));
  uint64_t *ht_next = malloc (WINDOW_SIZE * sizeof (uint64_t));
  if (!ht_heads || !ht_next)
    {
      free (ht_heads);
      free (ht_next);
      return -1;
    }
  for (size_t slot = 0; slot < HT_SIZE; slot++)
    ht_heads[slot] = NO_POS;
  for (size_t slot = 0; slot < WINDOW_SIZE; slot++)
    ht_next[slot] = NO_POS;
  uint8_t data[DATA_SIZE];
  size_t data_len = fread (data, 1, sizeof (data), fin);
  uint64_t data_base = 0;
  uint64_t win_pos = 0;
  size_t look_len = data_len < WINDOW_SIZE ? data_len : WINDOW_SIZE;
  while (look_len > 0)
    {
      const uint8_t *cursor = data + (win_pos - data_base);
      size_t best_len = 0;
      uint64_t best_dist = 0;
      if (look_len >= MIN_MATCH)
	{
	  uint64_t pos = ht_heads[hash3 (cursor)];
	  int chain_left = MAX_CHAIN;
	  size_t limit = look_len < MAX_MATCH ? look_len : MAX_MATCH;
	  while (pos != NO_POS && pos < win_pos
		 && win_pos - pos <= WINDOW_SIZE && chain_left-- > 0)
	    {
	      const uint8_t *candidate = data + (pos - data_base);
	      size_t len;
#ifdef MZ77_X86
	      if (kind == MATCH_AVX512)
		len = match_length_avx512 (candidate, cursor, limit);
	      else if (kind == MATCH_AVX2)
		len = match_length_avx2 (candidate, cursor, limit);
	      else
#endif
		len = match_length (candidate, cursor, limit);
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
	  bw_write (&bw, cursor[0], 8);
	  advance = 1;
	}
      for (size_t offset = 0; offset < advance; offset++)
	{
	  if (offset + MIN_MATCH <= look_len)
	    ht_insert (ht_heads, ht_next, &cursor[offset], win_pos);
	  win_pos++;
	}
      look_len -= advance;
      if (look_len < MAX_MATCH)
	{
	  size_t start = (size_t) (win_pos - data_base);
	  size_t ahead = data_len - start;
	  if (ahead < WINDOW_SIZE)
	    {
	      if (data_len + (WINDOW_SIZE - ahead) > sizeof (data))
		{
		  size_t keep_from = start - WINDOW_SIZE;
		  memmove (data, data + keep_from, data_len - keep_from);
		  data_len -= keep_from;
		  data_base += keep_from;
		  start -= keep_from;
		}
	      data_len += fread (data + data_len, 1, sizeof (data) - data_len,
				 fin);
	      ahead = data_len - start;
	    }
	  look_len = ahead < WINDOW_SIZE ? ahead : WINDOW_SIZE;
	}
    }
  bw_finish (&bw);
  free (ht_heads);
  free (ht_next);
  return 0;
}

static int
compress_word (FILE *fin, FILE *fout)
{
  return compress_stream (fin, fout, MATCH_WORD);
}

#ifdef MZ77_X86
__attribute__ ((target ("avx2,bmi")))
static int
compress_avx2 (FILE *fin, FILE *fout)
{
  return compress_stream (fin, fout, MATCH_AVX2);
}

__attribute__ ((target ("avx512bw,avx512vl")))
static int
compress_avx512 (FILE *fin, FILE *fout)
{
  return compress_stream (fin, fout, MATCH_AVX512);
}
#endif

static enum MatchKind
match_kind (void)
{
  const char *forced = getenv ("MZ77_MATCH");
  if (forced && strcmp (forced, "word") == 0)
    return MATCH_WORD;
#ifdef MZ77_X86
  __builtin_cpu_init ();
  int has_avx2 = __builtin_cpu_supports ("avx2")
    && __builtin_cpu_supports ("bmi");
  if (forced && strcmp (forced, "avx2") == 0 && has_avx2)
    return MATCH_AVX2;
  if (__builtin_cpu_supports ("avx512bw")
      && __builtin_cpu_supports ("avx512vl"))
    return MATCH_AVX512;
  if (has_avx2)
    return MATCH_AVX2;
#endif
  return MATCH_WORD;
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
  int result;
  switch (match_kind ())
    {
#ifdef MZ77_X86
    case MATCH_AVX512:
      result = compress_avx512 (fin, fout);
      break;
    case MATCH_AVX2:
      result = compress_avx2 (fin, fout);
      break;
#endif
    default:
      result = compress_word (fin, fout);
      break;
    }
  int status = (result != 0 || ferror (fin) || ferror (fout)) ? -1 : 0;
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
  OutputBlock output;
  output.stream = fout;
  output.used = 0;
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
	  output_byte (&output, (uint8_t) literal);
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
	      output_byte (&output, byte);
	      window[(win_pos + offset) % WINDOW_SIZE] = byte;
	    }
	  win_pos += len;
	}
    }
  output_flush (&output);
  if (ferror (fin) || ferror (fout))
    status = -1;
  fclose (fin);
  if (fclose (fout) != 0)
    status = -1;
  return status;
}
