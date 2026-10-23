# Benchmarks and profiling for the codec.  Nothing here is built by 'make'
# or run by 'make check'; see bench/README.
EXTRA_PROGRAMS = mz77-bench
mz77_bench_SOURCES = bench.c
mz77_bench_CPPFLAGS = -I$(top_srcdir)/src
mz77_bench_CFLAGS = $(PROFILE_CFLAGS) $(PGO_CFLAGS)
mz77_bench_LDFLAGS = $(PROFILE_LDFLAGS)
mz77_bench_LDADD = $(top_builddir)/src/libmz77codec.a
dist_noinst_SCRIPTS = common.sh fetch-silesia.sh bench.sh compare.sh profile.sh \
		      campaign.sh summarize.sh
dist_noinst_DATA = README
BENCH_ENV = CC='$(CC)' MZ77_SRCDIR='$(abs_top_srcdir)' \
	    MZ77_BUILDDIR='$(abs_top_builddir)'

corpus:
	$(BENCH_ENV) $(SHELL) $(srcdir)/fetch-silesia.sh

bench: mz77-bench
	$(BENCH_ENV) MZ77_BENCH='$(abs_builddir)/mz77-bench' \
	  $(SHELL) $(srcdir)/bench.sh

bench-compare:
	$(BENCH_ENV) $(SHELL) $(srcdir)/compare.sh

profile:
	$(BENCH_ENV) $(SHELL) $(srcdir)/profile.sh

.PHONY: corpus bench bench-compare profile

CLEANFILES = $(EXTRA_PROGRAMS) gmon.out

clean-local:
	rm -rf bin pgo-data pgo-train

