# #13 Link-time and profile-guided optimisation

## Before

`--enable-release` compiled every file with `-O3 -DNDEBUG -fhardened`
on its own; nothing was optimised across translation units, and no
build used a run-time profile.

## Change

* `--enable-release` now compiles and links with `-flto=auto`, and
  configure picks `gcc-ar`/`gcc-ranlib` so the static codec library
  keeps its LTO index.  The codec, the GTK program, the test and the
  benchmark driver are optimised across translation units.
* With `-flto` on the link line, GCC prints "linker hardening options
  not enabled by '-fhardened' because other link options were
  specified on the command line", so the release profile passes the
  linker options itself: `-Wl,-z,relro,-z,now` (PIE is already the
  default).  `readelf -d src/mz77` shows BIND_NOW and PIE as before.
  The warning itself remains and is harmless; the strict CI build,
  which adds `-Werror`, passes.
* `make pgo` builds an instrumented program, trains it on test/input
  and a synthetic set (`mz77-bench --generate --size=4000000`) with
  every match routine the CPU supports (`MZ77_MATCH` unset, `avx2` and
  `word`), and rebuilds with `-fprofile-use
  -fprofile-partial-training`, which keeps code that the training did
  not reach (for example the AVX-512 routine on a CPU without it)
  optimised normally instead of for size.  `make clean` removes the
  profile data.

The synthetic training set comes from the same generator, with the
same seeds, as the four generated files of the benchmark corpus (it is
their first 4 MB).  The silesia files, which are real data, are not
used for training, so they are the fair test of PGO below.

-march=native is not used: the release program has to run on any
x86-64 CPU and chooses its match routine at run time instead.

## Correctness

`make check` and strict `distcheck` in release (now with LTO) and
debug; archives byte-identical to the baseline for every corpus file
on both machines, with LTO alone (x-07-lto) and with LTO and PGO
(07-lto-pgo).  The codec source does not change in this step.

## Results

Whole corpus, sums of per-file medians (bench/log/*/SUMMARY.md):

| machine | | before (AVX2 step) | LTO | LTO + PGO | speed-up | vs baseline |
|---|---|---|---|---|---|---|
| debian (Ryzen 7 5700U) | compression | 3.483 s | 3.438 s | 3.290 s | **1.06x** | **3.07x** |
| | decompression | 0.944 s | 0.940 s | 0.901 s | **1.05x** | **2.29x** |
| hppavillion (i3-1115G4) | compression | 4.693 s | 5.404 s | 6.040 s | (see below) | |
| | decompression | 1.152 s | 1.374 s | 1.284 s | (see below) | |

Cycle counts for compressing the whole corpus
(bench/log/*/07-lto-pgo/raw/followup/):

| machine | before | LTO | LTO + PGO |
|---|---|---|---|
| debian | 14.43 G | n/a | 13.51 G (1.07x fewer) |
| debian, silesia only | 8.82 G | n/a | 8.17 G (1.08x fewer) |
| hppavillion, median of 7 | 15.14 G | 14.70 G (3 % fewer) | 14.97 G |

On debian PGO is what counts: LTO alone gains 1 % in wall time, LTO
and PGO 6 % in compression and 5 % in decompression, with 18 % fewer
instructions in compression.  The silesia files, which the training
never saw, gain as much as the generated ones, so the gain is not an
artefact of the training data.

On hppavillion the laptop was in use during the run and the wall times
are not usable (they range from 0.31x to 1.82x per file).  In cycles
LTO is about 3 % better than before; PGO removes 13 % of the
instructions but no cycles, and lands between the two.  The cycle
counts themselves vary by up to 6 % between rounds there, so a
measurement on an idle laptop would be needed to settle PGO on Tiger
Lake.

LTO stays in `--enable-release` (it costs nothing and helps a little
on both machines); PGO stays a separate `make pgo` target.  The
details are in bench/log/debian/07-lto-pgo/EXPLANATION.md,
bench/log/debian/x-07-lto/EXPLANATION.md,
bench/log/hppavillion/07-lto-pgo/EXPLANATION.md and
bench/log/hppavillion/x-07-lto/EXPLANATION.md.
