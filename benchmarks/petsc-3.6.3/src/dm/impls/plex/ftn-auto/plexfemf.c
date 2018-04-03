#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* plexfem.c */
/* Fortran interface file */

/*
* This file was generated automatically by bfort from the C source
* file.  
 */

#ifdef PETSC_USE_POINTER_CONVERSION
#if defined(__cplusplus)
extern "C" { 
#endif 
extern void *PetscToPointer(void*);
extern int PetscFromPointer(void *);
extern void PetscRmPointer(void*);
#if defined(__cplusplus)
} 
#endif 

#else

#define PetscToPointer(a) (*(PetscFortranAddr *)(a))
#define PetscFromPointer(a) (PetscFortranAddr)(a)
#define PetscRmPointer(a)
#endif

#include "petscdmplex.h"
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsetmaxprojectionheight_ DMPLEXSETMAXPROJECTIONHEIGHT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsetmaxprojectionheight_ dmplexsetmaxprojectionheight
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexgetmaxprojectionheight_ DMPLEXGETMAXPROJECTIONHEIGHT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexgetmaxprojectionheight_ dmplexgetmaxprojectionheight
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexcomputeintegralfem_ DMPLEXCOMPUTEINTEGRALFEM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexcomputeintegralfem_ dmplexcomputeintegralfem
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexcomputeinterpolatorfem_ DMPLEXCOMPUTEINTERPOLATORFEM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexcomputeinterpolatorfem_ dmplexcomputeinterpolatorfem
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  dmplexsetmaxprojectionheight_(DM dm,PetscInt *height, int *__ierr ){
*__ierr = DMPlexSetMaxProjectionHeight(
	(DM)PetscToPointer((dm) ),*height);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexgetmaxprojectionheight_(DM dm,PetscInt *height, int *__ierr ){
*__ierr = DMPlexGetMaxProjectionHeight(
	(DM)PetscToPointer((dm) ),height);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexcomputeintegralfem_(DM dm,Vec X,PetscReal *integral,void*user, int *__ierr ){
*__ierr = DMPlexComputeIntegralFEM(
	(DM)PetscToPointer((dm) ),
	(Vec)PetscToPointer((X) ),integral,user);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexcomputeinterpolatorfem_(DM dmc,DM dmf,Mat In,void*user, int *__ierr ){
*__ierr = DMPlexComputeInterpolatorFEM(
	(DM)PetscToPointer((dmc) ),
	(DM)PetscToPointer((dmf) ),
	(Mat)PetscToPointer((In) ),user);
}
#if defined(__cplusplus)
}
#endif
