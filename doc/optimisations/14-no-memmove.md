# #14 One buffer for window and look-ahead (no memmove per token)

## Before

The compressor kept the 4 KiB sliding window in a ring buffer and the
4 KiB look-ahead in a separate array.  After every token it shifted the
whole look-ahead left with `memmove` (up to 4 KiB per token, so on
incompressible data about 4 KB of copying per input byte), and every byte
of a candidate match was fetched through `byte_at`, which chose between
the two arrays with a branch and a modulo.

## Change

* Window and look-ahead share one 64 KiB array `data`; `data_base` is the
  absolute position of `data[0]`, and the current position is a pointer
  into the array.  Advancing is pointer arithmetic.
* When the look-ahead drops below 258 bytes and fewer than 4 KiB of
  input are left in the array, the array is topped up with one
  `fread`.  If it has no room for that, the last 4 KiB of history plus
  what is left ahead (at most 8 KiB) first move to the front once.
  That is one move of at most 8 KiB per ~56 KiB of input instead of
  one move of up to 4 KiB per token.
* `byte_at` is gone: a candidate match and the look-ahead are both plain
  pointers into `data`, so the match loop is a straight byte compare.
* The look-ahead length the old program used (`buf_len`, refilled to
  4 KiB whenever it dropped below 258) is kept exactly as `look_len`,
  because it decides which positions near the end of a block get hash
  entries; keeping it keeps the archives byte-identical.

## Correctness

`make check`, and archives byte-identical to the baseline for the
edge-case files (empty, 1..9 bytes, 255, 256, 4095..4097,
65535..65537, 200 KB, random and repetitive), 1.5 MB slices of
dickens, mozilla, x-ray, mixed and zeros (which cross many buffer
moves), the files in test/input and every corpus file
(raw/archives.sha256 of both machines).

## Complexity

Copying drops from O(n * L) (L = 4096 look-ahead, worst case one shift
per literal) to O(n): each byte is moved at most once per 56 KiB block.
The match search is unchanged: O(n * C * M) worst case for chain length
C = 256 and match length M = 258.

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before (#12) | after | speed-up | vs baseline |
|---|---|---|---|---|---|
| debian (Ryzen 7 5700U) | compression | 9.893 s | 7.040 s | **1.41x** | **1.43x** |
| | decompression | 0.951 s | 0.932 s | (unchanged) | 2.22x |
| hppavillion (i3-1115G4) | compression | 15.416 s | 10.562 s | **1.46x** | **1.49x** |
| | decompression | 1.327 s | 1.286 s | (unchanged) | 1.95x |

perf stat on dickens, compression: 2.428 G to 1.366 G instructions and
1.071 G to 0.785 G cycles on debian; 2.406 G to 1.322 G instructions
and 1.135 G to 0.784 G cycles on hppavillion.

Every file compresses faster.  The gain follows the number of tokens
per byte, since the old program shifted the look-ahead once per token:
random.dat, nearly all literals, is 3.8x (debian) to 4.4x
(hppavillion) faster; binary files with short matches (osdb, sao,
x-ray, ooffice) about 2x to 3x; text about 1.3x; zeros.dat, one long
match after another, least.  Decompression is not touched.  The
details are in bench/log/debian/03-no-memmove/EXPLANATION.md and
bench/log/hppavillion/03-no-memmove/EXPLANATION.md.
