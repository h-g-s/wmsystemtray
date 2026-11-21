AUTOMAKE_OPTIONS = foreign subdir-objects
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = b0rken m4

bin_PROGRAMS = wmsystemtray

wmsystemtray_SOURCES = fdtray.c fdtray.h icons.c wmsystemtray.c wmsystemtray.h wmsystemtray.xpm
wmsystemtray_CFLAGS = @X_CFLAGS@
wmsystemtray_LDADD = b0rken/libb0rken.a @X_LIBS@

check_PROGRAMS = test-icons test-tray
test_icons_SOURCES = tests/test-icons.c icons.c wmsystemtray.h
test_icons_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir) @X_CFLAGS@
test_tray_SOURCES = tests/test-tray.c
test_tray_CFLAGS = @X_CFLAGS@
test_tray_LDADD = @X_LIBS@
TESTS = test-icons tests/test-icon-leak.sh tests/test-options.sh tests/test-tray.sh

man_MANS = wmsystemtray.1
EXTRA_DIST = bootstrap $(man_MANS) tests/test-icon-leak.sh tests/test-options.sh tests/test-tray.sh

reallyclean: distclean
	-rm -rf autom4te.cache
	-rm Makefile.in */Makefile.in aclocal.m4 config.h.in* config.guess config.sub configure install-sh ltmain.sh missing mkinstalldirs stamp-h.in
