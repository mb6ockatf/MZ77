SUBDIRS = lib src test bench
dist_doc_DATA = README
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
EXTRA_DIST = m4/gnulib-cache.m4

clean-local:
	find . -name '*.orig' -exec rm -f {} +
	rm -rf pgo-data pgo-train

bench bench-compare profile corpus: all
	cd bench && $(MAKE) $(AM_MAKEFLAGS) $@

.PHONY: bench bench-compare profile corpus

# Profile-guided build: instrument, train on test/input and a small
# synthetic set (not the benchmark corpus), then rebuild with the profile.
# Every match routine the CPU supports is trained; -fprofile-partial-training
# keeps the untrained ones optimised normally.
PGO_DIR = $(abs_top_builddir)/pgo-data
PGO_TRAIN = $(abs_top_builddir)/pgo-train
PGO_GENERATE = -fprofile-generate -fprofile-dir='$(PGO_DIR)'
PGO_USE = -fprofile-use -fprofile-partial-training \
	  -fprofile-dir='$(PGO_DIR)' -Wno-missing-profile
pgo:
	rm -rf '$(PGO_DIR)' '$(PGO_TRAIN)'
	$(MAKE) $(AM_MAKEFLAGS) clean
	$(MAKE) $(AM_MAKEFLAGS) all PGO_CFLAGS="$(PGO_GENERATE)"
	cd bench && $(MAKE) $(AM_MAKEFLAGS) mz77-bench PGO_CFLAGS="$(PGO_GENERATE)"
	bench/mz77-bench --generate='$(PGO_TRAIN)' --size=4000000 > /dev/null
	for match in auto avx2 word; do \
	  MZ77_MATCH=$$match bench/mz77-bench --repeat=1 \
	    $(top_srcdir)/test/input/* '$(PGO_TRAIN)'/*.dat > /dev/null || exit 1; \
	done
	rm -rf '$(PGO_TRAIN)'
	cd src && $(MAKE) $(AM_MAKEFLAGS) clean
	cd test && $(MAKE) $(AM_MAKEFLAGS) clean
	cd bench && $(MAKE) $(AM_MAKEFLAGS) clean
	$(MAKE) $(AM_MAKEFLAGS) all PGO_CFLAGS="$(PGO_USE)"
	cd bench && $(MAKE) $(AM_MAKEFLAGS) mz77-bench PGO_CFLAGS="$(PGO_USE)"

.PHONY: pgo
