# 01-block-io on hppavillion

## What was measured

Issue #11, block I/O (doc/optimisations/11-block-io.md), from the main
worktree: 1f70c33 plus raw/uncommitted.patch, which is what the #11
commit contains.  `bench/campaign.sh 01-block-io`, pinned to CPU 3,
REPEAT=9, after `make clean` (raw/build.log shows the codec being
compiled).

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-23 01:28 MSK.  The Claude Code session copied the Debian logs
to this machine and wrote files during the run, a few seconds of light
I/O.

## Results

| | 00-baseline | 01-block-io | speed-up |
|---|---|---|---|
| compression | 15.740 s | 13.871 s | 1.13x (noise, see below) |
| decompression | 2.503 s | 1.653 s | **1.51x** |
| decompression throughput | 116.6 MB/s | 176.6 MB/s | |
| ratio | 0.4753 | 0.4753 | |

Archives byte-identical to the baseline for every corpus file.

perf stat on dickens:

| | 00-baseline | 01-block-io |
|---|---|---|
| compression cycles | 1.215 G | 1.149 G |
| compression instructions | 2.639 G | 2.537 G |
| decompression cycles | 0.348 G | 0.213 G |
| decompression instructions | 0.811 G | 0.536 G |

## Interpretation

Decompression: 1.51x faster in wall time, 1.63x fewer cycles and 34 %
fewer instructions on dickens, consistent with Debian (1.67x).

Compression: the 1.13x in wall time is not real.  Per file it ranges
from 0.74x (xml, x-ray, random.dat) to 1.54x (samba), and the faster
and slower files come in runs (ooffice to sao all about 1.5x, the files
before and after them not), which is what a change in turbo frequency
during the run looks like.  The cycle count drops 5 % and instructions
4 %, the same small gain as on Debian, where wall time did not move.
