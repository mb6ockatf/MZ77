# 03-no-memmove on debian

## What was measured

Issue #14, one buffer for window and look-ahead
(doc/optimisations/14-no-memmove.md): the compressor keeps history and
look-ahead in one 64 KiB array and moves a pointer instead of shifting
the look-ahead with `memmove` after every token; `byte_at` and the ring
buffer are gone.  The source measured is 08b2ec5 (the #12 commit) plus
raw/uncommitted.patch, which touches only src/lz77.c and is exactly
what the #14 commit contains.

Same procedure as before: `bench/campaign.sh 03-no-memmove` through
~/run-step.sh (touched sources, `make clean`), pinned to CPU 15,
REPEAT=9, full bench, archives, bench-compare and profile.  Debian's
tree was brought to 08b2ec5 from a git bundle first.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-23 07:22 MSK
(raw/conditions.txt says 14:22Z: Debian's clock is about 10 hours
fast).  raw/build.log shows lz77.c being compiled.

## Results

| | 00-baseline | 02-inline-bitio | 03-no-memmove | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 10.100 s | 9.893 s | 7.040 s | **1.41x** | **1.43x** |
| decompression | 2.068 s | 0.951 s | 0.932 s | 1.02x | 2.22x |
| compression throughput | 28.9 MB/s | 29.5 MB/s | 41.5 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 02-inline-bitio and to the baseline for
every corpus file (raw/archives.sha256), and identical to the
hppavillion archives of the same step.

Per file (../SUMMARY.md), compression is faster on every file: 1.08x
on zeros.dat, 1.13x to 1.38x on text-like files, 2.05x to 2.24x on
osdb, sao and x-ray, and 3.79x on random.dat.

perf stat on dickens, compression: 1.071 G → 0.785 G cycles, 2.428 G →
1.366 G instructions (44 % fewer).

## Interpretation

The old compressor shifted the whole look-ahead (up to 4 KiB) after
every token.  On data with few matches that is one shift per byte, so
random.dat, which is almost all literals, gains most (3.79x), followed
by the binary files with short matches (osdb, sao, x-ray, about 2x).
Text compresses with longer matches, so fewer shifts per byte, and
gains about 1.3x; zeros.dat is one long match after another and
gains least.  Besides the copying, every compared byte used to go
through `byte_at` (a branch and a modulo); now the match loop compares
two plain pointers, which is the rest of the drop in instructions.

What is left in compression is the match search itself: the
byte-by-byte compare and the hash-chain walk.  #15 attacks the first.

Decompression is untouched by this change.  The 2 % in wall time and
the 6 % more cycles in perf stat on dickens (163 M → 173 M) are noise
and code layout.
