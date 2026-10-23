# 06-avx2 on hppavillion

## What was measured

The AVX2 match compare (doc/optimisations/avx2.md), measured mainly on
debian.  This machine has AVX-512BW, so by default it keeps the
AVX-512 path of #16 and this run measures the same code path as
05-avx512bw in a build that also contains the AVX2 copy of the loop.
The AVX2 path was measured as the extra run ../x-06-avx2
(`MZ77_MATCH=avx2`, bench and archives only).  The source measured is
02312cd (the #16 commit) plus raw/uncommitted.patch, which touches
only src/lz77.c.

`bench/campaign.sh 06-avx2`, pinned to CPU 3, REPEAT=9, after
`make clean` (raw/build.log shows the codec being compiled), right
after strict `distcheck` in release and debug.

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC), started
2026-10-24 00:27 MSK.  The laptop was otherwise idle; the Debian run
of the same step was going on the other machine.

## Results

| | 00-baseline | 05-avx512bw | 06-avx2 | x-06-avx2 | 06 vs 05 |
|---|---|---|---|---|---|
| compression | 15.740 s | 4.824 s | 4.693 s | 5.600 s | 1.03x |
| decompression | 2.503 s | 1.171 s | 1.152 s | 1.347 s | 1.02x |

Archives byte-identical to 05-avx512bw and to the baseline for every
corpus file, on the default path and with `MZ77_MATCH=avx2`
(x-06-avx2/raw/archives.sha256), and identical to the debian archives
of the same step.

A follow-up A/B test after both runs (raw/followup/ab-cycles.txt),
compressing the whole corpus, two alternating runs each:

| build and path | cycles | instructions |
|---|---|---|
| #16 (02312cd), AVX-512 | 14.82 G / 14.89 G | 36.48 G |
| this step, AVX-512 (default) | 14.83 G / 14.90 G | 36.48 G |
| this step, `MZ77_MATCH=avx2` | 14.38 G / 14.39 G | 38.84 G |
| this step, `MZ77_MATCH=word` | 18.07 G / 18.01 G | 45.14 G |

## Interpretation

The default path is unchanged: the same cycles and the same
instructions as #16.  The 1.03x in wall time and the x-06-avx2 wall
times (slower than the default although the cycle counts say
otherwise, and slower decompression although decompression did not
change) are frequency changes of this laptop again; the cycle counts
are the comparison to trust.

They show something the default choice does not take into account: on
this Tiger Lake CPU the AVX2 path takes 3 % fewer cycles than the
AVX-512 one over the corpus, although it runs 6 % more instructions.
Per file it is as fast or faster everywhere except random.dat, osdb,
sao and x-ray, which are within 1 % to 2 %; mixed.dat takes 7 % fewer
cycles.  Why was not established: a 64-byte step can only beat a
32-byte one on matches longer than 32 bytes, which are rare outside
mixed.dat, so the AVX-512 path has little to gain and apparently pays
a little more per candidate.  Whether to prefer AVX2 over AVX-512 when
both are present is left as a decision; the order measured here is
AVX-512 first.

bench-compare (raw/variants.txt) is again dominated by frequency
changes on this machine (-march=native slower than -O3, PGO slower
than LTO), so it says nothing reliable this time.
