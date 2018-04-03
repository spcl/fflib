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

ff_op_h ff_op_create_recv(void * buff, ff_size_t length, ff_peer peer, ff_tag tag){
    ff_op * op;
    ff_op_h handle;
    //FF_CHECK_FAIL(handle = ff_op_create(&op));
#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(handle = ff_storage_create_op(&op));
    op->ffhandle = handle;
#else
    FF_CHECK_FAIL(handle = ff_storage_create(sizeof(ff_op), (void **) &op));
#endif

    op->length = length;
    op->type = RECEIVE;
    op->peer = peer;
    op->tag = tag;
    op->buff = buff;
    op->depcount = 0;
    op->options = 0;
    op->locked = 0;
    op->_depct = PTL_CT_NONE;
    op->md=0;
    op->me=0;
    op->rndvct=PTL_CT_NONE;
#ifdef FF_ONECT
    op->outdegree=0;
#endif


    FF_PCALL(PtlCTAlloc(ni_logical, &(op->ct)));
    FF_PCALL(PtlCTSet(op->ct, (ptl_ct_event_t) { 1 , 0 }));
    //printf("[Rank %i] FF_CREATE_RCV: from %i; size: %i; ct_handle: %i; buff: %p\n", ff_get_rank(), (int) peer, (int) length, (int) op->ct, buff);
    return handle;
}

int rtspayload;
int ff_post_receive(ff_op * op, ptl_handle_ct_t depct, int threshold){


    //int threshold = op->depcount;
    if (op->length > FF_RNDVZ_THRESHOLD){

        PDEBUG(printf("rndv receive protocol buff: %p; len: %i; peer: %i\n", op->buff, op->length, op->peer);)
        //ptl_me_t rts; //Now is a ME
        ptl_handle_me_t rts_handle;
        ptl_md_t data;

#ifdef FF_EXPERIMENTAL
        FF_PCALL(PtlCTAlloc(ni_logical, &(op->rndvct)));
        if (depct!=PTL_CT_NONE){
            FF_PCALL(PtlTriggeredCTInc(op->rndvct, (ptl_ct_event_t) {1, 0}, depct, threshold));
        }else{
            FF_PCALL(PtlCTSet(op->rndvct, (ptl_ct_event_t) {1, 0}));
        }
#else
        //Setup the CT on which the TriggeredGet depends (at least for counting the reception of the RTS)
        if (depct==PTL_CT_NONE) {
            FF_PCALL(PtlCTAlloc(ni_logical, &(depct)));
            FF_PCALL(PtlCTSet(depct, (ptl_ct_event_t) {1, 0}));
            op->depct = depct;
            op->_depct = depct;
        }
#endif
        
        //Setup the triggered get
        data.start = op->buff;
        data.length = op->length;
        data.ct_handle = op->ct;
        data.eq_handle = eventqueue;
        data.options = PTL_MD_EVENT_CT_REPLY | PTL_MD_EVENT_SEND_DISABLE | PTL_MD_EVENT_SUCCESS_DISABLE;
        ptl_match_bits_t tag = DATA_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) tag |= SHADOW_TAG;
        FF_PCALL(PtlMDBind(ni_logical, &data, &(op->handle)));

	    op->md = op->handle;


        //Receive the RTS from the sender
        op->data_me.start = &rtspayload;
        op->data_me.length = sizeof(int);
        op->data_me.match_id = (ptl_process_t) { .rank=op->peer};
        ptl_match_bits_t ctrltag = CTRL_RTS_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) ctrltag |= SHADOW_TAG;

        op->data_me.match_bits = ctrltag; // CTRL_RTS_MSG | (ptl_match_bits_t) op->tag;
#ifdef FF_EXPERIMENTAL
        op->data_me.ct_handle = op->rndvct;
#else
        op->data_me.ct_handle = depct;
#endif
        op->data_me.uid = PTL_UID_ANY;
        //rts.options = PTL_ME_EVENT_CT_OVERFLOW | PTL_ME_EVENT_CT_COMM | PTL_ME_OP_PUT;
        op->data_me.options = PTL_ME_USE_ONCE /*| PTL_ME_EVENT_CT_OVERFLOW*/ | PTL_ME_OP_PUT | PTL_ME_EVENT_CT_COMM  | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE;

        //TODO: this must be a triggeredappend if depct!=PTL_CT_NONE
        FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, (void *) op, &(op->me)));

        //PtlTriggeredCTInc(op->ct, (ptl_ct_event_t) {.success=1, .failure=0}, depct, 1);
#ifdef FF_EXPERIMENTAL
        FF_PCALL(PtlTriggeredGet(op->handle, 0, op->length, (ptl_process_t) {.rank=op->peer}, logical_pt_index, tag, 0, NULL, op->rndvct, 2));
