# 06-avx2 on debian

## What was measured

The AVX2 match compare (doc/optimisations/avx2.md), the counterpart of
#16 for this machine: the Ryzen 7 5700U (Zen 2) has AVX2 but no
AVX-512, so after #16 it still ran the 8-byte loop of #15.
`match_length_avx2` compares 32 bytes per step, a third copy of the
compression loop is compiled with `target ("avx2,bmi")`, and
`lz77_compress` picks it when the CPU has AVX2 and BMI1 but not
AVX-512BW.  This run takes the AVX2 path.  The source measured is
02312cd (the #16 commit) plus raw/uncommitted.patch, which touches
only src/lz77.c and is exactly what the commit contains.  There is no
GitHub issue for this step.

Same procedure as before: `bench/campaign.sh 06-avx2` through
~/run-step.sh (touched sources, `make clean`), pinned to CPU 15,
REPEAT=9, full bench, archives, bench-compare and profile.  Debian's
tree was brought to 02312cd from a git bundle first.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-24 00:25 MSK
(raw/conditions.txt says 07:25Z: Debian's clock is about 10 hours
fast).  raw/build.log shows lz77.c being compiled.

## Results

| | 00-baseline | 05-avx512bw | 06-avx2 | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 10.100 s | 3.920 s | 3.483 s | **1.13x** | **2.90x** |
| decompression | 2.068 s | 0.923 s | 0.944 s | 0.98x | 2.19x |
| compression throughput | 28.9 MB/s | 74.5 MB/s | 83.8 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 05-avx512bw and to the baseline for
every corpus file (raw/archives.sha256), and identical to the
hppavillion archives of the same step.

A follow-up A/B test after the run (raw/followup/ab-cycles.txt),
compressing the whole corpus, two alternating runs each:

| build | cycles | instructions |
|---|---|---|
| #16 (02312cd, 8-byte loop here) | 16.27 G / 16.16 G | 41.21 G |
| this step, `MZ77_MATCH=word` | 16.25 G / 15.97 G | 41.21 G |
| this step, AVX2 | 14.49 G / 14.45 G | 40.30 G |

Per file in cycles, AVX2 against #16: mixed.dat 1.44x, nci 1.13x, xml
1.08x, samba 1.06x, text.dat 1.04x, mozilla, mr, webster and zeros.dat
1.00x to 1.02x; and fewer than before on six files: ooffice and
reymont 0.97x, dickens, x-ray and random.dat 0.93x, osdb 0.92x and sao
0.90x.  The per-file wall times in ../SUMMARY.md agree with these
within a few per cent.

## Interpretation

Over the corpus AVX2 takes 1.12x fewer cycles, and the wall-time 1.13x
agrees, but almost all of it is mixed.dat, which has long matches and
took 35 % of the corpus's compression time after #16.  The twelve
silesia files together take 8.80 G cycles against 8.87 G with #16,
under 1 % fewer: files with long matches (nci, samba, xml) gain, files
whose matches are short or whose candidates differ early (dickens,
osdb, sao, x-ray, random.dat) lose 7 % to 10 %.  On dickens the
profile build shows the same: 0.525 G → 0.552 G cycles
(raw/profile/summary.txt).

A variant that compares the first 8 bytes as one `uint64_t` before
going to 32-byte steps, meant to keep the cost of early mismatches at
the level of #15, was measured as well and was worse everywhere
(15.7 G cycles over the corpus; raw/followup/variant-word-first.txt and
.patch).  So the loss on the short-match files does not come from the
first compare alone; the AVX2 copy of the loop is slower on them as a
whole.  Its cause was not established; the step is kept as measured
because it is a net gain on the corpus and costs at most 10 % on any
one file.

On hppavillion this step changes nothing by default (it keeps the
AVX-512 path), but with `MZ77_MATCH=avx2` the AVX2 path takes 3 %
fewer cycles there than AVX-512; see ../../hppavillion/06-avx2.

bench-compare (raw/variants.txt): -O2, -O3 and -march=native are within
3 % of each other (3.47 s to 3.56 s), LTO and PGO bring it to 3.39 s.
This is for #13.
