#include "petscsys.h"
#include "petscfix.h"
#include "petsc/private/fortranimpl.h"
/* dtfe.c */
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

#include "petscfe.h"
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacesetfromoptions_ PETSCSPACESETFROMOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacesetfromoptions_ petscspacesetfromoptions
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacedestroy_ PETSCSPACEDESTROY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacedestroy_ petscspacedestroy
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacecreate_ PETSCSPACECREATE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacecreate_ petscspacecreate
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacegetorder_ PETSCSPACEGETORDER
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacegetorder_ petscspacegetorder
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacesetorder_ PETSCSPACESETORDER
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacesetorder_ petscspacesetorder
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacepolynomialsettensor_ PETSCSPACEPOLYNOMIALSETTENSOR
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacepolynomialsettensor_ petscspacepolynomialsettensor
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscspacepolynomialgettensor_ PETSCSPACEPOLYNOMIALGETTENSOR
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscspacepolynomialgettensor_ petscspacepolynomialgettensor
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspaceview_ PETSCDUALSPACEVIEW
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspaceview_ petscdualspaceview
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesetfromoptions_ PETSCDUALSPACESETFROMOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesetfromoptions_ petscdualspacesetfromoptions
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesetup_ PETSCDUALSPACESETUP
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesetup_ petscdualspacesetup
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacedestroy_ PETSCDUALSPACEDESTROY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacedestroy_ petscdualspacedestroy
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacecreate_ PETSCDUALSPACECREATE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacecreate_ petscdualspacecreate
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspaceduplicate_ PETSCDUALSPACEDUPLICATE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspaceduplicate_ petscdualspaceduplicate
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacegetdm_ PETSCDUALSPACEGETDM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacegetdm_ petscdualspacegetdm
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesetdm_ PETSCDUALSPACESETDM
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesetdm_ petscdualspacesetdm
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacegetorder_ PETSCDUALSPACEGETORDER
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacegetorder_ petscdualspacegetorder
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesetorder_ PETSCDUALSPACESETORDER
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesetorder_ petscdualspacesetorder
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacegetfunctional_ PETSCDUALSPACEGETFUNCTIONAL
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacegetfunctional_ petscdualspacegetfunctional
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacegetdimension_ PETSCDUALSPACEGETDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacegetdimension_ petscdualspacegetdimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacecreatereferencecell_ PETSCDUALSPACECREATEREFERENCECELL
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacecreatereferencecell_ petscdualspacecreatereferencecell
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacegetheightsubspace_ PETSCDUALSPACEGETHEIGHTSUBSPACE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacegetheightsubspace_ petscdualspacegetheightsubspace
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacelagrangegetcontinuity_ PETSCDUALSPACELAGRANGEGETCONTINUITY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacelagrangegetcontinuity_ petscdualspacelagrangegetcontinuity
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacelagrangesetcontinuity_ PETSCDUALSPACELAGRANGESETCONTINUITY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacelagrangesetcontinuity_ petscdualspacelagrangesetcontinuity
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesimplesetdimension_ PETSCDUALSPACESIMPLESETDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesimplesetdimension_ petscdualspacesimplesetdimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscdualspacesimplesetfunctional_ PETSCDUALSPACESIMPLESETFUNCTIONAL
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscdualspacesimplesetfunctional_ petscdualspacesimplesetfunctional
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesetfromoptions_ PETSCFESETFROMOPTIONS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesetfromoptions_ petscfesetfromoptions
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfedestroy_ PETSCFEDESTROY
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfedestroy_ petscfedestroy
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfecreate_ PETSCFECREATE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfecreate_ petscfecreate
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetspatialdimension_ PETSCFEGETSPATIALDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetspatialdimension_ petscfegetspatialdimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesetnumcomponents_ PETSCFESETNUMCOMPONENTS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesetnumcomponents_ petscfesetnumcomponents
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetnumcomponents_ PETSCFEGETNUMCOMPONENTS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetnumcomponents_ petscfegetnumcomponents
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesettilesizes_ PETSCFESETTILESIZES
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesettilesizes_ petscfesettilesizes
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegettilesizes_ PETSCFEGETTILESIZES
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegettilesizes_ petscfegettilesizes
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetbasisspace_ PETSCFEGETBASISSPACE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetbasisspace_ petscfegetbasisspace
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesetbasisspace_ PETSCFESETBASISSPACE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesetbasisspace_ petscfesetbasisspace
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetdualspace_ PETSCFEGETDUALSPACE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetdualspace_ petscfegetdualspace
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesetdualspace_ PETSCFESETDUALSPACE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesetdualspace_ petscfesetdualspace
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetquadrature_ PETSCFEGETQUADRATURE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetquadrature_ petscfegetquadrature
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfesetquadrature_ PETSCFESETQUADRATURE
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfesetquadrature_ petscfesetquadrature
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfegetdimension_ PETSCFEGETDIMENSION
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfegetdimension_ petscfegetdimension
#endif
#ifdef PETSC_HAVE_FORTRAN_CAPS
#define petscfecreatedefault_ PETSCFECREATEDEFAULT
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE) && !defined(FORTRANDOUBLEUNDERSCORE)
#define petscfecreatedefault_ petscfecreatedefault
#endif


