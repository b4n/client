
bin_PROGRAMS = tlclient

tlclient_SOURCES  = src/main.c \
                    src/game.c \
                    src/game.h
tlclient_LDADD    = $(AM_LIBS) @TL_CLIENT_LIBS@
tlclient_CPPFLAGS = @TL_DEBUG_CPPFLAGS@
tlclient_CFLAGS   = $(AM_CFLAGS) @TL_CLIENT_CFLAGS@

ACLOCAL_AMFLAGS = -I build/m4
