#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* dmplexsnes.c */
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
#include "petscsnes.h"
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsnesgetgeometryfem_ DMPLEXSNESGETGEOMETRYFEM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsnesgetgeometryfem_ dmplexsnesgetgeometryfem
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsnesgetgeometryfvm_ DMPLEXSNESGETGEOMETRYFVM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsnesgetgeometryfvm_ dmplexsnesgetgeometryfvm
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsnesgetgradientdm_ DMPLEXSNESGETGRADIENTDM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsnesgetgradientdm_ dmplexsnesgetgradientdm
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsnescomputeresidualfem_ DMPLEXSNESCOMPUTERESIDUALFEM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsnescomputeresidualfem_ dmplexsnescomputeresidualfem
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define dmplexsnescomputejacobianfem_ DMPLEXSNESCOMPUTEJACOBIANFEM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define dmplexsnescomputejacobianfem_ dmplexsnescomputejacobianfem
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  dmplexsnesgetgeometryfem_(DM dm,Vec *cellgeom, int *__ierr ){
*__ierr = DMPlexSNESGetGeometryFEM(
	(DM)PetscToPointer((dm) ),cellgeom);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexsnesgetgeometryfvm_(DM dm,Vec *facegeom,Vec *cellgeom,PetscReal *minRadius, int *__ierr ){
*__ierr = DMPlexSNESGetGeometryFVM(
	(DM)PetscToPointer((dm) ),facegeom,cellgeom,minRadius);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexsnesgetgradientdm_(DM dm,PetscFV fv,DM *dmGrad, int *__ierr ){
*__ierr = DMPlexSNESGetGradientDM(
	(DM)PetscToPointer((dm) ),
	(PetscFV)PetscToPointer((fv) ),dmGrad);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexsnescomputeresidualfem_(DM dm,Vec X,Vec F,void*user, int *__ierr ){
*__ierr = DMPlexSNESComputeResidualFEM(
	(DM)PetscToPointer((dm) ),
	(Vec)PetscToPointer((X) ),
	(Vec)PetscToPointer((F) ),user);
}
PETSC_EXTERN void PETSC_STDCALL  dmplexsnescomputejacobianfem_(DM dm,Vec X,Mat Jac,Mat JacP,void*user, int *__ierr ){
*__ierr = DMPlexSNESComputeJacobianFEM(
	(DM)PetscToPointer((dm) ),
	(Vec)PetscToPointer((X) ),
	(Mat)PetscToPointer((Jac) ),
	(Mat)PetscToPointer((JacP) ),user);
}
#if defined(__cplusplus)
}
#endif
