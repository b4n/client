#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([TuneL client],
        [0.0],
        [martin.antony@yahoo.fr],
        [tlclient])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR([build/aux])
AC_CONFIG_MACRO_DIR([build/m4])
AM_INIT_AUTOMAKE([1.11.1 -Wall foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AM_PROG_AR

AM_SILENT_RULES([yes])

# Checks for programs.
LT_PREREQ([2.2.0])
LT_INIT
AC_PROG_CC
AM_PROG_CC_C_O

# basic setup

#
# revision of the library
# 
# CURRENT:REVISION:AGE
#
# Please remeber to bump library version before releases
#   Remeberer:
# =====================================================================
# |   If you have...                              |   Bump version to |
# =====================================================================
# | Not changed the interface (bug fixes)         | CURRENT:REV+1:AGE |
# ---------------------------------------------------------------------
# | Augmented the interface (new functions)       | CURRENT+1:0:AGE+1 |
# ---------------------------------------------------------------------
# | Broken old interface (e.g. removed functions) | CURRENT+1:0:0     |
# ---------------------------------------------------------------------
TL_CLIENT_LTVERSION="0:0:0"
AC_SUBST([TL_CLIENT_LTVERSION])

# Checks for libraries.

# AX_REQUIRE_HEADER(HEADER, IF-PRESENT)
# ----------------------------
# Check presence of HEADER and exit if not present, or execute IF-PRESENT if
# prsent
AC_DEFUN([AX_REQUIRE_HEADER],
         [AC_CHECK_HEADERS([$1],
                           [$2],
                           [AC_MSG_ERROR([[$1 not found]])])])

# AX_REQUIRE_LIB(LIBRARY, FUNC, IF-PRESENT)
# ----------------------------
# Check presence of HEADER and exit if not present, or execute IF-PRESENT if
# prsent
AC_DEFUN([AX_REQUIRE_LIB],
         [AC_CHECK_LIB([$1], [$2],
                       [$3],
                       [AC_MSG_ERROR([[$1 not found]])])])


# PThread
AX_REQUIRE_HEADER([pthread.h])
AX_REQUIRE_LIB([pthread], [pthread_create])

# Wrapper around PKG_CHECK_MODULES() that fill the TL_CLIENT_CFLAGS and
# TL_CLIENT_LIBS variables according to the PKG's CFLAGS and LIBS
AC_DEFUN([AX_PKG_CHECK_MODULES_C],
         [PKG_CHECK_MODULES([$1],
                            [$2])
          TL_CLIENT_CFLAGS="${TL_CLIENT_CFLAGS} ${$1_CFLAGS}"
          TL_CLIENT_LIBS="${TL_CLIENT_LIBS} ${$1_LIBS}"
         ])

TL_CLIENT_CFLAGS=
TL_CLIENT_LIBS=

AX_PKG_CHECK_MODULES_C([LIBCURL],       [libcurl])
AX_PKG_CHECK_MODULES_C([SCEUTILS],      [sceutils])
AX_PKG_CHECK_MODULES_C([SCECORE],       [scecore])
AX_PKG_CHECK_MODULES_C([SCEINTERFACE],  [sceinterface])
AX_PKG_CHECK_MODULES_C([SDL],           [sdl])
AX_PKG_CHECK_MODULES_C([TLCOMMON],      [tlcommon])

AC_SUBST([TL_CLIENT_CFLAGS])
AC_SUBST([TL_CLIENT_LIBS])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h \
                  string.h \
                  stdarg.h \
                  unistd.h \
                  limits.h \
                  arpa/inet.h \
                  netinet/in.h \
                  sys/socket.h \
                  sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([strchr])

# Tuning
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [enable debugging [[default=yes]]])],
              [enable_debug="$enableval"],
              [enable_debug="yes"])
AC_MSG_CHECKING([whether to enable debugging])
AS_IF([test "x$enable_debug" = "xyes"],
      [AC_DEFINE([TL_DEBUG], [1], [is debugging enabled])
       # FIXME: please only use TL_DEBUG, not DEBUG
       TL_DEBUG_CPPFLAGS="-DTL_DEBUG -DDEBUG"
       AC_MSG_RESULT([yes])],
      [TL_DEBUG_CPPFLAGS=
       AC_MSG_RESULT([no])])
AC_SUBST([TL_DEBUG_CPPFLAGS])

# output
AC_CONFIG_FILES([Makefile
                 tlclient.pc
                 src/Makefile
                ])
AC_OUTPUT


echo "------------------------------------------"
echo "Configuration choices:"
echo "* Debugging enabled               : $enable_debug"
echo "* Base installation directory     : $prefix"
echo ""
echo "Now type 'make' to build $PACKAGE_NAME"
