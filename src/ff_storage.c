/* Copyright (c) 2014 ETH - Eidgenössische Technische Hochschule Zürich
 * Scalable and Parallel Computing Lab - (http://spcl.inf.ethz.ch/)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer listed
 *    in this license in the documentation and/or other materials
 *     provided with the distribution.
 * 
 *  - Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * The copyright holders provide no reassurances that the source code
 * provided does not infringe any patent, copyright, or any other
 * intellectual property rights of third parties.  The copyright holders
 * disclaim any liability to any recipient for claims brought against
 * recipient by any third party for infringement of that parties
 * intellectual property rights.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors
 * ========
 * Salvatore Di Girolamo <digirols@inf.ethz.ch>
 */

#include "ff_storage.h"
#include "uthash.h"

/*
struct hentry{
    int id;
    void * addr;
    int size;
    UT_hash_handle hh;
};


struct hentry *allocated = NULL;
int ids = 1;
*/
#ifdef FF_MEM_REUSE
//ff_op * ops=NULL;
//ff_schedule * schedules=NULL;
//ff_container * containers=NULL;
/*
ff_handle ff_storage_create_op(ff_op ** ptr){
    int handle;
    if (ops==NULL) return ff_storage_create(sizeof(ff_op),(void **) ptr);
    *ptr = ops;
    handle = ops->ffhandle;
    ops = ops->trash_next;
    return handle;
}

ff_handle ff_storage_create_schedule(ff_schedule ** ptr){
    int handle;
    if (schedules==NULL) return ff_storage_create(sizeof(ff_schedule), (void **) ptr);
    *ptr = schedules;
    handle = schedules->handle;
    schedules = schedules->trash_next;
    return handle;
}
*/



#ifdef FF_ALLOCATOR

#define FF_MEMSIZE 1024*1024*4
#define FF_ALLOC_MIN 64

 
#define MNEXT(PTR) = (ff_mblock *)(PTR + ((ff_mblock *) PTL - sizeof(ff_mblock))
typedef struct _ff_mblock{
    size_t size;
    struct _ff_mblock * next;
    int free;
}ff_mblock;

void * ffmem;


#endif

void * ff_malloc(size_t size){
    
#ifdef FF_ALLOCATOR
    ff_mblock * current = (ff_mblock *) ffmem;
    
    PDEBUG(printf("[Rank %i] allocating %i bytes\n", ff_get_rank(), size);)
    
    while (current!=NULL) {
        PDEBUG(printf("current->size: %i: required size: %i; current->free: %i; current->next: %x\n", current->size, size, current->free, current->next);)
        if (current->size >= size && current->free) break;

        /* try to recompact */
        if (current->next!=NULL && current->free && current->next->free){
            /* merge */
            current->size += current->next->size + sizeof(ff_mblock);
            current->next = current->next->next;
        }else current = current->next;
    }

    //printf("[Rank %i] block: %x; !=NULL: %i\n", ff_get_rank(), current, current!=NULL);
    if (current==NULL) {
        printf("[Rank %i] ff_malloc: not enough memory.\n", ff_get_rank());
        return NULL;
    }

    current->free = 0;
        
    if (current->size - size > sizeof(ff_mblock) + FF_ALLOC_MIN){
        //printf("[Rank %i] splitting\n", ff_get_rank());
        /* split the slot */
        ff_mblock * newblock = ((void *) current) + sizeof(ff_mblock) + size;
        //printf("[Rank %i] splitting: newblock: %x; end of data: %x\n", ff_get_rank(), newblock, ffmem + FF_MEMSIZE);
        newblock->next = current->next;
        newblock->free = 1;
        current->next = newblock;
        newblock->size = current->size - size - sizeof(ff_mblock);
        current->size = size;
        //printf("[Rank %i] splitting ok\n", ff_get_rank());
    }

    PDEBUG(printf("allocated ff_mbloc: %x; data: %x; sizeof(ff_mblock): %i (hex: %x)\n", current, current + sizeof(ff_mblock), sizeof(ff_mblock), sizeof(ff_mblock));)
    //printf("[Rank %i] allocated ff_mbloc: %x; data: %x; sizeof(ff_mblock): %i (hex: %x)\n", ff_get_rank(), current, current + sizeof(ff_mblock), sizeof(ff_mblock), sizeof(ff_mblock));

    return ((void *) current) + sizeof(ff_mblock);
#else
    return malloc(size);
#endif
}



void ff_free(void * ptr){
#ifdef FF_ALLOCATOR
    if (ptr==NULL) return;
    //printf("[Rank %i] free at %x; ff_mblock %x;\n", ff_get_rank(), ptr, (ff_mblock *) (ptr - sizeof(ff_mblock)));
    
    PDEBUG(printf("free at %x; ff_mblock %x;\n", ptr, (ff_mblock *) (ptr - sizeof(ff_mblock)));)
    ff_mblock * mb = (ff_mblock *) (ptr - sizeof(ff_mblock));
    mb->free = 1;
    /* the merging of free block should be done here */   
#else 
    free(ptr);
#endif 
}



void ff_storage_init(){
    struct hentry * he = (struct hentry *) malloc(sizeof(struct hentry)*FF_STORAGE_PRE_OP);

    ops = (ff_op *) malloc(sizeof(ff_op)*FF_STORAGE_PRE_OP);
    for (int i=0; i<FF_STORAGE_PRE_OP; i++){
        ops[i].trash_next = (i<FF_STORAGE_PRE_OP-1) ? &(ops[i+1]) : NULL;
        he[i].addr = &(ops[i]);
        he[i].id = ids++;
        he[i].size = sizeof(ff_op);
        HASH_ADD_INT(allocated, id, &(he[i]));
        ops[i].ffhandle = he[i].id;
        //printf("init: created: %i\n", ops[i].ffhandle);
    }
#ifdef FF_ALLOCATOR
    ffmem = malloc(FF_MEMSIZE);
    if (ffmem==NULL) { printf("FF_STORAGE: Error allocating memory.\n"); exit(-1); }
    ((ff_mblock *) ffmem)->size = FF_MEMSIZE - sizeof(ff_mblock);
    ((ff_mblock *) ffmem)->next = NULL;
    ((ff_mblock *) ffmem)->free = 1;
#endif
    
}

void ff_storage_release_op(ff_op * op){
    op->trash_next = ops;
    op->ct=0;
    ops = op;
}

void ff_storage_release_schedule(ff_schedule * schedule){
    schedule->trash_next = schedules;
    schedules = schedule;
}
#endif

ff_handle ff_storage_create(int size, void ** ptr){
    struct hentry * he = (struct hentry *) malloc(sizeof(struct hentry));
    *ptr = malloc(size);
    //printf("storage: malloc of %i at %x\n", size, *ptr);
    if (he==NULL || *ptr==NULL) return FF_FAIL;
    he->addr = *ptr;
    he->id = ids++;
    he->size = size;
    HASH_ADD_INT(allocated, id, he);
    return he->id;
}
/*
void * ff_storage_get(ff_handle handle){
    struct hentry * he;
    HASH_FIND_INT(allocated, &handle, he);
    if (he==NULL) return NULL;
    return he->addr;
}
*/


void ff_storage_delete(ff_handle handle){
    struct hentry * he = NULL;
    HASH_FIND_INT(allocated, &handle, he);
    if (he!=NULL) {
        HASH_DEL(allocated, he);
        free(he->addr);
        free(he);
    }
}