/* Definitions of Fortran Wrapper routines */
#if defined(__cplusplus)
extern "C" {
#endif
PETSC_EXTERN void PETSC_STDCALL  petscspacesetfromoptions_(PetscSpace sp, int *__ierr ){
*__ierr = PetscSpaceSetFromOptions(
	(PetscSpace)PetscToPointer((sp) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscspacedestroy_(PetscSpace *sp, int *__ierr ){
*__ierr = PetscSpaceDestroy(sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscspacecreate_(MPI_Fint * comm,PetscSpace *sp, int *__ierr ){
*__ierr = PetscSpaceCreate(
	MPI_Comm_f2c(*(comm)),sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscspacegetorder_(PetscSpace sp,PetscInt *order, int *__ierr ){
*__ierr = PetscSpaceGetOrder(
	(PetscSpace)PetscToPointer((sp) ),order);
}
PETSC_EXTERN void PETSC_STDCALL  petscspacesetorder_(PetscSpace sp,PetscInt *order, int *__ierr ){
*__ierr = PetscSpaceSetOrder(
	(PetscSpace)PetscToPointer((sp) ),*order);
}
PETSC_EXTERN void PETSC_STDCALL  petscspacepolynomialsettensor_(PetscSpace sp,PetscBool *tensor, int *__ierr ){
*__ierr = PetscSpacePolynomialSetTensor(
	(PetscSpace)PetscToPointer((sp) ),*tensor);
}
PETSC_EXTERN void PETSC_STDCALL  petscspacepolynomialgettensor_(PetscSpace sp,PetscBool *tensor, int *__ierr ){
*__ierr = PetscSpacePolynomialGetTensor(
	(PetscSpace)PetscToPointer((sp) ),tensor);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspaceview_(PetscDualSpace sp,PetscViewer v, int *__ierr ){
*__ierr = PetscDualSpaceView(
	(PetscDualSpace)PetscToPointer((sp) ),
	(PetscViewer)PetscToPointer((v) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesetfromoptions_(PetscDualSpace sp, int *__ierr ){
*__ierr = PetscDualSpaceSetFromOptions(
	(PetscDualSpace)PetscToPointer((sp) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesetup_(PetscDualSpace sp, int *__ierr ){
*__ierr = PetscDualSpaceSetUp(
	(PetscDualSpace)PetscToPointer((sp) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacedestroy_(PetscDualSpace *sp, int *__ierr ){
*__ierr = PetscDualSpaceDestroy(sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacecreate_(MPI_Fint * comm,PetscDualSpace *sp, int *__ierr ){
*__ierr = PetscDualSpaceCreate(
	MPI_Comm_f2c(*(comm)),sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspaceduplicate_(PetscDualSpace sp,PetscDualSpace *spNew, int *__ierr ){
*__ierr = PetscDualSpaceDuplicate(
	(PetscDualSpace)PetscToPointer((sp) ),spNew);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacegetdm_(PetscDualSpace sp,DM *dm, int *__ierr ){
*__ierr = PetscDualSpaceGetDM(
	(PetscDualSpace)PetscToPointer((sp) ),dm);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesetdm_(PetscDualSpace sp,DM dm, int *__ierr ){
*__ierr = PetscDualSpaceSetDM(
	(PetscDualSpace)PetscToPointer((sp) ),
	(DM)PetscToPointer((dm) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacegetorder_(PetscDualSpace sp,PetscInt *order, int *__ierr ){
*__ierr = PetscDualSpaceGetOrder(
	(PetscDualSpace)PetscToPointer((sp) ),order);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesetorder_(PetscDualSpace sp,PetscInt *order, int *__ierr ){
*__ierr = PetscDualSpaceSetOrder(
	(PetscDualSpace)PetscToPointer((sp) ),*order);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacegetfunctional_(PetscDualSpace sp,PetscInt *i,PetscQuadrature *functional, int *__ierr ){
*__ierr = PetscDualSpaceGetFunctional(
	(PetscDualSpace)PetscToPointer((sp) ),*i,functional);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacegetdimension_(PetscDualSpace sp,PetscInt *dim, int *__ierr ){
*__ierr = PetscDualSpaceGetDimension(
	(PetscDualSpace)PetscToPointer((sp) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacecreatereferencecell_(PetscDualSpace sp,PetscInt *dim,PetscBool *simplex,DM *refdm, int *__ierr ){
*__ierr = PetscDualSpaceCreateReferenceCell(
	(PetscDualSpace)PetscToPointer((sp) ),*dim,*simplex,refdm);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacegetheightsubspace_(PetscDualSpace sp,PetscInt *height,PetscDualSpace *bdsp, int *__ierr ){
*__ierr = PetscDualSpaceGetHeightSubspace(
	(PetscDualSpace)PetscToPointer((sp) ),*height,bdsp);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacelagrangegetcontinuity_(PetscDualSpace sp,PetscBool *continuous, int *__ierr ){
*__ierr = PetscDualSpaceLagrangeGetContinuity(
	(PetscDualSpace)PetscToPointer((sp) ),continuous);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacelagrangesetcontinuity_(PetscDualSpace sp,PetscBool *continuous, int *__ierr ){
*__ierr = PetscDualSpaceLagrangeSetContinuity(
	(PetscDualSpace)PetscToPointer((sp) ),*continuous);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesimplesetdimension_(PetscDualSpace sp,PetscInt *dim, int *__ierr ){
*__ierr = PetscDualSpaceSimpleSetDimension(
	(PetscDualSpace)PetscToPointer((sp) ),*dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscdualspacesimplesetfunctional_(PetscDualSpace sp,PetscInt *func,PetscQuadrature q, int *__ierr ){
*__ierr = PetscDualSpaceSimpleSetFunctional(
	(PetscDualSpace)PetscToPointer((sp) ),*func,
	(PetscQuadrature)PetscToPointer((q) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscfesetfromoptions_(PetscFE fem, int *__ierr ){
*__ierr = PetscFESetFromOptions(
	(PetscFE)PetscToPointer((fem) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscfedestroy_(PetscFE *fem, int *__ierr ){
*__ierr = PetscFEDestroy(fem);
}
PETSC_EXTERN void PETSC_STDCALL  petscfecreate_(MPI_Fint * comm,PetscFE *fem, int *__ierr ){
*__ierr = PetscFECreate(
	MPI_Comm_f2c(*(comm)),fem);
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetspatialdimension_(PetscFE fem,PetscInt *dim, int *__ierr ){
*__ierr = PetscFEGetSpatialDimension(
	(PetscFE)PetscToPointer((fem) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscfesetnumcomponents_(PetscFE fem,PetscInt *comp, int *__ierr ){
*__ierr = PetscFESetNumComponents(
	(PetscFE)PetscToPointer((fem) ),*comp);
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetnumcomponents_(PetscFE fem,PetscInt *comp, int *__ierr ){
*__ierr = PetscFEGetNumComponents(
	(PetscFE)PetscToPointer((fem) ),comp);
}
PETSC_EXTERN void PETSC_STDCALL  petscfesettilesizes_(PetscFE fem,PetscInt *blockSize,PetscInt *numBlocks,PetscInt *batchSize,PetscInt *numBatches, int *__ierr ){
*__ierr = PetscFESetTileSizes(
	(PetscFE)PetscToPointer((fem) ),*blockSize,*numBlocks,*batchSize,*numBatches);
}
PETSC_EXTERN void PETSC_STDCALL  petscfegettilesizes_(PetscFE fem,PetscInt *blockSize,PetscInt *numBlocks,PetscInt *batchSize,PetscInt *numBatches, int *__ierr ){
*__ierr = PetscFEGetTileSizes(
	(PetscFE)PetscToPointer((fem) ),blockSize,numBlocks,batchSize,numBatches);
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetbasisspace_(PetscFE fem,PetscSpace *sp, int *__ierr ){
*__ierr = PetscFEGetBasisSpace(
	(PetscFE)PetscToPointer((fem) ),sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscfesetbasisspace_(PetscFE fem,PetscSpace sp, int *__ierr ){
*__ierr = PetscFESetBasisSpace(
	(PetscFE)PetscToPointer((fem) ),
	(PetscSpace)PetscToPointer((sp) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetdualspace_(PetscFE fem,PetscDualSpace *sp, int *__ierr ){
*__ierr = PetscFEGetDualSpace(
	(PetscFE)PetscToPointer((fem) ),sp);
}
PETSC_EXTERN void PETSC_STDCALL  petscfesetdualspace_(PetscFE fem,PetscDualSpace sp, int *__ierr ){
*__ierr = PetscFESetDualSpace(
	(PetscFE)PetscToPointer((fem) ),
	(PetscDualSpace)PetscToPointer((sp) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetquadrature_(PetscFE fem,PetscQuadrature *q, int *__ierr ){
*__ierr = PetscFEGetQuadrature(
	(PetscFE)PetscToPointer((fem) ),q);
}
PETSC_EXTERN void PETSC_STDCALL  petscfesetquadrature_(PetscFE fem,PetscQuadrature q, int *__ierr ){
*__ierr = PetscFESetQuadrature(
	(PetscFE)PetscToPointer((fem) ),
	(PetscQuadrature)PetscToPointer((q) ));
}
PETSC_EXTERN void PETSC_STDCALL  petscfegetdimension_(PetscFE fem,PetscInt *dim, int *__ierr ){
*__ierr = PetscFEGetDimension(
	(PetscFE)PetscToPointer((fem) ),dim);
}
PETSC_EXTERN void PETSC_STDCALL  petscfecreatedefault_(DM dm,PetscInt *dim,PetscInt *numComp,PetscBool *isSimplex, char prefix[],PetscInt *qorder,PetscFE *fem, int *__ierr ){
*__ierr = PetscFECreateDefault(
	(DM)PetscToPointer((dm) ),*dim,*numComp,*isSimplex,prefix,*qorder,fem);
}
#if defined(__cplusplus)
}
#endif
