
#requiresdefine   'PETSC_HAVE_FORTRAN'
ALL: lib

CFLAGS   =
FFLAGS   =
SOURCEC  = bandwidthf.c gcreatef.c axpyf.c zerodiagf.c multequalf.c getcolvf.c
SOURCEF  =
SOURCEH  = 
DIRS     =
LIBBASE  = libpetscmat
LOCDIR   = src/mat/utils/ftn-auto/
include ${PETSC_DIR}/lib/petsc/conf/variables
include ${PETSC_DIR}/lib/petsc/conf/rules
include ${PETSC_DIR}/lib/petsc/conf/test
