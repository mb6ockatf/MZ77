# x-07-lto on debian

An extra run for 07-lto-pgo: the #13 source (55149a1 plus
raw/uncommitted.patch, build files only) built with plain
`--enable-release`, which now means `-O3 -DNDEBUG -fhardened
-flto=auto`, without `make pgo`.  Run by ~/run-13.sh after `make clean`
(raw/build.log shows the whole program being compiled with -flto=auto),
`bench/campaign.sh x-07-lto` with SKIP_PROFILE=1, pinned to CPU 15,
REPEAT=9, started 2026-10-24 00:43 MSK.

| | 06-avx2 | x-07-lto | 07-lto-pgo |
|---|---|---|---|
| compression | 3.483 s | 3.438 s | 3.290 s |
| decompression | 0.944 s | 0.940 s | 0.901 s |

Archives byte-identical to 06-avx2 and to the baseline for every
corpus file.

LTO on its own gains about 1 % in compression and nothing measurable
in decompression; the gain of #13 on this machine is PGO's.  See
../07-lto-pgo/EXPLANATION.md.
