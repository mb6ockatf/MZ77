# Optimisation results: where to find them and how to reproduce them

This file is the entry point for the coursework report.  It says which
numbers to quote, where every number comes from, and how to measure
them again.

## 1. Where everything is

| what | where |
|---|---|
| **numbers for the report, step by step** (recommended) | section 2 below; source files bench/log/debian/steps/steps.txt and bench/log/hppavillion/steps/steps.txt |
| what each optimisation changes, and its results | doc/optimisations/NN-NAME.md, one file per step (list in doc/optimisations/README) |
| wall-clock times of every step, per file and in total | bench/log/MACHINE/SUMMARY.md |
| everything one step's measurement printed, unedited | bench/log/MACHINE/NN-STEP/raw/ (layout in bench/log/README) |
| what one measurement means, and its conditions | bench/log/MACHINE/NN-STEP/EXPLANATION.md |
| extra runs of the same build on another code path | bench/log/MACHINE/x-NN-NAME/ |
| follow-up cycle counts made right after a step | bench/log/MACHINE/NN-STEP/raw/followup/ |
| how the tools work | bench/README |

MACHINE is `debian` (AMD Ryzen 7 5700U, Zen 2, AVX2, GCC 14.2) or
`hppavillion` (Intel Core i3-1115G4, Tiger Lake, AVX-512, GCC 16.2).
Numbers from the two machines are never compared with each other, only
steps on the same machine.

The corpus is the Silesia corpus (12 real files, 211 MB) plus four
generated files of 20 MB each (text, random, zeros, mixed); see
bench/README.  The compression ratio is 0.4753 at every step: no step
changes the archive format, and every step's archives are
byte-identical to the original program's (raw/archives.sha256).

## 2. Numbers for the report

### Which numbers to use

Use the **cycle counts** of bench/steps.sh for "step N made it X %
faster".  They were measured at the end, back to back, every step
rebuilt from its commit, five rounds each on one pinned core, median
taken.  CPU cycles do not change with the clock frequency, which on the
laptop moved by up to 40 % between the wall-clock runs.

Use **wall-clock seconds** from bench/log/debian/SUMMARY.md when you
need absolute times or MB/s; the Ryzen machine kept a stable clock.
The laptop's wall-clock numbers (bench/log/hppavillion/SUMMARY.md) are
unreliable from step 01 on and should not be quoted as speed-ups.

How to read the speed-up: **1.52x** means the step runs 1.52 times as
fast as before, which is **52 % faster**, or **34 % less time**
(1 - 1/1.52).  Pick one wording and keep it through the report.

### Compression, whole corpus (CPU cycles, lower is better)

"vs previous" compares a step with the step before it; "vs baseline"
with the original program.

| step | what | debian cycles | vs previous | vs baseline | hppavillion cycles | vs previous | vs baseline |
|---|---|---|---|---|---|---|---|
| 00 | original program | 41.85 G | | 1.00x | 42.15 G | | 1.00x |
| 01 | #11 block I/O | 39.93 G | 1.05x | 1.05x | 41.68 G | 1.01x | 1.01x |
| 02 | #12 inline bit I/O | 41.94 G | 0.95x | 1.00x | 40.86 G | 1.02x | 1.03x |
| 03 | #14 no memmove | 27.62 G | **1.52x** | 1.52x | 31.65 G | **1.29x** | 1.33x |
| 04 | #15 compare 8 bytes | 16.65 G | **1.66x** | 2.51x | 17.99 G | **1.76x** | 2.34x |
| 05 | #16 AVX-512 | 16.29 G | 1.02x (not used: no AVX-512) | 2.57x | 14.90 G | **1.21x** | 2.83x |
| 06 | AVX2 | 14.40 G | **1.13x** | 2.91x | 14.69 G | 1.01x (not used: AVX-512 is preferred) | 2.87x |
| 07 | #13 LTO only | 14.19 G | 1.01x | 2.95x | 14.66 G | 1.00x | 2.88x |
| 07 | #13 LTO + PGO | 13.47 G | **1.05x** (vs LTO) | **3.11x** | 14.80 G | 0.99x (vs LTO) | **2.85x** |

### Decompression, whole corpus (CPU cycles)

| step | what | debian cycles | vs previous | vs baseline | hppavillion cycles | vs previous | vs baseline |
|---|---|---|---|---|---|---|---|
| 00 | original program | 7.57 G | | 1.00x | 7.38 G | | 1.00x |
| 01 | #11 block I/O | 4.64 G | **1.63x** | 1.63x | 4.42 G | **1.67x** | 1.67x |
| 02 | #12 inline bit I/O | 3.24 G | **1.43x** | 2.34x | 3.42 G | **1.29x** | 2.16x |
| 03 to 06 | compression-only steps | 3.24 to 3.34 G | 0.98x to 1.00x | 2.27x to 2.34x | 3.36 to 3.40 G | 0.99x to 1.02x | 2.17x to 2.20x |
| 07 | #13 LTO only | 3.32 G | 1.00x | 2.28x | 3.33 G | 1.02x | 2.22x |
| 07 | #13 LTO + PGO | 3.14 G | **1.06x** (vs LTO) | **2.41x** | 3.10 G | **1.07x** (vs LTO) | **2.38x** |

### Wall-clock time on debian (seconds, whole corpus)

From bench/log/debian/SUMMARY.md, sums of per-file medians of 9 runs,
the release build of each step:

