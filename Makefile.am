SUBDIRS = src

bin_PROGRAMS = tlclient

tlclient_SOURCES = src/main.c
tlclient_LDADD   = src/libtlclient.la @TL_CLIENT_LIBS@
tlclient_CFLAGS  = @TL_CLIENT_CFLAGS@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = tlclient.pc

ACLOCAL_AMFLAGS = -I build/m4
