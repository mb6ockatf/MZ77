# 05-avx512bw on debian

## What was measured

Issue #16, AVX-512BW match compare (doc/optimisations/16-avx512bw.md).
The Ryzen 7 5700U (Zen 2) has no AVX-512, so here `lz77_compress`
takes the portable 8-byte loop of #15; this run checks that the
run-time choice and the second copy of the compression loop cost
nothing on a CPU without AVX-512.  The source measured is c1c1f4a (the
squashed #14 and #15 commit) plus raw/uncommitted.patch, which touches
only src/lz77.c and is exactly what the #16 commit contains.

Same procedure as before: `bench/campaign.sh 05-avx512bw` through
~/run-step.sh (touched sources, `make clean`), pinned to CPU 15,
REPEAT=9, full bench, archives, bench-compare and profile.  Debian's
tree was moved to the squashed history first (git bundle of
08b2ec5..dev, `git reset --hard` to it, since 836c993 and a239af8 were
replaced by c1c1f4a).

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-24 00:01 MSK
(raw/conditions.txt says 07:01Z: Debian's clock is about 10 hours
fast).  `kernel.perf_event_paranoid` was 1, so the profile stage ran.
raw/build.log shows lz77.c being compiled.

## Results

| | 00-baseline | 04-compare-8-bytes | 05-avx512bw | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 10.100 s | 3.875 s | 3.920 s | 0.99x | 2.58x |
| decompression | 2.068 s | 0.928 s | 0.923 s | 1.01x | 2.24x |
| compression throughput | 28.9 MB/s | 75.3 MB/s | 74.5 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 04-compare-8-bytes and to the baseline
for every corpus file (raw/archives.sha256), and identical to the
hppavillion archives of the same step, which took the AVX-512 path.

A follow-up A/B test after the run (raw/followup/ab-cycles.txt),
compressing the whole corpus, three alternating runs each:

| build | cycles | instructions |
|---|---|---|
| #15 (c1c1f4a) | 16.33 G / 16.24 G / 16.32 G | 41.30 G |
| #16 (fallback) | 16.23 G / 16.18 G / 16.24 G | 41.21 G |

## Interpretation

The fallback costs nothing: the release build takes 0.4 % fewer cycles
and 0.2 % fewer instructions than #15 over the corpus.  The 1 % in wall
time, the same on every file, is within the run-to-run spread of this
machine (04 and 05 were measured nine hours apart, on either side of
the profile rerun).  The CPU check runs once per file, and the
compression loop of the word copy is the same code as in #15.

perf stat on dickens in the profile build (-O2 -g, no -fhardened)
shows 0.499 G → 0.525 G cycles and 1.033 G → 1.087 G instructions.
That build lays out and inlines the code differently from the release
build; the release-build A/B above is the comparison that matters.

bench-compare (raw/variants.txt): -O1, -O2 and -O3 are now within
1.5 % of each other (3.89 s to 3.95 s), -march=native is 3 % faster
than -O3, LTO adds nothing, and PGO about 3 %.  None of these builds
can take the AVX-512 path on this CPU; the AVX2 step is the
counterpart for Zen 2.
