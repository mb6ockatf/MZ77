# #11 Block I/O

## Before

The bit writer emitted every finished byte with `fputc`, the bit reader
fetched every byte with `fgetc`, and the decompressor wrote every output
byte with `fputc`.  stdio buffers these calls, but each one is still a
function call into libc with a lock check, a bounds check on the stdio
buffer and, for the GTK program's worker threads, locking of the
`FILE`.  The profile of the baseline (bench/log/*/00-baseline/raw/profile/)
shows these calls on the hot path of both directions.

## Change

* `BitWriter` and `BitReader` (src/bitio.h) get their own 64 KiB block.
  Finished bytes are stored into the block; the block goes to the file
  with one `fwrite` when it is full and in `bw_finish`.  The reader
  refills its block with one `fread` when it runs dry.
* The decompressor writes into a 64 KiB `OutputBlock` that is flushed
  with `fwrite` when full and at the end.
* Behaviour at end of input is unchanged: `br_read` returns -1 exactly
  when fewer bits remain than asked for.

The compressor's input already came in 4 KiB `fread` chunks and is
left alone here (#14 rewrites it).

## Correctness

* `make check` (round-trip tests).
* Archives byte-identical to the baseline for test/input, for empty,
  1..9-byte, block-boundary (4095..4097, 65535..65537) and 200 KB random
  and repetitive files, and for every corpus file (raw/archives.sha256).
* The first run of every corpus file in mz77-bench checks that
  decompression restores the input.

## Complexity

Unchanged: O(n) bit I/O work for n input bytes.  Only the constant per
byte drops: a store into an array instead of a libc call.

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before | after | speed-up |
|---|---|---|---|---|
| debian (Ryzen 7 5700U) | compression | 10.100 s | 10.270 s | 0.98x |
| | decompression | 2.068 s | 1.241 s | **1.67x** |
| hppavillion (i3-1115G4) | compression | 15.740 s | 13.871 s | (noise) |
| | decompression | 2.503 s | 1.653 s | **1.51x** |

perf stat on dickens, decompression: 350 M to 216 M cycles and 862 M
to 602 M instructions on debian; 348 M to 213 M cycles on hppavillion.

Decompression was limited by libc calls, one per input and one per
output byte; it is now 1.5 to 1.7 times faster.  Compression is
unchanged within noise (3 to 4 % fewer instructions): it already read
its input in chunks and writes only about 2.6 bytes per token, so its
time is in the match search, which #14 and #15 address.  The details
and the noise analysis are in bench/log/debian/01-block-io/EXPLANATION.md
and bench/log/hppavillion/01-block-io/EXPLANATION.md.
