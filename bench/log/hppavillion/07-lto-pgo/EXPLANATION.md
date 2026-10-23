# 07-lto-pgo on hppavillion

## What was measured

Issue #13, link-time and profile-guided optimisation
(doc/optimisations/13-lto-pgo.md), measured after `make pgo`, the same
way as on debian: touch the sources, `make clean`, `bench/campaign.sh
x-07-lto` (plain `--enable-release`, now with `-flto=auto`,
SKIP_PROFILE=1), then `make pgo` (raw/pgo.log) and `bench/campaign.sh
07-lto-pgo`, whose build stage found everything up to date
(raw/build.log compiles nothing).  The source is 55149a1 (the AVX2
commit) plus raw/uncommitted.patch, build files only.  This machine
takes the AVX-512 path; `make pgo` trains it, the AVX2 path and the
8-byte loop. Pinned to CPU 3, REPEAT=9.

## Conditions

As in 00-baseline (hppavillion, GCC 16.2, CPU 3, on AC); x-07-lto
started 2026-10-24 00:44 MSK, 07-lto-pgo at 00:54.  The laptop was not
idle this time: another Claude Code session and the desktop were in
use, so the other CPU threads had work, and CPU 3 shares its core with
one of them.  Load averages of 1.3 to 1.9 are in raw/conditions.txt.

## Results

| | 06-avx2 | x-07-lto | 07-lto-pgo |
|---|---|---|---|
| compression | 4.693 s | 5.404 s | 6.040 s |
| decompression | 1.152 s | 1.374 s | 1.284 s |

Archives byte-identical to 06-avx2 and to the baseline for every
corpus file, with and without PGO, and identical to the debian archives
of the same step.

The wall times are not usable: per file (../SUMMARY.md) they range
from 0.31x (mr) to 1.82x (dickens) in compression, with the same
pattern in decompression, which is the signature of frequency changes
and of load on the other threads.

Follow-up A/B tests after the run (raw/followup/ab-cycles.txt and
ab-cycles-more.txt), compressing all files, seven alternating rounds:

| build | cycles, median (range) | instructions |
|---|---|---|
| before #13 (55149a1) | 15.14 G (14.80 to 15.72) | 36.5 G |
| LTO (rebuilt as for x-07-lto) | 14.70 G (14.63 to 15.16) | 35.8 G |
| LTO + PGO (this step) | 14.97 G (14.76 to 15.94) | 31.3 G |

## Interpretation

Even the cycle counts vary by up to 6 % between rounds here, most
likely because of the other work on the machine.  Within that: LTO
takes about 3 % fewer cycles than the build before #13, and PGO,
although it removes 13 % of the instructions, does not reduce cycles
further on this machine; it comes out between the LTO build and the
old one.  Decompression (ab-cycles.txt, decompress lines minus
compress lines) is within the noise for all three builds.

So on Tiger Lake with GCC 16.2, #13 is worth about 3 %, from LTO; on
debian (Zen 2, GCC 14.2) it is worth 5 % to 7 %, from PGO.  A
measurement on an idle laptop would be needed to tell whether PGO
helps here at all.

bench-compare (raw/variants.txt) is disturbed in the same way and says
nothing reliable this time.
