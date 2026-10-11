check_PROGRAMS = roundtrip
roundtrip_SOURCES = roundtrip.c
roundtrip_CPPFLAGS = -I$(top_srcdir)/src
roundtrip_CFLAGS = $(PROFILE_CFLAGS)
roundtrip_LDFLAGS = $(PROFILE_LDFLAGS)
roundtrip_LDADD = $(top_builddir)/src/libmz77codec.a

TESTS = roundtrip
AM_TESTS_ENVIRONMENT = MZ77_TEST_DATA='$(srcdir)'; export MZ77_TEST_DATA;

EXTRA_DIST = input dist
CLEANFILES = gmon.out

clean-local:
	rm -rf roundtrip-work.*
