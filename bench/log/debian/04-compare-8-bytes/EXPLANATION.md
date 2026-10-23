# 04-compare-8-bytes on debian

## What was measured

Issue #15, compare 8 bytes at a time in the match loop
(doc/optimisations/15-compare-8-bytes.md): `match_length` loads 8
bytes of the candidate and of the look-ahead as `uint64_t`, XORs them
and finds the first differing byte with `ctz`; the last 0 to 7 bytes
are compared one at a time.  The source measured is 836c993 (the #14
commit) plus raw/uncommitted.patch, which touches only src/lz77.c and
is exactly what the #15 commit contains.

Same procedure as before: `bench/campaign.sh 04-compare-8-bytes`
through ~/run-step.sh (touched sources, `make clean`), pinned to CPU 15,
REPEAT=9, full bench, archives, bench-compare and profile.  Debian's
tree was brought to 836c993 from a git bundle first.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-23 15:04 MSK
(raw/conditions.txt says 22:04Z: Debian's clock is about 10 hours
fast).  The machine had been rebooted three minutes before, after
being in use by its owner from 11:00.  raw/build.log shows lz77.c being
compiled.

The reboot reset `kernel.perf_event_paranoid` to 3, so the profile
stage failed (raw/profile-failed.log) and the campaign stopped before
writing SUMMARY.md.  After the setting was lowered to 1, the profile
stage alone was run again at about 23:20 MSK on the same build
(~/resume-profile.sh; raw/campaign.log records both attempts), and
SUMMARY.md was written then.  bench, archives and bench-compare are
from the first run.

## Results

| | 00-baseline | 03-no-memmove | 04-compare-8-bytes | vs previous | vs baseline |
|---|---|---|---|---|---|
| compression | 10.100 s | 7.040 s | 3.875 s | **1.82x** | **2.61x** |
| decompression | 2.068 s | 0.932 s | 0.928 s | 1.00x | 2.23x |
| compression throughput | 28.9 MB/s | 41.5 MB/s | 75.3 MB/s | | |
| ratio | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 03-no-memmove and to the baseline for
every corpus file (raw/archives.sha256), and identical to the
hppavillion archives of the same step.

perf stat on dickens, compression: 0.785 G → 0.499 G cycles, 1.366 G →
1.033 G instructions; IPC 1.74 → 2.07.  Conditional branches fall from
282 M to 182 M and mispredictions from 26.0 M to 14.9 M (cachegrind).

A follow-up A/B test after the profile rerun (raw/followup/ab-cycles.txt,
built by ~/ab-debian.sh, raw/followup/build14.log), compressing the
whole corpus, two alternating runs each:

| build | cycles | instructions |
|---|---|---|
| #14 (836c993) | 28.2 G / 28.2 G | 63.8 G |
| #15 | 16.2 G / 16.3 G | 41.3 G |

Per file in cycles, #15 against #14: mixed.dat 1.90x, text.dat 1.61x,
samba 1.58x, reymont 1.58x, mr 1.50x, nci 1.45x, mozilla 1.40x,
dickens, webster and xml 1.36x to 1.38x, ooffice 1.26x, zeros.dat
1.23x, sao 1.14x, osdb 1.10x, x-ray 0.97x and random.dat 0.93x.

## Interpretation

The same picture as on hppavillion: the gain follows the match
lengths.  Over the corpus 1.74x fewer cycles (hppavillion 1.66x), and
the wall-time 1.82x agrees with it.  Files with long repeats gain
most; x-ray (3 % more cycles) and random.dat (7 % more, hppavillion
13 %), whose candidates almost always differ in the first byte or two,
lose a little, because each such candidate now costs two 8-byte loads
and a `tzcnt` instead of one byte compare.  Zen 2 loses less there
than Tiger Lake.

The per-file wall times in ../SUMMARY.md overstate a few files against
the cycle counts (random.dat 1.01x and x-ray 1.11x in wall time, 0.93x
and 0.97x in cycles; mr and samba about 2.1x against 1.5x).  The
03-no-memmove and 04 runs are hours apart and on either side of a
reboot, so the cycle counts, taken back to back, are the ones to trust
per file; the whole-corpus numbers agree either way.

The profile on dickens is now flat: before, line 102 (the byte
compare) took 26 % of the samples and the chain-walk condition next to
it 13 %; now the hottest lines, the hash-table insert, the 8-byte load
in `match_length` and the chain walk, take about 8 % each.  What is
left of compression is spread over the hash chain, the hash and the
bit writer.

bench-compare (raw/variants.txt) has plain -O1 compressing the corpus
in 3.91 s against 4.12 s for -O2 and 4.17 s for -O3, and
-O3 -march=native 3.84 s; LTO adds nothing on top of -march=native, and
PGO gives about 5 % on compression.  Like the -O2 against -O3 result in
03-no-memmove on hppavillion, this is for #13, which decides the
release flags.
