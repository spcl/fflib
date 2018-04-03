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


#ifndef _FF_STORAGE_H_
#define _FF_STORAGE_H_

#include "ff_impl.h"

#include "uthash.h"

struct hentry{
    int id;
    void * addr;
    int size;
    UT_hash_handle hh;
};

//struct hentry *allocated = NULL;
//int ids = 1;


void ff_storage_init();


__attribute__((always_inline)) inline void * ff_storage_get(ff_handle handle){
    struct hentry * he;
    HASH_FIND_INT(allocated, &handle, he);
    if (he==NULL) return NULL;
    return he->addr;
}


ff_handle ff_storage_create(int size, void ** ptr);
///void * ff_storage_get(ff_handle handle)  __attribute__((always_inline));
void ff_storage_delete(ff_handle handle);



#ifdef FF_MEM_REUSE
//ff_handle ff_storage_create_op(ff_op ** ptr);
//ff_handle ff_storage_create_schedule(ff_schedule ** ptr);


void ff_storage_init();

__attribute__((always_inline)) inline ff_handle ff_storage_create_op(ff_op ** ptr){
    int handle;
    if (ops==NULL) {
        //printf("creating\n");
        return ff_storage_create(sizeof(ff_op),(void **) ptr);
    }
    //printf("reusing %i\n", ops->ffhandle);
    *ptr = ops;
    handle = ops->ffhandle;
    ops = ops->trash_next;
    return handle;
}

__attribute__((always_inline)) inline ff_handle ff_storage_create_schedule(ff_schedule ** ptr){
    int handle;
    if (schedules==NULL) return ff_storage_create(sizeof(ff_schedule), (void **) ptr);
    *ptr = schedules;
    handle = schedules->handle;
    schedules = schedules->trash_next;
    return handle;
}



void ff_storage_release_op(ff_op * op);

void ff_storage_release_schedule(ff_schedule * schedule);
#endif

#endif //__FF_STORAGE_H__
