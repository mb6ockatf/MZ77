# x-05-word on hppavillion

An extra run for 05-avx512bw: the same source and build, with
`MZ77_MATCH=word` in the environment, so the compressor takes the
portable 8-byte loop of #15 instead of the AVX-512 one.
`MACHINE=hppavillion MZ77_MATCH=word SKIP_COMPARE=1 SKIP_PROFILE=1
bench/campaign.sh x-05-word`, pinned to CPU 3, REPEAT=9, started
2026-10-24 00:14 MSK, right after 05-avx512bw and its follow-up.

| | 04-compare-8-bytes | x-05-word | 05-avx512bw |
|---|---|---|---|
| compression | 5.986 s | 5.707 s | 4.824 s |
| decompression | 1.308 s | 1.177 s | 1.171 s |

Archives byte-identical to 05-avx512bw and to the baseline for every
corpus file.

The wall times put the portable path between #15 and the AVX-512
path, but the clock frequency of this laptop varies between runs (see
the decompression times, which no step since 02-inline-bitio has
changed).  The cycle counts in ../05-avx512bw/raw/followup/ab-cycles.txt
are the reliable comparison: 18.6 G for this path, 18.2 G for #15,
14.9 G for AVX-512.  See ../05-avx512bw/EXPLANATION.md.
