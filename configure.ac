AC_PREREQ([2.69])
AC_INIT([prongc], [1.0], [https://github.com/omeridrissi/prongc/issues], [], [https://github.com/omeridrissi/prongc])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Require C11
AC_MSG_CHECKING([whether compiler supports C11])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[
#if __STDC_VERSION__ < 201112L
#error "not C11"
#endif
]])], [AC_MSG_RESULT([yes])], [AC_MSG_ERROR([a C11 compiler is required])])

# External dependency: llvm-config (and libclang)
AC_ARG_WITH([llvm-config],
  [AS_HELP_STRING([--with-llvm-config=PATH], [path to llvm-config program])],
  [LLVM_CONFIG="$with_llvm_config"],
  [AC_PATH_PROGS([LLVM_CONFIG], [llvm-config-21 llvm-config], [no])]
)

AS_IF([test "x$LLVM_CONFIG" = "xno" || test ! -x "$LLVM_CONFIG"], [
  AC_MSG_ERROR([llvm-config not found. Install LLVM/Clang development packages or use --with-llvm-config=PATH])
])

# Check version
AC_MSG_CHECKING([llvm-config version >= 21.0.0])
llvm_version=`$LLVM_CONFIG --version`
AS_VERSION_COMPARE([$llvm_version], [21.0.0],
  [AC_MSG_RESULT([no ($llvm_version)]); AC_MSG_ERROR([llvm-config >= 21.0.0 required, found $llvm_version])],
  [AC_MSG_RESULT([yes ($llvm_version)])],
  [AC_MSG_RESULT([yes ($llvm_version)])]
)

# Get flags from llvm-config
LIBCLANG_CPPFLAGS=`$LLVM_CONFIG --cflags`
LIBCLANG_LIBDIR=`$LLVM_CONFIG --libdir`
LIBCLANG_LDFLAGS=`$LLVM_CONFIG --ldflags`
LIBCLANG_LIBS="-lclang `$LLVM_CONFIG --libs --system-libs`"

# Checks for libclang headers.
save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$LIBCLANG_CPPFLAGS $CPPFLAGS"
AC_CHECK_HEADER([clang-c/Index.h], [], [
  AC_MSG_ERROR([libclang headers not found. Install clang development package.])
])
CPPFLAGS="$save_CPPFLAGS"

# Checks for libclang library.
save_LDFLAGS="$LDFLAGS"
save_LIBS="$LIBS"
LDFLAGS="$LIBCLANG_LDFLAGS $LDFLAGS"
LIBS="$LIBCLANG_LIBS $LIBS"
AC_CHECK_LIB([clang], [clang_createIndex], [:], [
  AC_MSG_ERROR([libclang library not found or missing clang_createIndex])
])
LDFLAGS="$save_LDFLAGS"
LIBS="$save_LIBS"

# Compiler warnings (simple version without custom macros)
PRONGC_CFLAGS="-D_ISOC11_SOURCE -Wall -Wextra -O3"

# Check for optional warning flags and add if supported
for flag in \
  -Wcast-align \
  -Wconversion \
  -Wfloat-equal \
  -Wformat=2 \
  -Wimplicit-fallthrough \
  -Wmisleading-indentation \
  -Wshadow \
  -Wsign-compare \
  -Wsign-conversion \
  -Wundef \
  -Wuninitialized \
  -Wunused \
  -Wwrite-strings; do
  AC_MSG_CHECKING([whether $CC accepts $flag])
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $flag -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
    [AC_MSG_RESULT([yes]); PRONGC_CFLAGS="$PRONGC_CFLAGS $flag"],
    [AC_MSG_RESULT([no])])
  CFLAGS="$save_CFLAGS"
done

# Generate files.
AC_SUBST([PRONGC_CFLAGS])
AC_SUBST([LIBCLANG_CPPFLAGS])
AC_SUBST([LIBCLANG_LDFLAGS])
AC_SUBST([LIBCLANG_LIBDIR])
AC_SUBST([LIBCLANG_LIBS])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
