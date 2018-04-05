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






//A NOP is a non-operation. However, since it is an "ff_op", a counter is associated
//with it.
int ff_nop_post(ff_op * op, ptl_handle_ct_t depct, int threshold){

        PDEBUG(printf("[Rank %i] Posting NOP: op: %x; ct: %i; depct: %i; depcount: %i; (dep0: %i; dep1: %i); threshold: %i\n", ff_get_rank(), op, op->ct, depct, op->depcount, (op->depcount>0) ? op->dependency[0]->ct : -1, (op->depcount>1) ? op->dependency[1]->ct : -1,  threshold);)
        if (depct==PTL_CT_NONE) {
            FF_PCALL(PtlCTInc(op->ct, (ptl_ct_event_t) { 1 , 0 }));

            //ptl_ct_event_t ev;
            //PtlCTGet(op->ct, &ev);
            //printf("[Rank %i] nop; ct incremented: %i; current value: (%i, %i)\n", ff_get_rank(), (int) op->ct, (int) ev.success, (int) ev.failure);

        }else{
            //int threshold = op->options == FF_NOP_OR ? 1 : op->depcount;
            //printf("[Rank %i] nop; ct: %i depending on %i. threshold: %i; depcount: %i\n", ff_get_rank(), op->ct, depct, threshold, op->depcount);
            FF_PCALL(PtlTriggeredCTInc(op->ct, (ptl_ct_event_t) { 1 , 0 }, depct, threshold));
        }
        return FF_SUCCESS;
}

int ff_nop_repost(ff_op * op){
    if (op->depct==PTL_CT_NONE) {
        PDEBUG(printf("ff_op.c: NOP reposting: incrementing ct\n");)

        FF_PCALL(PtlCTInc(op->ct, (ptl_ct_event_t) { 1 , 0 }));
    }PDEBUG(else printf("ff_op.c: NOP reposting: triggered increment ct\n");)
}


ff_op_h ff_op_nop_create(int options){
    ff_op * op;
    ff_op_h handle;
#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(handle = ff_storage_create_op(&op));
    op->ffhandle = handle;
#else
    FF_CHECK_FAIL(handle = ff_storage_create(sizeof(ff_op), (void **) &op));
#endif

    op->buff = NULL;
    op->length = 0;
    op->type = NOP;
    op->peer = 0;
    op->tag = 0;
    op->rndvct=PTL_CT_NONE;
    op->options = options;
    op->_depct = PTL_CT_NONE;
    //op->buff = sndbuff;
    
#ifdef FF_ONE_CT
    op->outdegree=0;
#endif

    op->depcount = 0;
    FF_PCALL(PtlCTAlloc(ni_logical, &(op->ct)));
    FF_PCALL(PtlCTSet(op->ct, (ptl_ct_event_t) { 1, 0 }));

    PDEBUG(printf("[Rank %i] Creating NOP: op: %x; ct: %i;\n", ff_get_rank(), op, op->ct);)

    //printf("[Rank %i] nop handle: %i; ct: %i\n", ff_get_rank(), handle, op->ct);
    return handle;
}


int ff_op_try_lock(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_try_lock(op);
}

int ff_op_unlock(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_unlock(op);
}

int _ff_op_try_lock(ff_op * op){

    ptl_ct_event_t ev;
    FF_PCALL(PtlCTInc(op->depct, (ptl_ct_event_t) { -1, 0 }));
    FF_PCALL(PtlCTGet(op->depct, &ev));

    if (op->depct == PTL_CT_NONE) return FF_FAIL;
    if (ev.success == op->depct-1){
        FF_PCALL(PtlCTInc(op->depct, (ptl_ct_event_t) { 1, 0 }));
        return FF_FAIL;
    }

    op->locked = 1;
    return FF_SUCCESS;

}

int _ff_op_unlock(ff_op * op){
    if (op->locked) {
        FF_PCALL(PtlCTInc(op->depct, (ptl_ct_event_t) { 1, 0 }));
        op->locked = 0;
    }
    return FF_SUCCESS;
}



