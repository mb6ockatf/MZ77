SUBDIRS = lib src test
dist_doc_DATA = README
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
EXTRA_DIST = m4/gnulib-cache.m4

clean-local:
	find . -name '*.orig' -exec rm -f {} +
