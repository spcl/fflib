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
 * Torsten Hoefler <htor@inf.ethz.ch>
 */


#include "ff_impl.h"
#include "ff_storage.h"



ff_handle _ff_schedule_create(unsigned int options, ff_schedule ** sched){
    //ff_schedule * sched;
    ff_schedule_h schedh;
#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(schedh = ff_storage_create_schedule(sched));
#else
    FF_CHECK_FAIL((schedh=ff_storage_create(sizeof(ff_schedule), (void **) sched)));
#endif
    //FF_PCALL(PtlCTAlloc(ni_logical, &(sched->startct)));

    return schedh;
}


#ifdef FF_STAT
int ff_schedule_stat(ff_schedule_h schedh, int * ops, int * counters){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    *ops = sched->length;
    *counters = sched->length + sched->_ct_count;
    return FF_SUCCESS;
}

#endif

ff_handle ff_schedule_create(unsigned int options, void * sndbuff, void * rcvbuff){
    ff_schedule * sched;
    ff_handle schedh;
    FF_CHECK_FAIL(schedh = _ff_schedule_create(options, &sched));
    static int id=0; //to remove
    //create the "start" counter
    //FF_PCALL(PtlCTAlloc(ni_logical, &(sched->startop.ct)));
    sched->startop = ff_op_nop_create(0);
    sched->in_use = ff_op_nop_create(FF_DEP_OR);
    sched->user_dep = ff_op_nop_create(0);
    sched->handle = schedh;

#ifdef FF_ONE_CT
    sched->waitop = ff_op_nop_create(0);
#endif

    sched->options = options;
    sched->length = 0;
    sched->is_user_dep = 0;
    sched->tmpbuff=NULL;
    sched->next = NULL;
    sched->id=id++;

    sched->sndbuffer = sndbuff;
    sched->rcvbuffer = rcvbuff;

    return schedh;
}

int ff_schedule_get_buffers(ff_schedule_h schedh, void ** sndbuff, void ** rcvbuff){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    if (sndbuff!=NULL) *sndbuff = sched->sndbuffer;
    if (rcvbuff!=NULL) *rcvbuff = sched->rcvbuffer;
    return FF_SUCCESS;
}

int ff_schedule_satisfy_user_dep(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return ff_op_post(sched->user_dep);
}


int _ff_schedule_add(ff_schedule * sched, ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    //printf("schedule adding op\n");
    if (op->type==1) {;}
    if (sched->length==FF_MAX_OP_PER_SCHED) return FF_FAIL;
    sched->ops[sched->length++] = op;
    return FF_SUCCESS;
}



int ff_schedule_add(ff_schedule_h schedh, ff_op_h oph){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_add(sched, oph);
}



int _ff_schedule_repost(ff_schedule * sched){

    for (int i=0; i<sched->length; i++){
        FF_CHECK_FAIL(_ff_op_reset(sched->ops[i]));
    }    

    FF_CHECK_FAIL(ff_op_reset(sched->startop));
    FF_CHECK_FAIL(ff_op_reset(sched->user_dep));

    for (int i=0; i<sched->length; i++){
        FF_CHECK_FAIL(_ff_op_repost(sched->ops[i]));
    }

    FF_CHECK_FAIL(ff_op_repost(sched->startop));
    FF_CHECK_FAIL(ff_op_repost(sched->user_dep));

    return FF_SUCCESS;
}


int _ff_schedule_post(ff_schedule * sched, int start){
    /*if (start) {
        FF_CHECK_FAIL(ff_op_post(sched->startop));
        FF_CHECK_FAIL(ff_op_post(sched->user_dep));
    }*/

#ifdef FF_ONE_CT
    ff_op * _waitop = (ff_op *) ff_storage_get(sched->waitop);
#endif

    for (int i=0; i<sched->length; i++){
#ifdef FF_ONE_CT
        //if ((op->options & FF_DONT_WAIT) == FF_DONT_WAIT) 
        if (sched->ops[i]->outdegree==0){
            PDEBUG(printf("outdegree: %i; op: %i\n", sched->ops[i]->outdegree, sched->ops[i]->type);)
            _ff_op_hb(sched->ops[i], _waitop);
        }
#endif

        //printf("[Rank %i] posting op.type: %i\n", ff_get_rank(), sched->ops[i]->type);
        FF_CHECK_FAIL(_ff_op_post(sched->ops[i]));

        //printf("[Rank %i] posting done\n", ff_get_rank());
#ifdef FF_STAT
        /* Count the multi-dependency counter */
        if (sched->ops[i]->depcount>1) sched->_ct_count++;
#endif
        
    }

#ifdef FF_ONE_CT
    FF_CHECK_FAIL(_ff_op_post(_waitop));
#endif

    if (start) {
        FF_CHECK_FAIL(ff_op_post(sched->startop));
        FF_CHECK_FAIL(ff_op_post(sched->user_dep));
    }

    //ff_eq_poll(); //!!!
    return FF_SUCCESS;
}








