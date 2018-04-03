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

#include "ff_impl.h"
#include "ff_storage.h"


/*
ff_container_h ff_container_create(int root, int count, ff_size_t unitsize, ff_tag tag, ff_schedule_h (*create_schedule)(ff_sched_info)){
    ff_container * container;
    ff_container_h handle;
    FF_CHECK_FAIL(handle = ff_storage_create(sizeof(ff_container), (void**) &container));

    container->info.root = root;
    container->info.count = count;
    container->info.unitsize = unitsize;
    container->info.tag = tag;

    container->head = NULL;
    container->tail = NULL;
    container->current = NULL;
    container->adegree = 0;
    container->started = 0;
    container->create_schedule = create_schedule;

    return handle;
}*/

ff_container_h ff_container_create(ff_sched_info info, ff_schedule_h (*create_schedule)(ff_sched_info)){
    ff_container * container;
    ff_container_h handle;
    FF_CHECK_FAIL(handle = ff_storage_create(sizeof(ff_container), (void**) &container));

    container->info = info;

    container->head = NULL;
    container->tail = NULL;
    container->current = NULL;
    container->adegree = 0;
    container->started = 0;
    
    container->create_schedule = create_schedule;
    
    return handle;


}

int ff_container_push(ff_container * container, ff_schedule * newsched){
   if (container->head==NULL){
        //printf("container: new is head: %i\n", newsched->startop, newsched->startop);
        

        newsched->next = NULL;
        container->head = newsched;
        container->current = newsched;
        if (container->started) { FF_CHECK_FAIL(ff_op_post(newsched->startop));};
    }else{
        //printf("container: adding schedule: %i\n", newsched->handle);
        
        FF_CHECK_FAIL(ff_op_hb(container->tail->in_use, newsched->startop));
        container->tail->next = newsched;
        
    }

    container->tail = newsched;

    //printf("container->current: %i; next: %i\n", container->current, container->current->next);
    
    //printf("startop: %i\n", newsched->startop);
    ff_op_post(newsched->startop);
    //printf("in_use: %i\n", newsched->in_use);
    ff_op_post(newsched->in_use);

    FF_CHECK_FAIL(ff_schedule_post(newsched->handle, 0));
    
    return FF_SUCCESS;
}


int ff_container_increase_async(ff_container_h containerh){
    ff_container * container;
    ff_schedule * newsched;
    ff_schedule_h newschedh;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));

    
    //FIXED To be correct, createSchedule() should take a ff_schedule_h (to mantain the abstraciton)
    FF_CHECK_FAIL(newschedh = container->create_schedule(container->info));
    FF_CHECK_NULL(newsched = (ff_schedule *) ff_storage_get(newschedh));

    PDEBUG(printf("[Rank %i] added schedule: %i; startop: %i\n", ff_get_rank(), newsched->handle, newsched->startop);)

    /*
    if (container->head==NULL){
        container->head = newsched;
        container->current = newsched;
        if (container->started) ff_op_post(newsched->startop);
    }



    ff_schedule_post(newschedh);


    if (container->tail!=NULL){
        ff_op_hb(container->tail->in_use, newsched->startop);
        container->tail->next = newsched;
    }

    container->tail = newsched;
    */

    FF_CHECK_FAIL(ff_container_push(container, newsched));

    return FF_SUCCESS;
}


int ff_container_trylock(ff_container_h containerh){
    ff_container * container;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));
    if (container->head==NULL) return FF_FAIL;
    return _ff_schedule_trylock(container->head);
}

int ff_container_unlock(ff_container_h containerh){
    ff_container * container;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));
    if (container->head==NULL) return FF_FAIL;
    return _ff_schedule_unlock(container->head);
}

//This should be called only the first time. Once it is called, a
//schedule should be automatically posted when created. (DONE)
int ff_container_start(ff_container_h containerh){
    ff_container * container;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));
    if (container->started) return FF_FAIL;
    container->started = 1;
    if (container->head!=NULL) return ff_op_post(container->head->startop);
    return FF_SUCCESS;
}

ff_schedule_h ff_container_wait(ff_container_h containerh){
    ff_container * container;
    ff_schedule * schedule;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));

    if (container->head==NULL) return FF_EMPTY;
    
    PDEBUG(printf("[Rank %i] container: waiting schedule: %i\n", ff_get_rank(), container->head->handle);)
    //printf("[Rank %i] container: waiting schedule: %i\n", ff_get_rank(), container->head->handle);

    FF_CHECK_FAIL(_ff_schedule_wait(container->head));
    schedule=container->head;
    container->head = container->head->next;
    if (container->current == schedule) container->current = container->head;
    


    return schedule->handle;
}


ff_schedule_h ff_container_get_head(ff_container_h containerh){
    ff_container * container;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));
    if (container->head==NULL) return FF_FAIL;
    return container->head->handle;
}

ff_schedule_h ff_container_get_next(ff_container_h containerh){
    ff_container * container;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));


    //The next schedule is the one that:
    //1) (has some operations that are user dependent AND this dependency is not satisfied yet) OR
    //2) is not in use.
    while (container->current != NULL &&
        ((container->current->is_user_dep && ff_op_is_executed(container->current->user_dep)) /*||
        ff_op_is_executed(container->current->in_use)*/)){
        
        container->current = container->current->next;
    }

    //printf("container getnext: %i\n", container->current);
    if (container->current==NULL) return FF_FAIL;
    PDEBUG(printf("[Rank %i] container.getNext: %i\n", ff_get_rank(), container->current->handle);)
    //printf("[Rank %i] container.getNext: %i\n", ff_get_rank(), container->current->handle);

    return container->current->handle;
}


int ff_container_flush(ff_container_h containerh){
    ff_container * container;
    ff_schedule * schedule;
    FF_CHECK_NULL(container = (ff_container *) ff_storage_get(containerh));


    if (container->head==NULL) {
        //printf("container: null head\n");
        return 0;
    }
    int c = 0;

    while (container->head!=NULL && _ff_schedule_test(container->head)==FF_SUCCESS){ // && _ff_schedule_trylock(container->head)){
        //printf("container: flusing. schedule: %i\n", container->head);
        schedule=container->head;
        //printf("container: updateing head\n");
        container->head = container->head->next;
        if (container->current == schedule) container->current=container->head;
        PDEBUG(printf("[Rank %i] container: flushing schedule: %i;\n", ff_get_rank(), schedule->handle);)
        //printf("contianer: free\n");
        _ff_schedule_free(schedule);
        //printf("complete\n");
        c++;
    }

    PDEBUG(if (container->head==NULL) { printf("[Rank %i] The container is empty now!\n", ff_get_rank()); });
    if (container->head==NULL) { printf("[Rank %i] The container is empty now!\n", ff_get_rank()); };

    return c;
}

