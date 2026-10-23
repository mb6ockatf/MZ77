# x-07-lto on hppavillion

An extra run for 07-lto-pgo: the #13 source (55149a1 plus
raw/uncommitted.patch, build files only) built with plain
`--enable-release`, which now means `-O3 -DNDEBUG -fhardened
-flto=auto`, without `make pgo`.  After touching the sources and
`make clean` (raw/build.log shows the whole program being compiled with
-flto=auto), `bench/campaign.sh x-07-lto` with SKIP_PROFILE=1, pinned
to CPU 3, REPEAT=9, started 2026-10-24 00:44 MSK.

| | 06-avx2 | x-07-lto | 07-lto-pgo |
|---|---|---|---|
| compression | 4.693 s | 5.404 s | 6.040 s |
| decompression | 1.152 s | 1.374 s | 1.284 s |

Archives byte-identical to 06-avx2 and to the baseline for every
corpus file.

The laptop was in use during this run and the wall times are not
usable (decompression, which LTO barely changes, is 19 % slower).  The
cycle counts in ../07-lto-pgo/raw/followup/ put this build about 3 %
below the build before #13 in compression.  See
../07-lto-pgo/EXPLANATION.md.
