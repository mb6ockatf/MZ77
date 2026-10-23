# 01-block-io on debian

## What was measured

Issue #11, block I/O (doc/optimisations/11-block-io.md): the bit
reader, the bit writer and the decompressor's output go through their
own 64 KiB blocks and one `fread`/`fwrite` per block instead of one
`fgetc`/`fputc` per byte.  The source measured is 1f70c33 plus
raw/uncommitted.patch, which is exactly what the #11 commit contains
(including the measuring-script fixes: `--user-callchains` in
profile.sh, the extra fields in conditions.txt, x-* runs in
summarize.sh).

Same procedure as 00-baseline: `bench/campaign.sh 01-block-io`, pinned
to CPU 15, REPEAT=9, full bench, archives, bench-compare and profile.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, on AC), started
2026-10-23 01:21 MSK.  Debian's clock is about 10 hours fast; see
00-baseline.

A first attempt at this step was stopped and deleted: the sources had
been copied from the laptop with their modification times, which were
older than the baseline's object files on Debian's fast clock, so
`make` did not rebuild them.  The run kept here started from `make
clean` with touched sources; raw/build.log shows lz77.c and bitio.c
being compiled.

## Results

| | 00-baseline | 01-block-io | speed-up |
|---|---|---|---|
| compression | 10.100 s | 10.270 s | 0.98x |
| decompression | 2.068 s | 1.241 s | **1.67x** |
| decompression throughput | 141.2 MB/s | 235.3 MB/s | |
| ratio | 0.4753 | 0.4753 | |

Archives are byte-identical to the baseline for every corpus file
(raw/archives.sha256).  Per-file numbers are in ../SUMMARY.md:
decompression is 1.56x to 1.81x faster on every file except zeros.dat
(1.32x: almost all of its work is long copies, not I/O).

perf stat on dickens, decompression: 350 M → 216 M cycles, 862 M → 602
M instructions, IPC 2.46 → 2.79.  Compression: 1.150 G → 1.113 G
cycles, 2.75 G → 2.66 G instructions.

## Interpretation

Decompression was mostly libc call overhead: one `fgetc` per input
byte and one `fputc` per output byte.  Removing it cut 30 % of the
instructions and made decompression 1.67x faster.

Compression does not change.  The compressor reads its input in 4 KiB
`fread` chunks already, and the bit writer produces only ~2.6 bytes per
token, so `fputc` was a small cost next to the match search.  perf
counts 3 % fewer instructions, but wall times move between -15 % and +6
% per file (nci 0.85x, x-ray 1.06x) with no pattern, and the -O3 variant
in bench-compare is within 1.5 %: this is noise and code-layout
change, not an effect of the change.  The match search is the target
of #14 and #15.
