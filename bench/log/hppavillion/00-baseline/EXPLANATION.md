# 00-baseline on hppavillion

## What was measured

The starting point, `dev` at 1f70c33 (PR #9's code plus the measuring
scripts), measured with `bench/campaign.sh 00-baseline` from a clean
worktree of that commit: make check, `make bench REPEAT=9`, archive
checksums, `make bench-compare REPEAT=9` and `make profile`, pinned to
CPU 3.  See debian/00-baseline/EXPLANATION.md for what each stage does.

## Conditions

    machine:   hppavillion, Intel Core i3-1115G4 (Tiger Lake),
               2 cores / 4 threads, AVX-512
    compiler:  gcc (GCC) 16.2.0
    governor:  powersave with energy preference "performance"
               (intel_pstate), on AC
    started:   2026-10-23 01:03 MSK (22:02Z)

This laptop also runs the desktop and the Claude Code session that
drives the measurements, at the same time as the Debian runs.  It has
two cores, and turbo frequency depends on temperature, so its wall
times are much noisier than Debian's; compare its perf cycle and
instruction counts first.

As on Debian, the first pass of the profile stage failed (`perf record
-g` needs `--user-callchains` with `perf_event_paranoid=2`) and only that
stage was rerun with the fixed bench/profile.sh, at 01:28 MSK.

## Results

| | time | throughput |
|---|---|---|
| compression | 15.740 s | 18.55 MB/s |
| decompression | 2.503 s | 116.63 MB/s |
| ratio | 0.4753 | |

| variant | compress s | decompress s |
|---|---|---|
| O0 | 36.290 | 3.801 |
| O2 | 14.036 | 2.612 |
| O3 | 14.182 | 2.506 |
| O3-native | 15.139 | 2.665 |
| O3-native-lto | 13.887 | 2.059 |
| O3-native-lto-pgo | 13.118 | 2.141 |

Profile of dickens: gprof `lz77_compress` 34 %, `byte_at` 16 %,
`lz77_decompress` 13 %, `_init` 18 % (time in libc: `memmove`, `fgetc`,
`fputc`, which gprof cannot see).  perf stat: compression 1.21 G cycles
and 2.64 G instructions, decompression 0.35 G cycles and 0.81 G
instructions.

## Interpretation

The same picture as on Debian: the match search dominates compression,
per-byte libc calls dominate decompression.  The laptop is 1.5x slower
than the Ryzen at compression and 1.2x at decompression.
