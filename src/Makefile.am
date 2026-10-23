bin_PROGRAMS = mz77
noinst_LIBRARIES = libmz77codec.a
libmz77codec_a_SOURCES = lz77.c lz77.h bitio.c bitio.h
libmz77codec_a_CFLAGS = $(PROFILE_CFLAGS) $(PGO_CFLAGS)
mz77_SOURCES = \
	       main.c antidebugger.c antidebugger.h dt.h \
	       messages.c messages.h queue.c queue.h
mz77_CFLAGS = $(GTK4_CFLAGS) $(PROFILE_CFLAGS) $(PGO_CFLAGS)
mz77_LDFLAGS = $(PROFILE_LDFLAGS)
mz77_LDADD = libmz77codec.a $(GTK4_LIBS) -lpthread
CLEANFILES = gmon.out
