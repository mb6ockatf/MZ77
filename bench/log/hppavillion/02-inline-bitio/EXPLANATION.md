# 02-inline-bitio on hppavillion

## What was measured

Issue #12, inline bit I/O (doc/optimisations/12-inline-bitio.md), from
the main worktree: cc7a482 (the #11 commit) plus
raw/uncommitted.patch, which touches only src/bitio.c and src/bitio.h
and is what the #12 commit contains.  `bench/campaign.sh
02-inline-bitio`, pinned to CPU 3, REPEAT=9, after `make clean`
(raw/build.log shows the codec being compiled).

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-23 03:22 MSK.  The laptop was otherwise idle; the Debian run of
the same step was going on the other machine.

## Results

| | 00-baseline | 01-block-io | 02-inline-bitio | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 15.740 s | 13.871 s | 15.416 s | 0.90x (noise, see below) | 1.02x |
| decompression | 2.503 s | 1.653 s | 1.327 s | **1.25x** | **1.89x** |
| decompression throughput | 116.6 MB/s | 176.6 MB/s | 220.0 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives byte-identical to 01-block-io, to the baseline and to the
debian archives of the same step.

perf stat on dickens:

| | 01-block-io | 02-inline-bitio |
|---|---|---|
| compression cycles | 1.149 G | 1.135 G |
| compression instructions | 2.537 G | 2.406 G |
| decompression cycles | 213 M | 190 M |
| decompression instructions | 536 M | 336 M |
| decompression IPC | 2.51 | 1.77 |

## Interpretation

Decompression: 37 % fewer instructions and 11 % fewer cycles on
dickens; 1.25x faster over the corpus in wall time.  As on Debian, IPC
falls because the remaining work is a serial chain of dependent shifts
and branches.  Tiger Lake gains less per cycle than Zen 2 here (11 %
against 25 %).

Compression: the 0.90x in wall time is mostly noise, but there is a
small real slowdown under it.  The per-file table in ../SUMMARY.md
has a run of consecutive files (ooffice, osdb, reymont, samba, sao,
webster) that are all about 0.7x in compression and about 0.85x in
decompression, while mozilla and mr just before them are 1.4x faster:
the pattern of a turbo-frequency change during the run, seen in
01-block-io as well.

A follow-up A/B test after the run (raw/followup/) settles it.  Wall
time cannot: five alternating rounds of #11 and #12 gave whole-corpus
compression totals from 13.6 s to 22.2 s for the same binary.  Cycle
counts, which do not depend on the clock frequency, can: compressing
the whole corpus takes 43.1 G to 43.8 G cycles with #12 against
41.6 G to 42.0 G with #11, about 3.5 % more, although #12 runs 2.4 %
fewer instructions.  Most of the loss is in mixed.dat (6 % more
cycles), mozilla and nci (4 %); most other files are 1 % to 5 %
faster.  On Debian (GCC 14.2) the same files take 2 % to 13 % fewer
cycles with #12, mixed.dat 10 % fewer.

So with GCC 16.2 on Tiger Lake, inlining the writer into the
compressor's loop costs a few percent in the files whose time is in
long hash-chain walks, most likely through code layout or register
allocation of the match loop rather than through the bit writer
itself.  #14 and #15 rewrite that loop, so this is to be looked at
again after them rather than worked around here.
