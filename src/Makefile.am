
lib_LTLIBRARIES = libtlclient.la

AM_LDFLAGS = -version-info @TL_CLIENT_LTVERSION@
AM_CFLAGS  = @TL_CLIENT_CFLAGS@
#AM_CXXFLAGS= @TL_CLIENT_CXXFLAGS@
libtlclient_la_LIBADD  = $(AM_LIBS) @TL_CLIENT_LIBS@
libtlclient_la_SOURCES = vcache.c \
                         game.c

tl_include_client_HEADERS = vcache.h \
                            game.h
