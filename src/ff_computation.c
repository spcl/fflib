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
#include "ff_impl.h"


static const ptl_op_t op_macro_translate[] = {PTL_MIN, PTL_MAX, PTL_SUM, PTL_PROD};
static const ptl_datatype_t type_macro_translate[] = {PTL_INT8_T, PTL_UINT8_T, PTL_INT16_T, PTL_UINT16_T, PTL_INT32_T, PTL_UINT32_T, PTL_INT64_T, PTL_UINT64_T, PTL_FLOAT, PTL_DOUBLE};

ff_op_h ff_op_create_computation(void * buff1, ff_size_t length1, void * buff2, ff_size_t length2, ff_operator_t _ffoperator, ff_datatype_t _ffdatatype, ff_tag tag){

    ff_op * newop;
    ff_op_h newoph;

    ptl_op_t _operator = op_macro_translate[_ffoperator];
    ptl_datatype_t datatype = type_macro_translate[_ffdatatype];

    //ff_op * op;

    //FF_CHECK_NULL(op=ff_storage_get(oph));

    ptl_me_t me;
    ptl_handle_me_t meh;

    //This thing is never cleaned-up ==> fix
    me.start = buff1;
    me.length = length1;
    //****FF_PCALL(PtlCTAlloc(ni_logical, &(me.ct_handle)));
    me.ct_handle = PTL_CT_NONE;

    //printf("COMPUTATION ct: %i\n", me.ct_handle);
    //***me.options = PTL_ME_EVENT_CT_COMM | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_OP_PUT | PTL_ME_USE_ONCE;
    me.options = PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_OP_PUT | PTL_ME_USE_ONCE;

   
    me.match_bits = COMP_MSG |  (ptl_match_bits_t) tag;
    me.uid = PTL_UID_ANY;

    me.match_id =(ptl_process_t) { .rank=ff_get_rank()};

    FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &me, PTL_PRIORITY_LIST, NULL, &meh));

#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(newoph = ff_storage_create_op(&newop));
    newop->ffhandle = newoph;
#else
    FF_CHECK_FAIL(newoph = ff_storage_create(sizeof(ff_op), (void **) &newop));
    //FF_CHECK_NULL(newop->buffer = ff_storage_get(buffer2_handle));
#endif

    FF_PCALL(PtlCTAlloc(ni_logical, &(newop->ct)));
    //***newop->ct = me.ct_handle;

    //printf("[Rank %i] computation ct: %i; mid: %i\n", ff_get_rank(), (int) newop->ct, me.match_id.rank);

    FF_PCALL(PtlCTSet(newop->ct, (ptl_ct_event_t) { 1, 0 }));

    newop->peer = ff_get_rank();

    //newop->handle = oph;
    newop->buff = buff2;
    newop->length = length2;
    newop->_operator = _operator;
    newop->datatype = datatype;
    newop->type = COMPUTATION;
    newop->options = 0;
    newop->depcount = 0;
    newop->locked = 0;
    newop->rndvct = PTL_CT_NONE;
    newop->_depct = PTL_CT_NONE;
#ifdef FF_ONECT
    newop->outdegree=0;
#endif


    //must match
    newop->tag = tag;

    return newoph;
}



int ff_post_computation(ff_op * comp, ptl_handle_ct_t depct, int threshold){

    ptl_md_t op;
    ptl_handle_md_t oph;

    op.start = comp->buff;
    op.length = comp->length;
    op.ct_handle = comp->ct;
    op.eq_handle = eventqueue;
    op.options = PTL_MD_EVENT_CT_ACK;


    FF_PCALL(PtlMDBind(ni_logical, &op, &oph));


    comp->handle=oph;
    comp->md = oph;


    if (depct==PTL_CT_NONE){
        //printf("atomic\n");
        FF_PCALL(PtlAtomic(oph, 0, op.length, PTL_ACK_REQ, (ptl_process_t) {.rank=comp->peer}, logical_pt_index, COMP_MSG| (ptl_match_bits_t) comp->tag, 0, NULL, 0, comp->_operator, comp->datatype));
    }else{
        //printf("triggered_atomic: depct: %i; depcount: %i; threshold: %i; op->length: %i\n", depct, comp->depcount, threshold, op.length);
        FF_PCALL(PtlTriggeredAtomic(oph, 0, op.length, PTL_ACK_REQ, (ptl_process_t) {.rank=comp->peer}, logical_pt_index, COMP_MSG | (ptl_match_bits_t) comp->tag, 0, NULL, 0, comp->_operator, comp->datatype, depct, threshold));
    }

    return FF_SUCCESS;
}
