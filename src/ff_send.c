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

ff_op_h ff_op_create_send(void * buffer, ff_size_t length, ff_peer peer, ff_tag tag){

    ff_op * op;
    ff_op_h handle;

#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(handle = ff_storage_create_op(&op));
    op->ffhandle = handle;
#else
    FF_CHECK_FAIL(handle = ff_storage_create(sizeof(ff_op), (void **) &op));
#endif

    op->buff = buffer;
    op->length = length;
    op->type = SEND;
    op->peer = peer;
    op->tag = tag;
    op->options = 0;
    //op->buff = sndbuff;
    op->depcount = 0;
    op->locked = 0;
    op->md = 0;
    op->_depct = PTL_CT_NONE;
    op->rndvct=PTL_CT_NONE;
    //op->handle=0;
#ifdef FF_ONECT
    op->outdegree=0;
#endif

    FF_PCALL(PtlCTAlloc(ni_logical, &(op->ct)));
    FF_PCALL(PtlCTSet(op->ct, (ptl_ct_event_t) { 1, 0 }));

    //PDEBUG(printf("[Rank %i] Creating send: peer: %i; tag: %i; ct: %i\n", ff_get_rank(), op->peer, op->tag, op->ct);)

    //printf("handle: %i\n", handle);
    //printf("[Rank %i] FF_CREATE_SEND ct: %i\n", ff_get_rank(), op->ct);
    return handle;
}

int rtspayload;

int ff_post_send(ff_op * op, ptl_handle_ct_t depct, int threshold){
    rtspayload=1;

    //int threshold = op->depcount;

    if (op->length > FF_RNDVZ_THRESHOLD){
        ptl_md_t rts;
        ptl_handle_md_t rts_handle;
        //ptl_me_t data;

        //printf("rndv protocol; threashold: %i; msg_size: %i\n", FF_RNDVZ_THRESHOLD, op->length);
        //PDEBUG(printf("rndvz sender: peer: %i\n", op->peer));
        //Post the data that must be read by the receiver
        op->data_me.options = PTL_ME_OP_GET | PTL_ME_USE_ONCE | PTL_ME_EVENT_CT_COMM | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE ; //no overflow here
        op->data_me.start = op->buff; // op->buff;
        op->data_me.length =  op->length;
        op->data_me.match_id = (ptl_process_t) { .rank=op->peer };
        op->data_me.match_bits = DATA_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) {
            //printf("snd tag: %x\n", data.match_bits);
            op->data_me.match_bits |= SHADOW_TAG;
            ////printf("snd tag + s: %x\n", data.match_bits);
        }
        op->data_me.ct_handle = op->ct;
        op->data_me.uid = PTL_UID_ANY;
        FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, NULL, &(op->handle)));

        //Send the RTS to the receiver
        rts.start = &rtspayload;
        rts.length = sizeof(int);
        rts.ct_handle = PTL_CT_NONE;
        rts.eq_handle = eventqueue;
        rts.options = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_SEND_DISABLE | PTL_MD_EVENT_SUCCESS_DISABLE;
        ptl_match_bits_t tag = CTRL_RTS_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) {
            //printf("snd tag: %x\n", data.match_bits);
            tag |= SHADOW_TAG;
            ////printf("snd tag + s: %x\n", data.match_bits);
        }
  
        FF_PCALL(PtlMDBind(ni_logical, &rts, &rts_handle));
    
    	op->md = rts_handle;

        if (depct==PTL_CT_NONE) { FF_PCALL(PtlPut(rts_handle, 0, 0, PTL_NO_ACK_REQ, (ptl_process_t) {.rank=op->peer}, logical_pt_index, tag, 0, (void *) op, 0)); }
        else { FF_PCALL(PtlTriggeredPut(rts_handle, 0, 0, PTL_NO_ACK_REQ, (ptl_process_t) {.rank=op->peer}, logical_pt_index, tag, 0, (void *) op, 0, depct, threshold)); }

    }else{

        ptl_md_t msg;
        ptl_match_bits_t ptag;
        msg.start = op->buff;
        msg.length = op->length; //op->size;
        msg.options = PTL_MD_EVENT_CT_SEND | PTL_MD_EVENT_SUCCESS_DISABLE | PTL_MD_EVENT_SEND_DISABLE;
        PDEBUG(msg.options &= ~PTL_MD_EVENT_SEND_DISABLE;)
        PDEBUG(msg.options &= ~PTL_MD_EVENT_SUCCESS_DISABLE;)
        //msg.eq_handle = PTL_EQ_NONE;
        msg.ct_handle = op->ct;
        msg.eq_handle = eventqueue;
        FF_PCALL(PtlMDBind(ni_logical, &msg, &(op->handle)));
        op->md = op->handle;
        ptag = DATA_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) {
            //printf("snd ptag: %x\n", ptag);
            ptag |= SHADOW_TAG;
            //printf("snd ptag + s: %x\n", ptag);
        }

        PDEBUG(printf("[Rank %i] sending %i byte to %i; triggered: %i; tag: %i; op: %x; ct: %i; op->depcount: %i; depct: %i; threshold: %i\n", ff_get_rank(), op->length, op->peer, (depct!=PTL_CT_NONE), op->tag, op, op->ct, op->depcount, depct, threshold);)
        //Send immediately the data

        if (depct==PTL_CT_NONE) {
            FF_PCALL(PtlPut(op->handle, 0, msg.length, PTL_NO_ACK_REQ, (ptl_process_t) {.rank = op->peer}, logical_pt_index, ptag, 0, op, 0));
        } else { 
            FF_PCALL(PtlTriggeredPut(op->handle, 0, msg.length, PTL_NO_ACK_REQ, (ptl_process_t) {.rank = op->peer}, logical_pt_index, ptag, 0, op, 0, depct, threshold)); 
        }
    }
    //printf("[Rank %i] ###fflib posting send done###\n", ff_get_rank());
    return FF_SUCCESS;
}


int ff_repost_send(ff_op * op){
    ptl_match_bits_t tag;

   
    if (op->length > FF_RNDVZ_THRESHOLD){

        /* re-append ME for the data */
        FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &(op->data_me), PTL_PRIORITY_LIST, NULL, &(op->handle)));
    
        tag = CTRL_RTS_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) tag |= SHADOW_TAG;

        /* Execute the put if no dependecies are found */
        if (op->depct==PTL_CT_NONE) { FF_PCALL(PtlPut(op->md, 0, 0, PTL_NO_ACK_REQ, (ptl_process_t) {.rank=op->peer}, logical_pt_index, tag, 0, (void *) op, 0)); }
    
    }else{ /* eager */

        tag = DATA_MSG | (ptl_match_bits_t) op->tag;
        if ((op->options & FF_SHADOW_TAG) == FF_SHADOW_TAG) tag |= SHADOW_TAG;

        /* Executed the put if no dependencies are found */
        if (op->depct==PTL_CT_NONE) {
            FF_PCALL(PtlPut(op->handle, 0, op->length, PTL_NO_ACK_REQ, (ptl_process_t) {.rank = op->peer}, logical_pt_index, tag, 0, op, 0));
        }     
           
    }

    return FF_SUCCESS;
}
