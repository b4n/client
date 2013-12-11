
lib_LTLIBRARIES = libtlclient.la

AM_LDFLAGS = -version-info @TL_CLIENT_LTVERSION@
AM_CFLAGS  = @TL_CLIENT_CFLAGS@
#AM_CXXFLAGS= @TL_CLIENT_CXXFLAGS@
libtlclient_la_LIBADD  = $(AM_LIBS) @TL_CLIENT_LIBS@
libtlclient_la_SOURCES = game.c

tlclient_includedir = $(includedir)/tunel/client
tlclient_include_HEADERS = game.h