int _ff_schedule_trylock(ff_schedule * sched){
    int i;
    int res = FF_SUCCESS;

    //the schedule cannot be locked if already "in_use".
    //NB: this doesn't lock the schedule. It's just an indicator
    //to check if some "independent" op is already executed.
    //NB2: this could be used to lock the activation of the next schedule
    if (!ff_op_try_lock(sched->in_use)) return FF_FAIL;


    for (i=0; i<sched->length && res==FF_SUCCESS; i++){
        if (1==1 || (sched->ops[i]->options & FF_BUF_DEP) == FF_BUF_DEP){
            res = _ff_op_try_lock(sched->ops[i]);
        }
    }

    //if the i-th operation is in execution, then the whole schedule
    //is in execution => unlock the previously locked ops.
    if (res!=FF_SUCCESS) {
        ff_op_unlock(sched->in_use);
        for (int j=0; j<i; j++) _ff_op_unlock(sched->ops[j]);
    }

    return res;
}

int ff_schedule_trylock(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_trylock(sched);
}


int _ff_schedule_unlock(ff_schedule * sched){
    for (int i=0; i<sched->length; i++){
        _ff_op_unlock(sched->ops[i]);
    }
    return FF_SUCCESS;
}


int ff_schedule_unlock(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_unlock(sched);
}




int ff_schedule_start(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    FF_CHECK_FAIL(ff_op_post(sched->startop));
    FF_CHECK_FAIL(ff_op_post(sched->user_dep));
    return FF_SUCCESS;
}

int ff_schedule_set_indep(ff_schedule_h schedh, ff_op_h oph){
    ff_schedule * sched;

    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));

    //this operation must be posted after the startop is incremented
    ff_op_hb(sched->startop, oph);

    //if this operation is executed (like a receive=>ME append), then
    //mark the schedule as "in use". This must be an "OR" operation:
    //as soon as an indip. operation is executed, the schedule must be marked
    //as "in use".
    ff_op_hb(oph, sched->in_use);

    return FF_SUCCESS;
}


int ff_schedule_set_user_dep(ff_schedule_h schedh, ff_op_h oph){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    sched->is_user_dep = 1;
    ff_op_hb(sched->user_dep, oph);
    return FF_SUCCESS;
}


int ff_schedule_post(ff_schedule_h schedh, int start){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_post(sched, start);
}


int ff_schedule_repost(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_repost(sched);
}


int ff_schedule_free(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_free(sched);
}

int _ff_schedule_free(ff_schedule * sched){

    ff_op_free(sched->startop);
    ff_op_free(sched->in_use);
    ff_op_free(sched->user_dep);

    for (int i=0; i<sched->length; i++){
        _ff_op_free(sched->ops[i]);
#ifdef FF_MEM_REUSE
        ff_storage_release_op(sched->ops[i]);
#endif 
    }


    if (sched->tmpbuff!=NULL) {
        //printf("free tmpbuff: %x\n", sched->tmpbuff);
        ff_free(sched->tmpbuff);
    }
#ifdef FF_MEM_REUSE
    ff_storage_release_schedule(sched);
#endif
    return FF_SUCCESS;
}

int ff_schedule_wait(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_wait(sched);
}


int ff_schedule_settmpbuff(ff_schedule_h schedh, void * buff){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    sched->tmpbuff = buff;
    return FF_SUCCESS;
}


int ff_schedule_test(ff_schedule_h schedh){
    ff_schedule * sched;
    FF_CHECK_NULL(sched = (ff_schedule *) ff_storage_get(schedh));
    return _ff_schedule_test(sched);
}


int _ff_schedule_test(ff_schedule * sched){
    for (int i=0; i<sched->length; i++){
        if (_ff_op_test(sched->ops[i])!=FF_SUCCESS) return FF_FAIL;
    }
    return FF_SUCCESS;
}

int _ff_schedule_wait(ff_schedule * sched){

#ifdef FF_ONE_CT
    FF_CHECK_FAIL(ff_op_wait(sched->waitop));
#else
    //while (1) ff_eq_poll();
    //printf("waiting schedule: %i\n", sched->handle);
    for (int i=0; i<sched->length; i++){
        //TODO: it's enough to wait only the ops with outdegree=0 (no other ops depend on it).
        //but a counter for the outdegree must be introduced in ff_op.
        FF_CHECK_FAIL(_ff_op_wait(sched->ops[i]));
        //printf("[Rank %i] FF_SCHEDULE_WAIT: operation %i of %i completed; must wait: %i\n", ff_get_rank(), i, sched->length, sched->ops[i]->options & FF_DONT_WAIT==FF_DONT_WAIT);
    }
    
#endif

    return FF_SUCCESS;
}

