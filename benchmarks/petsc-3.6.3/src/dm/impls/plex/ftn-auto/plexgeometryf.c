#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* plexgeometry.c */
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
#define dmplexcomputegeometryfvm_ DMPLEXCOMPUTEGEOMETRYFVM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexcomputegeometryfvm_ dmplexcomputegeometryfvm
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexcomputegradientfvm_ DMPLEXCOMPUTEGRADIENTFVM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexcomputegradientfvm_ dmplexcomputegradientfvm
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  dmplexcomputegeometryfvm_(DM dm,Vec *cellgeom,Vec *facegeom, int *__ierr ){
*__ierr = DMPlexComputeGeometryFVM(
	(DM)PetscToPointer((dm) ),cellgeom,facegeom);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexcomputegradientfvm_(DM dm,PetscFV fvm,Vec faceGeometry,Vec cellGeometry,DM *dmGrad, int *__ierr ){
*__ierr = DMPlexComputeGradientFVM(
	(DM)PetscToPointer((dm) ),
	(PetscFV)PetscToPointer((fvm) ),
	(Vec)PetscToPointer((faceGeometry) ),
	(Vec)PetscToPointer((cellGeometry) ),dmGrad);
}
#if defined(__cplusplus)
}
#endif
