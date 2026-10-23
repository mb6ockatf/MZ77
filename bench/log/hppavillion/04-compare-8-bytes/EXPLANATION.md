# 04-compare-8-bytes on hppavillion

## What was measured

Issue #15, compare 8 bytes at a time in the match loop
(doc/optimisations/15-compare-8-bytes.md), from the main worktree:
836c993 (the #14 commit) plus raw/uncommitted.patch, which touches only
src/lz77.c and is what the #15 commit contains.  `bench/campaign.sh
04-compare-8-bytes`, pinned to CPU 3, REPEAT=9, after `make clean`
(raw/build.log shows the codec being compiled).

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-23 11:25 MSK.  The laptop was otherwise idle.  The Debian run
of this step was done later: the Debian machine was in use by its
owner from 11:00.

## Results

| | 00-baseline | 03-no-memmove | 04-compare-8-bytes | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 15.740 s | 10.562 s | 5.986 s | **1.76x** | **2.63x** |
| decompression | 2.503 s | 1.286 s | 1.308 s | 0.98x | 1.91x |
| compression throughput | 18.6 MB/s | 27.6 MB/s | 48.8 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives byte-identical to 03-no-memmove and to the baseline for every
corpus file.

perf stat on dickens, compression: 0.784 G → 0.520 G cycles, 1.322 G →
1.044 G instructions.

A follow-up A/B test after the run (raw/followup/ab-cycles.txt),
compressing the whole corpus, two alternating runs each:

| build | cycles | instructions |
|---|---|---|
| #11 (cc7a482) | 41.3 G / 41.4 G | 112.9 G |
| #14 (836c993) | 30.6 G / 30.7 G | 63.9 G |
| #15 | 18.4 G / 18.4 G | 45.1 G |

Per file, #15 takes fewer cycles than #14 on every file but one:
mixed.dat 1.99x, text.dat 1.71x, samba 1.79x, mr and reymont 1.67x,
dickens, mozilla, webster 1.45x to 1.48x, x-ray 1.00x, and random.dat
0.87x (13 % more cycles).

## Interpretation

The byte-by-byte match loop cost one load, compare and branch per
matching byte; comparing 8 bytes at once and finding the first
difference with `tzcnt` cuts that by up to 8 times on long matches.
The gain follows the match lengths: mixed.dat and the text files, with
long repeats, gain 1.5x to 2x in cycles; x-ray, whose matches are
short, gains nothing; random.dat, whose candidates almost always differ
in the first byte or two, does a little more work per candidate (two
8-byte loads and a `tzcnt` instead of one byte compare) and loses
13 %.  Over the corpus 1.66x fewer cycles; the wall-time 1.76x is in
line with that.

The per-file wall times are again disturbed by frequency changes:
mozilla to reymont decompress about 0.69x and webster to xml about
1.47x, with no change to decompression.  Trust the cycle counts per
file.

The GCC 16.2 question from 02-inline-bitio (#12 cost about 3.5 % more
compression cycles than #11 on this machine) is settled: #14 and #15
rewrote the match loop, and the whole corpus now compresses in 18.4 G
cycles against 41.3 G for #11, 2.24x fewer.  There is nothing left to
fix there.

bench-compare (raw/variants.txt): -O2 and -O3 are now close (5.65 s and
5.78 s), and PGO gives about 6 % on compression.
