#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* plexgenerate.c */
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
#define dmplextrianglesetoptions_ DMPLEXTRIANGLESETOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplextrianglesetoptions_ dmplextrianglesetoptions
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplextetgensetoptions_ DMPLEXTETGENSETOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplextetgensetoptions_ dmplextetgensetoptions
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  dmplextrianglesetoptions_(DM dm, char *opts, int *__ierr ){
*__ierr = DMPlexTriangleSetOptions(
	(DM)PetscToPointer((dm) ),opts);
}
PETSC_EXTERN void PETSC_STDCALL  dmplextetgensetoptions_(DM dm, char *opts, int *__ierr ){
*__ierr = DMPlexTetgenSetOptions(
	(DM)PetscToPointer((dm) ),opts);
}
#if defined(__cplusplus)
}
#endif