#else
        FF_PCALL(PtlTriggeredGet(op->handle, 0, op->length, (ptl_process_t) {.rank=op->peer}, logical_pt_index, tag, 0, NULL, depct, threshold+1));
#endif
    }else{

        //ptl_me_t data;
        ptl_handle_me_t data_handle;

        op->data_me.start = op->buff;
        op->data_me.length = op->length;
        op->data_me.ct_handle = op->ct;

        op->data_me.options = PTL_ME_OP_PUT | PTL_ME_USE_ONCE | /* PTL_ME_EVENT_CT_OVERFLOW |*/ PTL_ME_EVENT_CT_COMM | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE;
        PDEBUG(op->data_me.options ^= PTL_ME_EVENT_COMM_DISABLE;)
        //if (op->buffer->options && FF_BUFFER_REUSE != FF_BUFFER_REUSE) data.options |= PTL_ME_USE_ONCE;

        op->data_me.match_id =  (ptl_process_t) { .rank=op->peer};
        op->data_me.match_bits = DATA_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) {
            //printf("match_bits: %lu\n", op->data_me.match_bits);
            op->data_me.match_bits |= SHADOW_TAG;
            //printf("match_bits + s: %lu; shadow tag: %lu\n", op->data_me.match_bits, SHADOW_TAG);
        }
        //data.ignore_bits = CTRL_MSG_MASK;
        op->data_me.uid = PTL_UID_ANY;
        
        //ptl_me_t test = op->data_me;

        PDEBUG(printf("[Rank %i] posting receive: peer: %i; tag: %i; depcounts: %i: ct: %i; depct: %i\n", ff_get_rank(), op->peer, op->tag, threshold, op->ct, depct);)
        //empty_events();
        //printf("ffcoll_portals_rcv.c: remove the 1=1 here!\n");
        if (depct==PTL_CT_NONE || 1==1) { FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, (void *) op, &(op->me))); }
        else {
            //printf("[Rank %i] triggeredMEAppend depct: %i; threshold: %i\n", ff_get_rank(), depct, threshold);
            FF_PCALL(PtlTriggeredMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, (void *) op, &(op->me), depct, threshold));
        }

        //op->me = data_handle;
        //To check: must be done only in the non-triggered case?
        //check_overflow(op);
    }    
    ff_eq_poll();
    

    return FF_SUCCESS;
}


int ff_repost_receive(ff_op * op) {

    /* reset rndvct if rndv protocol is used */
    if (op->length > FF_RNDVZ_THRESHOLD){
            FF_PCALL(PtlCTSet(op->rndvct, (ptl_ct_event_t) { 1, 0 }));
    }
    
    /* do append: it will be for the rts if rndvz, for the actual data if eager */
    FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, (void *) op, &(op->me)));

    return FF_SUCCESS;  
}

void ff_complete_receive(ptl_event_t event){
    PDEBUG(printf("[Rank %i] copying: initiator: %i; user_ptr: %p\n", ff_get_rank(), event.initiator.rank, event.user_ptr);)
    ff_op * op = (ff_op *) event.user_ptr;

    if ((event.match_bits & DATA_MSG) == DATA_MSG) {
        //PDEBUG(printf("[Rank %i] data overflow; op->ct: %i; op->tag: %i\n", ff_get_rank(), (int) op->ct, op->tag);)
        if (event.start!=NULL){
            ff_size_t blen = op->length;

            //printf("[Rank %i] data overflow; op->ct: %i; op->tag: %i: src: %p; dest: %p; size: %i\n", ff_get_rank(), (int) op->ct, op->tag, event.start, op->buff, event.mlength>blen ? blen : event.mlength );

            //int size = event.mlength>op->size ? op->size : event.mlength;
            memcpy(op->buff, event.start, event.mlength>blen ? blen : event.mlength);
        }

        PtlCTInc(op->ct, (ptl_ct_event_t) {1,0});
    }else if ((event.match_bits & CTRL_RTS_MSG) == CTRL_RTS_MSG){
        PDEBUG(printf("ctrl msg overflow!\n");)
        /*if (op->depcount==1){
            printf("1 dep; op type: %i\n", op->dependency[0]->type);
        }*/
#ifdef FF_EXPERIMENTAL
        PtlCTInc(op->rndvct, (ptl_ct_event_t) {1, 0});
#else
        PtlCTInc(op->depct, (ptl_ct_event_t) {1, 0});
#endif
        PDEBUG(printf("inc done\n");)
        //printf("inc done\n");
    }
    PDEBUG(printf("[Rank %i] copy complete\n", ff_get_rank());)
}

