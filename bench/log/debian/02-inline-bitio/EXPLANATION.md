# 02-inline-bitio on debian

## What was measured

Issue #12, inline bit I/O (doc/optimisations/12-inline-bitio.md):
`br_read` and `bw_write` become `static inline` functions in
src/bitio.h, the writer drains whole bytes only when 32 or more bits
are pending, and the reader refills its 64-bit buffer to more than 56
bits at once.  The source measured is cc7a482 (the #11 commit) plus
raw/uncommitted.patch, which touches only src/bitio.c and src/bitio.h
and is exactly what the #12 commit contains.

Same procedure as before: `bench/campaign.sh 02-inline-bitio` through
~/run-step.sh (touched sources, `make clean`), pinned to CPU 15,
REPEAT=9, full bench, archives, bench-compare and profile.  Debian's
tree was first brought to cc7a482 from a git bundle, so `base` in
raw/conditions.txt is the real parent commit.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-23 03:20 MSK
(raw/conditions.txt says 10:20Z: Debian's clock is about 10 hours
fast).  The load average of 1.00 is an interrupt thread
(irq/31-PNP0C50, an I2C HID device) that has stayed runnable since
boot and uses about 3 % of one CPU; it was there for every step.  raw/build.log shows
bitio.c and lz77.c being compiled.

## Results

| | 00-baseline | 01-block-io | 02-inline-bitio | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 10.100 s | 10.270 s | 9.893 s | 1.04x | 1.02x |
| decompression | 2.068 s | 1.241 s | 0.951 s | **1.30x** | **2.17x** |
| decompression throughput | 141.2 MB/s | 235.3 MB/s | 306.8 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 01-block-io and to the baseline for
every corpus file (raw/archives.sha256), and identical to the
hppavillion archives of the same step.

Per file (../SUMMARY.md), decompression is 1.14x to 2.04x faster on
every file except zeros.dat (1.00x, long copies only); random.dat,
which is almost all literals and so almost all bit reads, gains most.
Compression is 1.04x to 1.23x faster on every file except mixed.dat
(0.92x).

perf stat on dickens:

| | 01-block-io | 02-inline-bitio |
|---|---|---|
| compression cycles | 1.113 G | 1.071 G |
| compression instructions | 2.661 G | 2.428 G |
| decompression cycles | 216 M | 163 M |
| decompression instructions | 602 M | 346 M |
| decompression IPC | 2.79 | 2.12 |

## Interpretation

Decompression: inlining `br_read` and refilling 7 or 8 bytes at a time
removes 42 % of the instructions; cycles fall 25 %.  IPC drops from
2.79 to 2.12 because what is left is a serial chain: every token's
width depends on the flag bit read just before it, and every bit read
depends on the shift of the previous one.  The two steps together make
decompression 2.17x faster than the baseline.

Compression: 9 % fewer instructions and 4 % fewer cycles on dickens,
and 15 of 16 files are faster by 4 % to 23 %.  The whole-corpus number
(1.04x) is held down by mixed.dat, which takes 37 % of the compression
time and came out 8 % slower in wall time.  That is noise: a follow-up
A/B test with cycle counts (raw/followup/ab-cycles.txt) gives mixed.dat
14.39 G cycles with #11 and 12.91 G with #12, 10 % fewer, and mozilla,
nci and dickens 2 % to 13 % fewer.  The
writer was a small share of compression to begin with.

bench-compare (raw/variants.txt) shows that LTO no longer helps
decompression (0.959 s against 0.945 s for plain -O3): the cross-file
inlining it used to provide is now done by the header.  This matters
for #13.
