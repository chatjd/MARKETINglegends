dnl require autoconf 2.60 (AS_ECHO/AS_ECHO_N)
AC_PREREQ([2.60])
define(_CLIENT_VERSION_MAJOR, 3)
define(_CLIENT_VERSION_MINOR, 0)
define(_CLIENT_VERSION_REVISION, 4)
define(_CLIENT_VERSION_BUILD, 56)

define(_ZC_BUILD_VAL, m4_if(m4_eval(_CLIENT_VERSION_BUILD < 25), 1, m4_incr(_CLIENT_VERSION_BUILD), m4_eval(_CLIENT_VERSION_BUILD < 50), 1, m4_eval(_CLIENT_VERSION_BUILD - 24), m4_eval(_CLIENT_VERSION_BUILD == 50), 1, , m4_eval(_CLIENT_VERSION_BUILD - 50)))
define(_CLIENT_VERSION_SUFFIX, m4_if(m4_eval(_CLIENT_VERSION_BUILD < 25), 1, _CLIENT_VERSION_REVISION-beta$1, m4_eval(_CLIENT_VERSION_BUILD < 50), 1, _CLIENT_VERSION_REVISION-rc$1, m4_eval(_CLIENT_VERSION_BUILD == 50), 1, _CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION-$1)))
define(_CLIENT_VERSION_IS_RELEASE, true)
define(_COPYRIGHT_YEAR, 2018)
AC_INIT([Vidulum],[_CLIENT_VERSION_MAJOR._CLIENT_VERSION_MINOR._CLIENT_VERSION_SUFFIX(_ZC_BUILD_VAL)],[https://github.com/vidulum/vidulum/issues],[vidulum])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([src/config/bitcoin-config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])

BITCOIN_DAEMON_NAME=vidulumd
BITCOIN_CLI_NAME=vidulum-cli
BITCOIN_TX_NAME=vidulum-tx

dnl Unless the user specified ARFLAGS, force it to be cr
AC_ARG_VAR(ARFLAGS, [Flags for the archiver, defaults to <cr> if not set])
if test "x${ARFLAGS+set}" != "xset"; then
  ARFLAGS="cr"
fi

AC_CANONICAL_HOST

AH_TOP([#ifndef BITCOIN_CONFIG_H])
AH_TOP([#define BITCOIN_CONFIG_H])
AH_BOTTOM([#endif //BITCOIN_CONFIG_H])

dnl faketime breaks configure and is only needed for make. Disable it here.
unset FAKETIME

dnl Automake init set-up and checks
AM_INIT_AUTOMAKE([no-define subdir-objects foreign tar-pax])

dnl faketime messes with timestamps and causes configure to be re-run.
dnl --disable-maintainer-mode can be used to bypass this.
AM_MAINTAINER_MODE([enable])

dnl make the compilation flags quiet unless V=1 is used
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Compiler checks (here before libtool).
if test "x${CXXFLAGS+set}" = "xset"; then
  CXXFLAGS_overridden=yes
else
  CXXFLAGS_overridden=no
fi

AC_PROG_CXX
m4_ifdef([AC_PROG_OBJCXX],[AC_PROG_OBJCXX])

dnl By default, libtool for mingw refuses to link static libs into a dll for
dnl fear of mixing pic/non-pic objects, and import/export complications. Since
dnl we have those under control, re-enable that functionality.
case $host in
  *mingw*)
     lt_cv_deplibs_check_method="pass_all"
  ;;
esac
dnl Require C++11 compiler (no GNU extensions)
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory], [nodefault])
dnl Check if -latomic is required for <std::atomic>
CHECK_ATOMIC

dnl Libtool init checks.
LT_INIT([pic-only])

dnl Check/return PATH for base programs.
AC_PATH_TOOL(AR, ar)
AC_PATH_TOOL(RANLIB, ranlib)
AC_PATH_TOOL(STRIP, strip)
AC_PATH_TOOL(GCOV, gcov)
AC_PATH_PROG(LCOV, lcov)
AC_PATH_PROG(GENHTML, genhtml)
AC_PATH_PROG([GIT], [git])
AC_PATH_PROG(CCACHE,ccache)
AC_PATH_PROG(XGETTEXT,xgettext)
AC_PATH_PROG(HEXDUMP,hexdump)
AC_PATH_TOOL(READELF,readelf)
AC_PATH_TOOL(CPPFILT,c++filt)

# Enable wallet
AC_ARG_ENABLE([wallet],
  [AS_HELP_STRING([--enable-wallet],
  [enable wallet (default is yes)])],
  [enable_wallet=$enableval],
  [enable_wallet=yes])

AC_ARG_ENABLE([mining],
  [AS_HELP_STRING([--enable-mining],
  [enable mining (default is yes)])],
  [enable_mining=$enableval],
  [enable_mining=yes])

AC_ARG_ENABLE([proton],
  [AS_HELP_STRING([--disable-proton],
  [disable Proton (AMQP messaging)])],
  [use_proton=$enableval],
  [use_proton=yes])

AC_ARG_ENABLE(tests,
    AS_HELP_STRING([--enable-tests],[compile tests (default is yes)]),
    [use_tests=$enableval],
    [use_tests=yes])

AC_ARG_ENABLE([asan],
  [AS_HELP_STRING([--enable-asan],
  [instrument the executables with asan (default is no)])],
  [use_asan=$enableval],
  [use_asan=no])

AC_ARG_ENABLE([tsan],
  [AS_HELP_STRING([--enable-tsan],
  [instrument the executables with tsan (default is no)])],
  [use_tsan=$enableval],
  [use_tsan=no])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--enable-hardening],
  [attempt to harden the resulting executables (default is yes)])],
  [use_hardening=$enableval],
  [use_hardening=yes])

AC_ARG_ENABLE([reduce-exports],
  [AS_HELP_STRING([--enable-reduce-exports],
  [attempt to reduce exported symbols in the resulting executables (default is no)])],
  [use_reduce_exports=$enableval],
  [use_reduce_exports=no])

AC_ARG_ENABLE([ccache],
  [AS_HELP_STRING([--enable-ccache],
  [use ccache for building (default is yes if ccache is found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

AC_ARG_ENABLE([lcov],
  [AS_HELP_STRING([--enable-lcov],
  [enable lcov testing (default is no)])],
  [use_lcov=yes],
  [use_lcov=no])

AC_ARG_ENABLE([glibc-back-compat],
  [AS_HELP_STRING([--enable-glibc-back-compat],
  [enable backwards compatibility with glibc])],
  [use_glibc_compat=$enableval],
  [use_glibc_compat=no])

AC_ARG_ENABLE([zmq],
  [AS_HELP_STRING([--disable-zmq],
  [disable ZMQ notifications])],
  [use_zmq=$enableval],
  [use_zmq=yes])

AC_ARG_WITH([protoc-bindir],[AS_HELP_STRING([--with-protoc-bindir=BIN_DIR],[specify protoc bin path])], [protoc_bin_path=$withval], [])

AC_ARG_ENABLE(man,
    [AS_HELP_STRING([--disable-man],
                    [do not install man pages (default is 