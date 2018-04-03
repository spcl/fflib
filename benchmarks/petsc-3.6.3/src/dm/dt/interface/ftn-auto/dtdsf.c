#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* dtds.c */
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

#include "petscds.h"
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdssetfromoptions_ PETSCDSSETFROMOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdssetfromoptions_ petscdssetfromoptions
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsdestroy_ PETSCDSDESTROY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsdestroy_ petscdsdestroy
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdscreate_ PETSCDSCREATE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdscreate_ petscdscreate
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetnumfields_ PETSCDSGETNUMFIELDS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetnumfields_ petscdsgetnumfields
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetspatialdimension_ PETSCDSGETSPATIALDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetspatialdimension_ petscdsgetspatialdimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgettotaldimension_ PETSCDSGETTOTALDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgettotaldimension_ petscdsgettotaldimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgettotalbddimension_ PETSCDSGETTOTALBDDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgettotalbddimension_ petscdsgettotalbddimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgettotalcomponents_ PETSCDSGETTOTALCOMPONENTS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgettotalcomponents_ petscdsgettotalcomponents
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetdiscretization_ PETSCDSGETDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetdiscretization_ petscdsgetdiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetbddiscretization_ PETSCDSGETBDDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetbddiscretization_ petscdsgetbddiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdssetdiscretization_ PETSCDSSETDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdssetdiscretization_ petscdssetdiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdssetbddiscretization_ PETSCDSSETBDDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdssetbddiscretization_ petscdssetbddiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsadddiscretization_ PETSCDSADDDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsadddiscretization_ petscdsadddiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsaddbddiscretization_ PETSCDSADDBDDISCRETIZATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsaddbddiscretization_ petscdsaddbddiscretization
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetimplicit_ PETSCDSGETIMPLICIT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetimplicit_ petscdsgetimplicit
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdssetimplicit_ PETSCDSSETIMPLICIT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdssetimplicit_ petscdssetimplicit
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetadjacency_ PETSCDSGETADJACENCY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetadjacency_ petscdsgetadjacency
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdssetadjacency_ PETSCDSSETADJACENCY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdssetadjacency_ petscdssetadjacency
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetfieldoffset_ PETSCDSGETFIELDOFFSET
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetfieldoffset_ petscdsgetfieldoffset
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetbdfieldoffset_ PETSCDSGETBDFIELDOFFSET
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetbdfieldoffset_ petscdsgetbdfieldoffset
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetcomponentoffset_ PETSCDSGETCOMPONENTOFFSET
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetcomponentoffset_ petscdsgetcomponentoffset
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetcomponentoffsets_ PETSCDSGETCOMPONENTOFFSETS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetcomponentoffsets_ petscdsgetcomponentoffsets
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetcomponentderivativeoffsets_ PETSCDSGETCOMPONENTDERIVATIVEOFFSETS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetcomponentderivativeoffsets_ petscdsgetcomponentderivativeoffsets
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetcomponentbdoffsets_ PETSCDSGETCOMPONENTBDOFFSETS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetcomponentbdoffsets_ petscdsgetcomponentbdoffsets
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdsgetcomponentbdderivativeoffsets_ PETSCDSGETCOMPONENTBDDERIVATIVEOFFSETS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdsgetcomponentbdderivativeoffsets_ petscdsgetcomponentbdderivativeoffsets
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  petscdssetfromoptions_(PetscDS prob, int *__ierr ){
*__ierr = PetscDSSetFromOptions(
	(PetscDS)PetscToPointer((prob) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdsdestroy_(PetscDS *prob, int *__ierr ){
*__ierr = PetscDSDestroy(prob);
}
PETSC_EXTERN void PETSC_STDCALL  petscdscreate_(MPI_Fint * comm,PetscDS *prob, int *__ierr ){
*__ierr = PetscDSCreate(
	MPI_Comm_f2c(*(comm)),prob);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetnumfields_(PetscDS prob,PetscInt *Nf, int *__ierr ){
*__ierr = PetscDSGetNumFields(
	(PetscDS)PetscToPointer((prob) ),Nf);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetspatialdimension_(PetscDS prob,PetscInt *dim, int *__ierr ){
*__ierr = PetscDSGetSpatialDimension(
	(PetscDS)PetscToPointer((prob) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgettotaldimension_(PetscDS prob,PetscInt *dim, int *__ierr ){
*__ierr = PetscDSGetTotalDimension(
	(PetscDS)PetscToPointer((prob) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgettotalbddimension_(PetscDS prob,PetscInt *dim, int *__ierr ){
*__ierr = PetscDSGetTotalBdDimension(
	(PetscDS)PetscToPointer((prob) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgettotalcomponents_(PetscDS prob,PetscInt *Nc, int *__ierr ){
*__ierr = PetscDSGetTotalComponents(
	(PetscDS)PetscToPointer((prob) ),Nc);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetdiscretization_(PetscDS prob,PetscInt *f,PetscObject *disc, int *__ierr ){
*__ierr = PetscDSGetDiscretization(
	(PetscDS)PetscToPointer((prob) ),*f,disc);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetbddiscretization_(PetscDS prob,PetscInt *f,PetscObject *disc, int *__ierr ){
*__ierr = PetscDSGetBdDiscretization(
	(PetscDS)PetscToPointer((prob) ),*f,disc);
}
PETSC_EXTERN void PETSC_STDCALL  petscdssetdiscretization_(PetscDS prob,PetscInt *f,PetscObject disc, int *__ierr ){
*__ierr = PetscDSSetDiscretization(
	(PetscDS)PetscToPointer((prob) ),*f,
	(PetscObject)PetscToPointer((disc) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdssetbddiscretization_(PetscDS prob,PetscInt *f,PetscObject disc, int *__ierr ){
*__ierr = PetscDSSetBdDiscretization(
	(PetscDS)PetscToPointer((prob) ),*f,
	(PetscObject)PetscToPointer((disc) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdsadddiscretization_(PetscDS prob,PetscObject disc, int *__ierr ){
*__ierr = PetscDSAddDiscretization(
	(PetscDS)PetscToPointer((prob) ),
	(PetscObject)PetscToPointer((disc) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdsaddbddiscretization_(PetscDS prob,PetscObject disc, int *__ierr ){
*__ierr = PetscDSAddBdDiscretization(
	(PetscDS)PetscToPointer((prob) ),
	(PetscObject)PetscToPointer((disc) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetimplicit_(PetscDS prob,PetscInt *f,PetscBool *implicit, int *__ierr ){
*__ierr = PetscDSGetImplicit(
	(PetscDS)PetscToPointer((prob) ),*f,implicit);
}
PETSC_EXTERN void PETSC_STDCALL  petscdssetimplicit_(PetscDS prob,PetscInt *f,PetscBool *implicit, int *__ierr ){
*__ierr = PetscDSSetImplicit(
	(PetscDS)PetscToPointer((prob) ),*f,*implicit);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetadjacency_(PetscDS prob,PetscInt *f,PetscBool *useCone,PetscBool *useClosure, int *__ierr ){
*__ierr = PetscDSGetAdjacency(
	(PetscDS)PetscToPointer((prob) ),*f,useCone,useClosure);
}
PETSC_EXTERN void PETSC_STDCALL  petscdssetadjacency_(PetscDS prob,PetscInt *f,PetscBool *useCone,PetscBool *useClosure, int *__ierr ){
*__ierr = PetscDSSetAdjacency(
	(PetscDS)PetscToPointer((prob) ),*f,*useCone,*useClosure);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetfieldoffset_(PetscDS prob,PetscInt *f,PetscInt *off, int *__ierr ){
*__ierr = PetscDSGetFieldOffset(
	(PetscDS)PetscToPointer((prob) ),*f,off);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetbdfieldoffset_(PetscDS prob,PetscInt *f,PetscInt *off, int *__ierr ){
*__ierr = PetscDSGetBdFieldOffset(
	(PetscDS)PetscToPointer((prob) ),*f,off);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetcomponentoffset_(PetscDS prob,PetscInt *f,PetscInt *off, int *__ierr ){
*__ierr = PetscDSGetComponentOffset(
	(PetscDS)PetscToPointer((prob) ),*f,off);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetcomponentoffsets_(PetscDS prob,PetscInt *offsets[], int *__ierr ){
*__ierr = PetscDSGetComponentOffsets(
	(PetscDS)PetscToPointer((prob) ),offsets);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetcomponentderivativeoffsets_(PetscDS prob,PetscInt *offsets[], int *__ierr ){
*__ierr = PetscDSGetComponentDerivativeOffsets(
	(PetscDS)PetscToPointer((prob) ),offsets);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetcomponentbdoffsets_(PetscDS prob,PetscInt *offsets[], int *__ierr ){
*__ierr = PetscDSGetComponentBdOffsets(
	(PetscDS)PetscToPointer((prob) ),offsets);
}
PETSC_EXTERN void PETSC_STDCALL  petscdsgetcomponentbdderivativeoffsets_(PetscDS prob,PetscInt *offsets[], int *__ierr ){
*__ierr = PetscDSGetComponentBdDerivativeOffsets(
	(PetscDS)PetscToPointer((prob) ),offsets);
}
#if defined(__cplusplus)
}
#endif
