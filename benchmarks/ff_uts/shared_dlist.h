#ifndef SHARED_DLIST_H
#define SHARED_DLIST_H

#include <upc_relaxed.h>

typedef shared struct shr_dcell * shr_dlist;

struct shr_dcell
{
  shared void *element;
  shr_dlist next;
  shr_dlist prev;
};

extern shr_dlist shr_dcons(shared void *element, shr_dlist prev, shr_dlist next);
extern shr_dlist shr_create_and_link(shared void *element, shr_dlist prev, shr_dlist next);
extern shared void* shr_unlink_and_free(shr_dlist l);

#endif /* SHARED_DLIST_H */
