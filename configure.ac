AC_INIT([libff], [1.0], [digirols@inf.ethz.ch])

m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT

AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])

AC_PROG_CC


AC_CONFIG_MACRO_DIR([m4])

FF_WITH_PORTALS4
AX_MPI([], [AC_MSG_ERROR(["Can't find MPI!"])])
CC=${MPICC}

AC_TYPE_UINT64_T

#AC_PROG_RANLIB
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