int _ff_op_post(ff_op * op){

    ptl_handle_ct_t depct = PTL_CT_NONE;

    PDEBUG(printf("---posting op: type: %i\n", op->type);)

    //Install dependecies
    if (op->depcount==1){
        //printf("[Rank %i] FF_POST_OP: 1; type: %i; dependency found. ct_handle: %i; depct: %i\n", ff_get_rank(), op->type, op->ct, op->dependency[0]->ct);
        depct = op->dependency[0]->ct;
    }else if(op->depcount>1){
        FF_PCALL(PtlCTAlloc(ni_logical, &depct));
        
        FF_PCALL(PtlCTSet(depct, (ptl_ct_event_t) { 1, 0 }));
        op->_depct = depct;
        //printf("depcount: %i; rank: %i; peer: %i; type: %i\n", op->depcount, ff_get_rank(), op->peer, op->type);
        for (int i=0; i<op->depcount; i++){
            //printf("[Rank %i] multidep: %i\n", ff_get_rank(), (int) op->dependency[i]->ct);
            FF_PCALL(PtlTriggeredCTInc(depct, (ptl_ct_event_t) { 1, 0 }, op->dependency[i]->ct, 2));
        }
    }

    int threshold = (op->options & FF_DEP_OR) == FF_DEP_OR ? 1 : op->depcount;

    //add 1 to the threshold for the schedule locking
    threshold += 1;


    //TODO: fix ff_post_* to don't take depct as arg (since now is part of the op struct)
    op->depct = depct;

    //Post operation
    if (op->type==SEND) { FF_CHECK_FAIL(ff_post_send(op, depct, threshold)); }
    else if (op->type==RECEIVE) { FF_CHECK_FAIL(ff_post_receive(op, depct, threshold)); }
    else if (op->type==COMPUTATION) { FF_CHECK_FAIL(ff_post_computation(op, depct, threshold)); }
    else if (op->type==NOP) { FF_CHECK_FAIL(ff_nop_post(op, depct, threshold)); }
    else{
        printf("Not implemented!\n");
        return FF_FAIL;
    }

    PDEBUG(printf("---end posting\n\n");)
    return FF_SUCCESS;
}


int _ff_op_reset(ff_op * op){
    /* Reset op ct */
    FF_PCALL(PtlCTSet(op->ct, (ptl_ct_event_t) { 1 , 0 }));

    /* reset depct if it's an intermediate counter (depcount>1) */
    if (op->depcount>1 && op->depct!=PTL_CT_NONE){
        FF_PCALL(PtlCTSet(op->depct, (ptl_ct_event_t) { 1, 0 }));
    }

    return FF_SUCCESS;
}

int _ff_op_repost(ff_op * op){
    /* TODO: CHECK FOR FF_REUSE OPTION */
    
    printf("--- REposting op: type: %i\n", op->type);
    
    if (op->type==SEND) { FF_CHECK_FAIL(ff_repost_send(op)); }
    else if (op->type==RECEIVE) { FF_CHECK_FAIL(ff_repost_receive(op)); }
    else if (op->type==COMPUTATION) { 
        printf("Computation repost: not implemented\n"); 
        return FF_FAIL;
    }else if (op->type==NOP) { FF_CHECK_FAIL(ff_nop_repost(op)); }
    else{
        printf("Not implemented!\n");
        return FF_FAIL;
    }

    printf("---end REposting\n\n");
    return FF_SUCCESS;
}

int ff_op_repost(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_repost(op);
}

int ff_op_reset(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_reset(op);
}


int ff_op_post(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_post(op);
}

int ff_op_setopt(ff_op_h oph, int options){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    op->options |= options;
    //printf("options: %lu\n", op->options);
    //if ((op->options & FF_SHADOW_TAG)==FF_SHADOW_TAG) printf("shadow tag detected\n");
    return FF_SUCCESS;
}





int ff_op_test(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_test(op);
}

int _ff_op_test(ff_op * op){
    ptl_ct_event_t ev;
    if ((op->options & FF_DONT_WAIT) == FF_DONT_WAIT) return FF_SUCCESS;
    FF_PCALL(PtlCTGet(op->ct, &ev));

    ///PDEBUG(printf("[Rank %i] TESTING OP: op_ptr: %x; type: %i; ct_handle: %i; current value: (%i, %i); peer: %i; tag: %i\n", ff_get_rank(), op, op->type, (int) op->ct, ev.success, ev.failure, (int) op->peer, op->tag);)

    ///ff_eq_poll();
    if (ev.success==2) return FF_SUCCESS;
    return FF_FAIL;
}




int ff_op_wait(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    return _ff_op_wait(op);
}


int _ff_op_wait(ff_op * op){
    ptl_ct_event_t ev;
    if ((op->options & FF_DONT_WAIT) == FF_DONT_WAIT) {
        PDEBUG(printf("[Rank %i] FF_WAIT_OP (skipping): op_ptr: %x; type: %i; ct_handle: %i; current value: (%i, %i); peer: %i\n", ff_get_rank(), op, op->type, (int) op->ct, ev.success, ev.failure, (int) op->peer);)
        return FF_SUCCESS;
    }
    PDEBUG(FF_PCALL(PtlCTGet(op->ct, &ev));)



    PDEBUG(printf("[Rank %i] FF_WAIT_OP (start): op_ptr: %x; type: %i; ct_handle: %i; current value: (%i, %i); peer: %i; tag: %i\n", ff_get_rank(), op, op->type, (int) op->ct, ev.success, ev.failure, (int) op->peer, op->tag);)
    ////ff_eq_poll();


#ifndef FF_BUSY_WAITING
    FF_PCALL(PtlCTWait(op->ct, 2, &ev));
#else
    int term=0;
    while (!term){
        PDEBUG(ff_eq_poll();) //comment this!
        FF_PCALL(PtlCTGet(op->ct, &ev));
        term = ev.success==2 || ev.failure>0;
    }
#endif

    PDEBUG(printf("[Rank %i] FF_WAIT_OP (stop): ct_handle: %i; current value: (%i, %i)\n", ff_get_rank(), (int) op->ct, ev.success, ev.failure);)
    if (ev.failure!=0) {
        printf("error on ct: %i; op: %i; failures: %i; success: %i; length: %i\n", op->ct, op->type, (int) ev.failure, (int) ev.success, (int) op->length);
        ////ff_eq_poll();
        exit(-1);
        return FF_FAIL;
    }
    return FF_SUCCESS;
}



