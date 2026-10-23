# 00-baseline on debian

## What was measured

The starting point: `dev` at 1f70c33, which is the code merged in PR #9
(b5f6d00) plus the measuring scripts.  The codec is the original one:
per-byte `fgetc`/`fputc` in the bit reader, bit writer and decompressor,
a 4 KiB look-ahead shifted with `memmove` after every token, and a
byte-by-byte match loop through `byte_at`.

`bench/campaign.sh 00-baseline` ran, pinned to CPU 15:

* `make check` (round-trip tests);
* `make bench REPEAT=9`: the codec as shipped (`--enable-release`:
  `-O3 -DNDEBUG -fhardened`), every corpus file compressed and
  decompressed 9 times, median wall time per file (raw/bench.csv,
  raw/bench.txt);
* archive checksums of every corpus file (raw/archives.sha256), the
  reference every later step is compared with;
* `make bench-compare REPEAT=9`: -O0 to -O3, -march=native, LTO and PGO
  builds of the same code (raw/variants.csv, raw/variants.txt);
* `make profile`: gprof, perf stat/record, callgrind and cachegrind on
  silesia/dickens (raw/profile/).

## Conditions

    machine:   debian, AMD Ryzen 7 5700U (Zen 2), 8 cores / 16 threads
    compiler:  gcc (Debian) 14.2.0
    governor:  powersave with energy preference "performance"
               (amd-pstate-epp), on AC, idle desktop
    started:   2026-10-23 01:01 MSK

Debian's clock is about 10 hours fast, so the UTC times in
raw/conditions.txt and raw/profile/machine.txt read 08:00Z instead of
the real 22:01Z on 2026-10-22.  Durations are correct.

The profile stage failed in the first pass: `perf record -g` asks for
kernel call chains too, which `perf_event_paranoid=2` forbids.
bench/profile.sh got `--user-callchains` (committed with #11) and only
the profile stage was rerun on the same build; raw/profile.log is from
the rerun, raw/campaign.log records both.

## Results

Whole corpus (16 files, 296 MB), sums of per-file medians:

| | time | throughput |
|---|---|---|
| compression | 10.100 s | 28.91 MB/s |
| decompression | 2.068 s | 141.17 MB/s |
| ratio | 0.4753 | |

Compiler settings (raw/variants.txt), compression / decompression s:

| variant | compress s | decompress s |
|---|---|---|
| O0 | 21.877 | 3.130 |
| O2 | 10.474 | 2.072 |
| O3 | 10.120 | 1.989 |
| O3-native | 10.620 | 2.022 |
| O3-native-lto | 9.498 | 1.755 |
| O3-native-lto-pgo | 9.179 | 1.750 |

Profile of dickens (raw/profile/summary.txt):

* gprof: `lz77_compress` 41 % self, `lz77_decompress` 16 %, `bw_flush`
  11 %, `br_read` 11 %, `hash3` 10 %; `byte_at` is called 367 million
  times for 10 MB of input.
* perf stat, compression: 1.15 G cycles, 2.75 G instructions (IPC 2.39);
  decompression: 0.35 G cycles, 0.86 G instructions.
* The hottest source line is the match loop condition (lz77.c:84, 15 %).

## Interpretation

Compression is dominated by the match search (the inner byte loop with
`byte_at`), decompression by per-byte calls: `br_read` with `fgetc`
behind it, and `fputc` per output byte.  -O2 and above are equivalent;
LTO helps decompression by 12 % because it inlines the bit reader
across files, which points at #11 and #12.  Mixed.dat (a short phrase
with random bytes) is the slowest file by far, 3.5 s of the 10.1 s,
because every position has a long hash chain of short matches.
