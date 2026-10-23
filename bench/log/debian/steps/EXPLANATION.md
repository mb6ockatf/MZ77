# steps on debian

bench/steps.sh, run once at the end of the optimisation work
(2026-10-24 02:16 MSK), after the 07-lto-pgo commit 5d53dc4:
every step's codec rebuilt from its commit with that step's release
flags (07 twice: LTO only, and LTO with the same PGO training as
`make pgo`), then five alternating rounds, pinned to one core
(machine.txt and the head of steps-raw.txt), of

* compressing the generated files and the Silesia files, and
* decompressing archives of every corpus file (--archives, so only
  decompression is counted),

counting user-space CPU cycles with perf stat.  steps.txt has the
median of the five rounds for each step and the speed-ups against the
previous step and the baseline; steps-raw.txt has every measurement.

These are the numbers to quote between steps
(doc/optimisations/RESULTS.md): unlike the wall-clock runs in the step
directories, which were made hours apart, all steps here were measured
under the same conditions within a few minutes, and cycle counts do
not depend on the clock frequency.  Single rounds differ by up to 7 %
(steps-raw.txt); the medians are steadier.

The builds differ from the campaign runs in one way: the benchmark
driver and the codec are compiled in one gcc call (as bench-compare
does), not through libtool and a static library.  That changes code
layout, which is why steps 01 and 02 show compression a few per cent
different from the campaign runs; their effect on compression is
within that margin.