| step | compression | decompression | compression MB/s |
|---|---|---|---|
| 00 original | 10.10 s | 2.07 s | 28.9 |
| 01 #11 | 10.27 s | 1.24 s | 28.4 |
| 02 #12 | 9.89 s | 0.95 s | 29.5 |
| 03 #14 | 7.04 s | 0.93 s | 41.5 |
| 04 #15 | 3.88 s | 0.93 s | 75.3 |
| 05 #16 | 3.92 s | 0.92 s | 74.5 |
| 06 AVX2 | 3.48 s | 0.94 s | 83.8 |
| 07 #13 LTO + PGO | 3.29 s | 0.90 s | 88.7 |

Overall: compression **10.10 s → 3.29 s (3.07x)**, decompression
**2.07 s → 0.90 s (2.29x)**, the same archives.

### Sentences ready for the report

* The original program compresses the whole corpus in 41.9 G cycles
  (10.1 s on the Ryzen) and decompresses it in 7.6 G cycles (2.1 s).
* Block I/O (#11) makes decompression 1.6x faster (63 % on the Ryzen,
  67 % on the Intel laptop); compression barely changes, because its
  time is spent in the match search, not in I/O.
* Inlining the bit reader and writer (#12) makes decompression another
  1.3x to 1.4x faster (2.2x to 2.3x against the original); compression
  is within ±5 %.
* Removing the `memmove` after every token (#14) makes compression
  1.3x to 1.5x faster.
* Comparing 8 bytes at a time (#15) makes compression another 1.7x
  faster, 2.3x to 2.5x against the original.  This is the largest
  single gain.
* AVX-512 (#16) makes compression 1.21x faster on the Intel laptop; the
  Ryzen has no AVX-512 and is unaffected.
* AVX2 makes compression 1.13x faster on the Ryzen, almost all of it on
  the one generated file with long matches; on the real (Silesia) files
  the gain is 2 %.
* LTO alone changes almost nothing (1 % or less).  PGO adds 5 % to
  compression and 6 % to decompression on the Ryzen, and 7 % to
  decompression on the laptop, but nothing to compression there.
* In total compression is 3.1x faster on the Ryzen and 2.9x faster on
  the laptop, decompression 2.4x faster on both, and every archive is
  byte-identical to the original program's.

### Things to mention honestly

* Steps 01 and 02 change compression by a few per cent either way
  depending on how the program is built and linked (the campaign runs
  in bench/log and steps.sh give different signs).  Treat them as "no
  change to compression".
* #16 and AVX2 each help only the CPU that has the instructions.  On
  the laptop the AVX2 routine, forced with `MZ77_MATCH=avx2`, is 3 %
  faster than the AVX-512 one that the program chooses by default
  (bench/log/hppavillion/06-avx2/raw/followup/ab-cycles.txt).
* AVX2 on the Ryzen makes five short-match files 7 % to 10 % slower
  (bench/log/debian/06-avx2/EXPLANATION.md).
* PGO is trained on a synthetic set that overlaps the generated part
  of the corpus; the Silesia files are not used for training and gain
  as much on the Ryzen (doc/optimisations/13-lto-pgo.md).

## 3. How to reproduce

### Prerequisites

* Linux on x86-64, GCC, autoconf, automake, libtool, gnulib, the GTK 4
  development files (see README), `perf` (linux-perf on Debian), and
  `taskset` (util-linux).
* `kernel.perf_event_paranoid` at 2 or lower:
  `sudo sysctl kernel.perf_event_paranoid=1`.
* Plug a laptop in and leave the machine idle while measuring.

### Build and fetch the corpus

    ./bootstrap
    ./configure --enable-release
    make -j$(nproc)
    make corpus            # downloads Silesia into bench/corpus/silesia

### All steps at once, in CPU cycles (section 2's tables)

    bench/steps.sh                       # ROUNDS=3 by default
    ROUNDS=5 CPU=3 bench/steps.sh        # what was run here

It rebuilds every step from its commit (the list is at the top of the
script), measures them in alternating rounds and writes
bench/results/COMMIT/steps.txt.  Five rounds took 9 minutes on the
laptop and 6 on the Ryzen, building included.  Set `MZ77_RESULTS=DIR` to choose the output directory.

### One step in full, as in bench/log

    git checkout COMMIT                  # the step's commit, below
    ./configure --enable-release && make clean && make -j$(nproc)
    MACHINE=mybox bench/campaign.sh 04-compare-8-bytes
    MACHINE=mybox MZ77_MATCH=word SKIP_COMPARE=1 SKIP_PROFILE=1 \
      bench/campaign.sh x-05-word        # an extra run on another path

For step 07 run `make pgo` after `make` and before `campaign.sh`.
Each run writes bench/log/MACHINE/STEP/ and rebuilds
bench/log/MACHINE/SUMMARY.md.

| step | commit |
|---|---|
| 00-baseline | 1f70c33 |
| 01-block-io (#11) | cc7a482 |
| 02-inline-bitio (#12) | 08b2ec5 |
| 03-no-memmove (#14) | 08b2ec5 plus bench/log/hppavillion/03-no-memmove/raw/uncommitted.patch (#14 and #15 were committed together) |
| 04-compare-8-bytes (#15) | c1c1f4a |
| 05-avx512bw (#16) | 02312cd |
| 06-avx2 | 55149a1 |
| 07-lto-pgo (#13) | 5d53dc4, then `make pgo` |

`MZ77_MATCH=word`, `avx2` (if the CPU has it) or unset chooses the
match routine from step 05 on.