int ff_op_hb(ff_op_h beforeh, ff_op_h afterh){

    ff_op * after, * before;
    FF_CHECK_NULL(after = (ff_op *) ff_storage_get(afterh));
    FF_CHECK_NULL(before = (ff_op *) ff_storage_get(beforeh));
    return _ff_op_hb(before, after);
}


int _ff_op_hb(ff_op * before, ff_op * after){

    if (after->depcount >= FF_MAX_DEPENDECIES) return FF_FAIL;
    after->dependency[after->depcount++] = before;

#ifdef FF_ONE_CT
    before->outdegree++;
#endif
    return FF_SUCCESS;
}



int ff_op_free(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));

    if (_ff_op_free(op)==FF_SUCCESS){
#ifdef FF_MEM_REUSE
        ff_storage_release_op(op);
#else
        ff_storage_delete(oph);
#endif
        return FF_SUCCESS;
    }
    return FF_FAIL;
}

int _ff_op_free(ff_op * op){

    /* TODO: ensure that an op is freed only when all its deps are already frees.
       Otherwise, you will get an PTL_ARG_INVALID from the PTLMDRelease (if there is 
       an MD AND if the op is marked as FF_DONT_WAIT (see activation tree case). */

    /* operation specific operations */
    if (op->type==SEND) {;}
    else if (op->type==RECEIVE) {;}
    else if (op->type==COMPUTATION) { ff_op_free_computation(op); }
    else if (op->type==NOP) {;}

    if (op->_depct!=PTL_CT_NONE) {
        FF_PCALL(PtlCTFree(op->_depct));
    }

    if (op->md!=0 && (op->type==SEND || op->type==RECEIVE || op->type==COMPUTATION)){
        PDEBUG(printf("[Rank %i] MD release; tag: %i\n", ff_get_rank(), op->tag);)
        FF_PCALL(PtlMDRelease(op->md));
    }
    
    //if (op->type==RECEIVE && op->me != 0) FF_PCALL(PtlMEUnlink(op->me));
    PDEBUG(printf("[Rank %i] removing ct %i\n", ff_get_rank(), op->ct);)
    FF_PCALL(PtlCTFree(op->ct));
    if (op->rndvct!=PTL_CT_NONE) FF_PCALL(PtlCTFree(op->rndvct));
    //free(op);

    return FF_SUCCESS;
}


int ff_op_is_executed(ff_op_h oph){
    ff_op * op;
    FF_CHECK_NULL(op = (ff_op *) ff_storage_get(oph));
    ptl_ct_event_t ctstatus;

    FF_PCALL(PtlCTGet(op->ct, &ctstatus));
    if (ctstatus.failure!=0) return FF_FAIL;
    return ctstatus.success>=2;
}


/*
ff_op * opset[FF_OP_LENGTH];
unsigned int nextop;


void ff_op_init(){
    memset(opset, 0, FF_OP_LENGTH);
    nextop=0;
}

ff_op_h ff_op_create(ff_op ** _op){
    ff_op * op;
    unsigned int arrid;
    FF_CHECK_NULL(op = (ff_op *) malloc(sizeof(ff_op)));

    op->next = NULL;
    //op->nextdep = NULL;
    op->id = nextop;

    arrid = nextop % FF_OP_LENGTH;

    if (opset[arrid]!=NULL) op->next = opset[arrid];
    opset[arrid] = op;
    *_op = op;
    //printf("op created: %i; arrid: %i\n", nextop, arrid);
    nextop++;
    return op->id;
}



ff_op * ff_op_get(ff_op_h id){
    unsigned int arrid;
    ff_op * tmp;
    arrid = id % FF_OP_LENGTH;
    //printf("op searching: %i; nextop: %i; arrid: %i; firstid: %i\n", id, nextop, arrid, 0);
    tmp = opset[arrid];
    while (tmp!=NULL && tmp->id!=id) tmp=tmp->next;
    return tmp;
}


void remove_op(ff_op_h id){
    unsigned int arrid;
    ff_op * tmp;
    arrid = nextop % FF_OP_LENGTH;

    if ((tmp=opset[arrid])==NULL) return;


    if (tmp->id==id) {
        opset[arrid] = tmp->next;
        free(tmp);
    }else{
        while (tmp->next!=NULL && tmp->next->id!=id) tmp = tmp->next;
        if (tmp->next!=NULL){
            tmp->next = tmp->next->next;
            free(tmp->next);
        }
    }
}
*/
