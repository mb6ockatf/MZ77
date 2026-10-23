# 07-lto-pgo on debian

## What was measured

Issue #13, link-time and profile-guided optimisation
(doc/optimisations/13-lto-pgo.md): `--enable-release` now compiles and
links with `-flto=auto` (with `gcc-ar`/`gcc-ranlib` for the static
codec library), and the new `make pgo` target builds an instrumented
program, trains it, and rebuilds with `-fprofile-use
-fprofile-partial-training`.  This step is measured after `make pgo`;
the same source without PGO, plain `--enable-release` with LTO, is the
extra run ../x-07-lto.  The source is 55149a1 (the AVX2 commit) plus
raw/uncommitted.patch, which touches only the build files
(configure.ac and four Makefile.am) and is exactly what the #13 commit
contains.  The codec source is unchanged.

The sequence was ~/run-13.sh: touch the sources, `make` (which reruns
autoconf and automake for the changed build files and keeps the
--enable-release configuration), `make clean`, `bench/campaign.sh
x-07-lto` with SKIP_PROFILE=1, then `make pgo` (raw/pgo.log) and
`bench/campaign.sh 07-lto-pgo`.  The campaign's own build stage found
everything up to date after `make pgo` (raw/build.log compiles
nothing), so the measured binary is the PGO one.  Pinned to CPU 15,
REPEAT=9.

## Conditions

As in 00-baseline (debian, GCC 14.2, CPU 15, powersave governor with
energy preference performance, on AC), started 2026-10-24 00:50 MSK,
right after x-07-lto (00:43) and `make pgo` (raw/conditions.txt says
07:50Z: Debian's clock is about 10 hours fast; the load average of
1.99 includes the PGO build that had just finished).

## Results

| | 00-baseline | 06-avx2 | x-07-lto | 07-lto-pgo | 07 vs 06 | 07 vs baseline |
|---|---|---|---|---|---|---|
| compression | 10.100 s | 3.483 s | 3.438 s | 3.290 s | **1.06x** | **3.07x** |
| decompression | 2.068 s | 0.944 s | 0.940 s | 0.901 s | **1.05x** | **2.29x** |
| ratio | 0.4753 | 0.4753 | 0.4753 | 0.4753 | | |

Archives are byte-identical to 06-avx2 and to the baseline for every
corpus file, with and without PGO (raw/archives.sha256,
../x-07-lto/raw/archives.sha256), and identical to the hppavillion
archives of the same step.

Per file (../SUMMARY.md), compression is 1.04x to 1.15x faster on every
file and decompression 1.02x to 1.10x on all but zeros.dat (0.90x,
0.006 s on a file that is one long match after another).

A follow-up A/B test after the run (raw/followup/ab-cycles.txt), the
build before #13 against the LTO and PGO build, two alternating runs
each:

| | before #13 | LTO + PGO | |
|---|---|---|---|
| compression, all files | 14.43 G cycles, 40.3 G instructions | 13.51 G, 33.0 G | 1.07x fewer cycles |
| compression, silesia only | 8.82 G, 21.1 G | 8.17 G, 17.6 G | 1.08x |
| decompression, all files | about 3.35 G | about 3.19 G | 1.05x |

## Interpretation

LTO alone gains 1 % in wall time (x-07-lto), in line with
bench-compare since 02-inline-bitio: the codec's hot code is in two
files and the bit I/O is already inlined through its header, so there
is little left to inline across files.

PGO gives the rest: 7 % fewer cycles in compression and 5 % in
decompression, with 18 % fewer instructions in compression.  The
training (`make pgo`) uses test/input and a 4 MB synthetic set from
`mz77-bench --generate`, which is the same generator, with the same
seeds, as the four generated files of the benchmark corpus; so the
generated part of the corpus is not independent of the training.  The
silesia files are: they are real data that the training never saw, and
they gain as much as the rest (1.08x fewer cycles in compression, 1.04x
to 1.11x in wall time per file), so the gain is not an artefact of
training on the test data.

raw/profile/ is built by bench/profile.sh with its own flags (-O2 -g,
no LTO, no PGO), so it shows the code as before #13, not the PGO
build.

bench-compare (raw/variants.txt), in which every variant has the
run-time choice, has -O3 at 3.53 s, -O3 -march=native at 3.47 s, with
LTO 3.40 s and with LTO and PGO 3.41 s for compression.  bench-compare
trains its PGO variant on the synthetic set only, on the default match
routine only and without -fprofile-partial-training, and uses
-march=native, so it is not the same build as `make pgo`; the release
build does not use -march=native, because the program has to run on
any x86-64 CPU and picks its match routine at run time instead.
