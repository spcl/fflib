#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* matis.c */
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

#include "petscmat.h"
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matissetpreallocation_ MATISSETPREALLOCATION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define matissetpreallocation_ matissetpreallocation
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matisgetmpixaij_ MATISGETMPIXAIJ
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define matisgetmpixaij_ matisgetmpixaij
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matisgetlocalmat_ MATISGETLOCALMAT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define matisgetlocalmat_ matisgetlocalmat
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matissetlocalmat_ MATISSETLOCALMAT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define matissetlocalmat_ matissetlocalmat
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define matcreateis_ MATCREATEIS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define matcreateis_ matcreateis
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  matissetpreallocation_(Mat B,PetscInt *d_nz, PetscInt d_nnz[],PetscInt *o_nz, PetscInt o_nnz[], int *__ierr ){
*__ierr = MatISSetPreallocation(
	(Mat)PetscToPointer((B) ),*d_nz,d_nnz,*o_nz,o_nnz);
}
PETSC_EXTERN void PETSC_STDCALL  matisgetmpixaij_(Mat mat,MatReuse *reuse,Mat *newmat, int *__ierr ){
*__ierr = MatISGetMPIXAIJ(
	(Mat)PetscToPointer((mat) ),*reuse,newmat);
}
PETSC_EXTERN void PETSC_STDCALL  matisgetlocalmat_(Mat mat,Mat *local, int *__ierr ){
*__ierr = MatISGetLocalMat(
	(Mat)PetscToPointer((mat) ),local);
}
PETSC_EXTERN void PETSC_STDCALL  matissetlocalmat_(Mat mat,Mat local, int *__ierr ){
*__ierr = MatISSetLocalMat(
	(Mat)PetscToPointer((mat) ),
	(Mat)PetscToPointer((local) ));
}
PETSC_EXTERN void PETSC_STDCALL  matcreateis_(MPI_Fint * comm,PetscInt *bs,PetscInt *m,PetscInt *n,PetscInt *M,PetscInt *N,ISLocalToGlobalMapping map,Mat *A, int *__ierr ){
*__ierr = MatCreateIS(
	MPI_Comm_f2c(*(comm)),*bs,*m,*n,*M,*N,
	(ISLocalToGlobalMapping)PetscToPointer((map) ),A);
}
#if defined(__cplusplus)
}
#endif
