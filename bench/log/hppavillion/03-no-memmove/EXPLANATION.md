# 03-no-memmove on hppavillion

## What was measured

Issue #14, one buffer for window and look-ahead
(doc/optimisations/14-no-memmove.md), from the main worktree: 08b2ec5
(the #12 commit) plus raw/uncommitted.patch, which touches only
src/lz77.c and is what the #14 commit contains.  `bench/campaign.sh
03-no-memmove`, pinned to CPU 3, REPEAT=9, after `make clean`
(raw/build.log shows the codec being compiled).

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-23 07:23 MSK.  The laptop was otherwise idle; the Debian run of
the same step was going on the other machine.

## Results

| | 00-baseline | 02-inline-bitio | 03-no-memmove | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 15.740 s | 15.416 s | 10.562 s | **1.46x** | **1.49x** |
| decompression | 2.503 s | 1.327 s | 1.286 s | 1.03x | 1.95x |
| compression throughput | 18.6 MB/s | 18.9 MB/s | 27.6 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives byte-identical to 02-inline-bitio, to the baseline and to the
debian archives of the same step.

perf stat on dickens, compression: 1.135 G → 0.784 G cycles, 2.406 G →
1.322 G instructions.

## Interpretation

The same picture as on Debian: compression is faster on every file,
most on random.dat (4.38x) and the binary files with short matches
(osdb 3.25x, ooffice 2.42x, x-ray 2.22x), least on text, because the
removed `memmove` cost one shift per token and data with few matches
has a token per byte.  On dickens 45 % fewer instructions and 31 %
fewer cycles, close to Debian's 44 % and 27 %.

The wall times are again disturbed by frequency changes: four
consecutive files (nci, ooffice, osdb, reymont) decompress about 1.43x
faster although decompression did not change, and their compression
speed-ups are inflated the same way.  The whole-corpus 1.46x is in line
with Debian's 1.41x and with the cycle counts, so it holds.

bench-compare (raw/variants.txt) shows plain -O2 compressing the corpus
in 8.59 s against 9.94 s for -O3 with GCC 16.2 (Debian, GCC 14.2:
6.69 s against 7.06 s).  This is worth looking at in #13, which
decides the release flags.
