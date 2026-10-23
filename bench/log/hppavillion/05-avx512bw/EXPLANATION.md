# 05-avx512bw on hppavillion

## What was measured

Issue #16, AVX-512BW match compare (doc/optimisations/16-avx512bw.md):
`match_length_avx512` compares 64 bytes per step with one masked
compare, and the compression loop is compiled a second time with
`target ("avx512bw,avx512vl")`; `lz77_compress` picks that copy at run
time when the CPU has AVX-512BW and VL.  The i3-1115G4 (Tiger Lake)
has both, so this run takes the AVX-512 path.  The source measured is
c1c1f4a (the squashed #14 and #15 commit) plus raw/uncommitted.patch,
which touches only src/lz77.c and is what the #16 commit contains.

`bench/campaign.sh 05-avx512bw`, pinned to CPU 3, REPEAT=9, after
`make clean` (raw/build.log shows the codec being compiled).  The
portable path of the same build was measured as well, as the extra run
../x-05-word (`MZ77_MATCH=word`, bench and archives only).

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-24 00:02 MSK.  The laptop was otherwise idle; the Debian run
of the same step was going on the other machine.  x-05-word started at
00:14, right after the cycle-count follow-up below.

## Results

| | 00-baseline | 04-compare-8-bytes | 05-avx512bw | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 15.740 s | 5.986 s | 4.824 s | **1.24x** | **3.26x** |
| decompression | 2.503 s | 1.308 s | 1.171 s | 1.12x (noise) | 2.14x |
| compression throughput | 18.6 MB/s | 48.8 MB/s | 60.5 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

x-05-word, the same build with `MZ77_MATCH=word`: compression 5.707 s,
decompression 1.177 s.

Archives byte-identical to 04-compare-8-bytes and to the baseline for
every corpus file, with the AVX-512 path and with `MZ77_MATCH=word`
(x-05-word/raw/archives.sha256), and identical to the debian archives
of the same step.

A follow-up A/B test after the run (raw/followup/ab-cycles.txt),
compressing the whole corpus, two alternating runs each:

| build | cycles | instructions |
|---|---|---|
| #15 (c1c1f4a) | 18.1 G / 18.3 G | 45.1 G |
| #16, `MZ77_MATCH=word` | 18.6 G / 18.6 G | 45.1 G |
| #16, AVX-512 | 14.9 G / 14.9 G | 36.5 G |

Per file in cycles, the AVX-512 path against #15: mixed.dat 1.43x, nci
1.25x, text.dat, samba, xml and webster 1.15x to 1.16x, mozilla and mr
about 1.1x, the other files 1.04x to 1.08x, and random.dat 0.96x.

## Interpretation

Over the corpus the AVX-512 path takes 1.22x fewer cycles than #15 and
19 % fewer instructions; the wall-time 1.24x agrees.  The gain is
smaller than #15's because the 8-byte loop had already removed most
of the per-byte work: a 64-byte step helps only on matches longer than
8 bytes, so mixed.dat, with its long repeats, gains most, and most
silesia files, whose matches are short, gain 4 % to 10 %.  random.dat
loses 4 %: almost every candidate differs in the first bytes, where
the 64-byte compare does more work than the 8-byte one.

The portable path in the same binary costs about 2 % more cycles than
#15 (18.6 G against 18.2 G) with the same instruction count, so it is
code layout, not extra work; the run-time choice is made once per
file.  On debian, the fallback path came out 0.4 % faster than #15.

The per-file wall times in ../SUMMARY.md are again disturbed by
frequency changes: random.dat, text.dat, zeros.dat and reymont to sao
decompress 1.4x to 1.5x faster although decompression did not change,
and their compression speed-ups are inflated the same way.  Trust the
cycle counts per file.

The hottest lines on dickens (raw/profile/summary.txt) are now the
mask test and `ctz` after the 64-byte compare (lz77.c:87 and 88,
17 %), then the chain walk and the hash-table insert.

bench-compare (raw/variants.txt): every variant has the run-time
choice, so all of them take the AVX-512 path here.  -O2 and -O3 are
close (5.54 s and 5.60 s), -march=native gives about 12 % on top of
-O3 (4.93 s), and LTO and PGO bring it to 4.54 s.  This is for #13.
