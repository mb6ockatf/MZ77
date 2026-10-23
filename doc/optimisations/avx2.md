# AVX2 match compare (AMD Zen 2)

The counterpart of #16 for debian's Ryzen 7 5700U, which has AVX2 but
no AVX-512 and so still ran the 8-byte loop of #15 after #16.
Measured on debian; also measured on hppavillion with
`MZ77_MATCH=avx2` for comparison with its AVX-512 path.  This step has
no GitHub issue.

## Change

* `match_length_avx2` compares 32 bytes per step: two
  `_mm256_loadu_si256`, `_mm256_cmpeq_epi8`, `_mm256_movemask_epi8`
  gives one bit per equal byte; the first zero bit, `tzcnt(~mask)`, is
  the match length.  The tail (fewer than 32 bytes before the limit)
  goes to the 8-byte loop of #15, so nothing is read past the limit.
* A third copy of the compression loop, `compress_avx2`
  (`target ("avx2,bmi")`), inlines it.
* Run-time choice: AVX-512BW if present, else AVX2, else the 8-byte
  loop.  `MZ77_MATCH=avx2` or `MZ77_MATCH=word` forces a path (`avx2`
  only when the CPU supports it).

BMI2's `pext`/`pdep` were considered for the bit I/O and rejected: on
Zen 2 they are microcoded and take hundreds of cycles.

## Correctness

`make check`, strict `distcheck` in release and debug, and archives
byte-identical to the baseline on every path (AVX-512, AVX2 and the
8-byte loop) for the edge-case files of #15, the files in test/input
and every corpus file (raw/archives.sha256 of both machines and of
hppavillion's x-06-avx2).

## Complexity

Same bound, O(n * C * M) worst case; for a 258-byte match the compare
does 8 32-byte steps and 2 single-byte compares, against 32 8-byte
steps and 2 single-byte compares.

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before (#16) | after | speed-up | vs baseline |
|---|---|---|---|---|---|
| debian (Ryzen 7 5700U), AVX2 | compression | 3.920 s | 3.483 s | **1.13x** | **2.90x** |
| | decompression | 0.923 s | 0.944 s | (unchanged) | 2.19x |
| hppavillion (i3-1115G4), AVX-512 as before | compression | 4.824 s | 4.693 s | (unchanged) | 3.35x |
| | decompression | 1.171 s | 1.152 s | (unchanged) | 2.17x |

Cycle counts for compressing the whole corpus
(bench/log/*/06-avx2/raw/followup/ab-cycles.txt):

| machine | #16 | 8-byte loop | AVX2 | AVX-512 |
|---|---|---|---|---|
| debian | 16.2 G (8-byte loop) | 16.1 G | **14.5 G** | n/a |
| hppavillion | 14.9 G (AVX-512) | 18.0 G | **14.4 G** | 14.9 G |

On debian AVX2 takes 1.12x fewer cycles than #16, but nearly all of it
is mixed.dat (1.44x), the one file with long matches.  The twelve
silesia files together gain under 1 %: nci, samba and xml gain 6 % to
13 %, while dickens, osdb, sao, x-ray and random.dat lose 7 % to 10 %.
A variant that checks the first 8 bytes as one `uint64_t` before the
32-byte loop did not recover those files and was slower overall
(15.7 G); it is kept in bench/log/debian/06-avx2/raw/followup/ and not
in the source.  The cause of the loss on short-match files was not
established.

On hppavillion the default stays AVX-512, but the AVX2 path takes 3 %
fewer cycles than AVX-512 over the corpus and is as fast or faster on
every file except four that are within 2 %.  On this data the
AVX-512 path of #16 has no advantage over AVX2 on Tiger Lake; the
order of the run-time choice (AVX-512 first) is kept as it was
measured.

The details are in bench/log/debian/06-avx2/EXPLANATION.md,
bench/log/hppavillion/06-avx2/EXPLANATION.md and
bench/log/hppavillion/x-06-avx2/EXPLANATION.md.
