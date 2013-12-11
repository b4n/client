
lib_LTLIBRARIES = libtlclient.la

libtlclient_la_LDFLAGS  = -version-info @TL_CLIENT_LTVERSION@
libtlclient_la_CFLAGS   = @TL_CLIENT_CFLAGS@
#libtlclient_la_CXXFLAGS = @TL_CLIENT_CXXFLAGS@
libtlclient_la_CPPFLAGS = @TL_DEBUG_CPPFLAGS@
libtlclient_la_LIBADD   = $(AM_LIBS) @TL_CLIENT_LIBS@
libtlclient_la_SOURCES  = game.c

tlclient_includedir = $(includedir)/tunel/client
tlclient_include_HEADERS = game.h
