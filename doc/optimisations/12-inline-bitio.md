# #12 Inline bit I/O

## Before

`bw_write` and `br_read` lived in bitio.c, so every token cost two or
three calls across translation units that the compiler could not inline
without LTO.  `bw_write` also drained after every call, testing for whole
bytes each time, and `br_read` checked its arguments and refilled one
byte at a time.

## Change

* `br_read` and `bw_write` move into src/bitio.h as `static inline`
  functions; only the slow paths (`br_refill`, `bw_drain`) stay in
  bitio.c.
* The writer accumulates up to 31 pending bits in its 64-bit buffer and
  drains whole bytes only when 32 or more are waiting, so most calls are
  a shift, an OR and a compare.
* The reader refills its 64-bit buffer to at least 57 bits at once, so
  most calls are a shift and a mask.
* The argument checks are gone: all callers pass constant widths of 1,
  8 and 12.

## Correctness

As for #11: `make check`, and archives byte-identical to the baseline
for test/input, the edge-case files (empty, 1..9 bytes, 255, 256,
4095..4097, 65535..65537, 200 KB, random and repetitive) and every
corpus file (raw/archives.sha256 of both machines).

## Complexity

Unchanged, O(n).  Fewer calls and branches per token.

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before (#11) | after | speed-up | vs baseline |
|---|---|---|---|---|---|
| debian (Ryzen 7 5700U) | compression | 10.270 s | 9.893 s | 1.04x | 1.02x |
| | decompression | 1.241 s | 0.951 s | **1.30x** | **2.17x** |
| hppavillion (i3-1115G4) | compression | 13.871 s | 15.416 s | (noise) | |
| | decompression | 1.653 s | 1.327 s | **1.25x** | **1.89x** |

perf stat on dickens, decompression: 602 M to 346 M instructions and
216 M to 163 M cycles on debian; 536 M to 336 M instructions and 213 M
to 190 M cycles on hppavillion.  Compression: 9 % (debian) and 5 %
(hppavillion) fewer instructions.

Decompression loses about 40 % of its instructions; the remaining work
is a serial chain of dependent shifts and branches, so IPC falls and
the cycle count drops less (25 % on Zen 2, 11 % on Tiger Lake).
Compression gains a few percent on debian: 15 of 16 files compress 4 %
to 23 % faster in wall time, and a cycle-count A/B test shows mixed.dat,
the one file that looked slower, taking 10 % fewer cycles.  On
hppavillion (GCC 16.2) the wall times are disturbed by a frequency
change in the middle of the run, and the cycle-count A/B test shows a
small real loss: about 3.5 % more cycles for the whole corpus, mostly
in mixed.dat, mozilla and nci, the files whose time is in long
hash-chain walks.  It is most likely code layout of the match loop,
which #14 and #15 rewrite; it is checked again after them.  The details are in
bench/log/debian/02-inline-bitio/EXPLANATION.md and
bench/log/hppavillion/02-inline-bitio/EXPLANATION.md.

LTO, which #13 adds, no longer speeds up decompression in
bench-compare: the inlining across files it provided is now done by
the header.
