# x-06-avx2 on hppavillion

An extra run for 06-avx2: the same source and build, with
`MZ77_MATCH=avx2` in the environment, so the compressor takes the AVX2
path instead of AVX-512.  `MACHINE=hppavillion MZ77_MATCH=avx2
SKIP_COMPARE=1 SKIP_PROFILE=1 bench/campaign.sh x-06-avx2`, pinned to
CPU 3, REPEAT=9, started 2026-10-24 00:37 MSK, right after 06-avx2
(the load average of 2.85 in raw/conditions.txt is from the 06-avx2
run and the build that just ended).

| | 06-avx2 (AVX-512) | x-06-avx2 (AVX2) |
|---|---|---|
| compression | 4.693 s | 5.600 s |
| decompression | 1.152 s | 1.347 s |

Archives byte-identical to 06-avx2 and to the baseline for every
corpus file.

The wall times are not usable: decompression, which is the same code
in both runs, came out 17 % slower here, so the clock frequency was
lower for this run.  The cycle counts in
../06-avx2/raw/followup/ab-cycles.txt say the opposite of the wall
times: 14.4 G cycles for the AVX2 path against 14.9 G for AVX-512.  See
../06-avx2/EXPLANATION.md.
