SUBDIRS = lib src test bench
dist_doc_DATA = README
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
EXTRA_DIST = m4/gnulib-cache.m4

clean-local:
	find . -name '*.orig' -exec rm -f {} +

bench bench-compare profile corpus: all
	cd bench && $(MAKE) $(AM_MAKEFLAGS) $@

.PHONY: bench bench-compare profile corpus
