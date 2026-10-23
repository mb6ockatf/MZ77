# #15 Compare 8 bytes at a time in the match loop

## Before

After #14 a candidate match is checked byte by byte: one load, compare
and branch per matching byte, for up to 258 bytes and up to 256
candidates per position.

## Change

`match_length` loads 8 bytes of the candidate and 8 bytes of the
look-ahead as `uint64_t` (with `memcpy`, so unaligned loads are legal),
XORs them, and if the result is non-zero the first differing byte is
`ctz(xor) / 8` on little-endian machines (`clz` on big-endian).  The last
0..7 bytes before the limit are compared one at a time.  The limit is
min(258, look-ahead), as before.

This is machine-dependent in the sense that it relies on fast unaligned
64-bit loads and a count-trailing-zeros instruction (x86-64 has both;
`tzcnt`/`bsf`), but it is portable C.

## Correctness

`make check`, and archives byte-identical to the baseline for the
edge-case files (empty, 1..9 bytes, 255, 256, 4095..4097,
65535..65537, 200 KB, random and repetitive), 1.5 MB slices of
dickens, mozilla, x-ray, mixed and zeros, the files in test/input and
every corpus file (raw/archives.sha256 of both machines).

## Complexity

Same bound, O(n * C * M) worst case; the match loop does about 1/8 of
the iterations on long matches.  On candidates that differ in the
first byte or two it does slightly more work than before (two 8-byte
loads and a `tzcnt` instead of one byte compare).

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before (#14) | after | speed-up | vs baseline |
|---|---|---|---|---|---|
| debian (Ryzen 7 5700U) | compression | 7.040 s | 3.875 s | **1.82x** | **2.61x** |
| | decompression | 0.932 s | 0.928 s | (unchanged) | 2.23x |
| hppavillion (i3-1115G4) | compression | 10.562 s | 5.986 s | **1.76x** | **2.63x** |
| | decompression | 1.286 s | 1.308 s | (unchanged) | 1.91x |

Cycle counts for compressing the whole corpus
(bench/log/*/04-compare-8-bytes/raw/followup/): on debian 28.2 G with
#14 and 16.3 G with #15 (1.74x fewer); on hppavillion 30.6 G with #14,
18.4 G with #15 (1.66x fewer), against 41.3 G for #11.

The gain follows the match lengths.  Files with long repeats gain most
(mixed.dat 2x fewer cycles, text.dat, samba, mr and reymont about
1.7x, dickens, mozilla and webster about 1.45x); x-ray, with short
matches, is unchanged (3 % more cycles on debian); random.dat, whose
candidates nearly always differ in the first byte or two, takes 13 %
more cycles on hppavillion and 7 % more on debian.  These are the
files that lose, and small ones: random data is not what an LZ77
archiver is for.  Decompression is not touched.  The details are in
bench/log/debian/04-compare-8-bytes/EXPLANATION.md and
bench/log/hppavillion/04-compare-8-bytes/EXPLANATION.md.
