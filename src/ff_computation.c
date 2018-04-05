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
static const size_t type_size[] = {8, 8, 16, 16, 32, 32, 64, 64, 32, 32, 32, 64};


ff_handle pack_in_iovec(void * data, ff_size_t length, uint32_t itemsize, ptl_iovec_t ** iovec_out, uint32_t * iovec_len_out){

    /* check how many items of the given datatype we can fit in one atomic */
    uint32_t io_size = (ptl_limits.max_atomic_size / itemsize) * itemsize;
    uint32_t iovec_len = (length + io_size - 1) / io_size;    
    ptl_iovec_t * iovec;

    ff_handle iovec_handle = ff_storage_create(sizeof(ptl_iovec_t)*iovec_len, (void **) &iovec);    
    uint32_t memleft = length, ividx = 0;
    while (memleft>0) {
        iovec[ividx].iov_base = data + ividx*io_size;
        iovec[ividx].iov_len = MIN(memleft, io_size);
        memleft -= MIN(memleft, io_size); ividx++;
    }
    
    *iovec_out = iovec;
    *iovec_len_out = iovec_len;    

    return iovec_handle;
}

ff_op_h ff_op_create_computation(void * buff1, ff_size_t length1, void * buff2, ff_size_t length2, ff_operator_t _ffoperator, ff_datatype_t _ffdatatype, ff_tag tag){

    ff_op * newop;
    ff_op_h newoph;

    ptl_op_t _operator = op_macro_translate[_ffoperator];
    ptl_datatype_t datatype = type_macro_translate[_ffdatatype];

    ff_size_t length = MIN(length1, length2);

    /* Creating operation */
#ifdef FF_MEM_REUSE
    FF_CHECK_FAIL(newoph = ff_storage_create_op(&newop));
    newop->ffhandle = newoph;
#else
    FF_CHECK_FAIL(newoph = ff_storage_create(sizeof(ff_op), (void **) &newop));
#endif

    FF_PCALL(PtlCTAlloc(ni_logical, &(newop->ct)));
    FF_PCALL(PtlCTSet(newop->ct, (ptl_ct_event_t) { 1, 0 }));
    newop->peer = ff_get_rank();

    /* Creating ME */
    ptl_me_t me;
    ptl_handle_me_t meh;
    me.ct_handle = PTL_CT_NONE;
    me.options = PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_OP_PUT | PTL_ME_USE_ONCE;
    me.match_bits = COMP_MSG |  (ptl_match_bits_t) tag;
    me.uid = PTL_UID_ANY;
    me.match_id =(ptl_process_t) { .rank=ff_get_rank()};

    /* Setting ME and OP */
    if (length > ptl_limits.max_atomic_size){
        printf("WARNING! Operation size bigger than atomic max size => switching to iovecs!!\n");

        ptl_iovec_t *iovec_in, *iovec_out;
        uint32_t iovec_in_len, iovec_out_len;

        ff_handle iovec_out_handle = pack_in_iovec(buff2, length, type_size[_ffdatatype], &iovec_out, &iovec_out_len);
        ff_handle iovec_in_handle = pack_in_iovec(buff1, length, type_size[_ffdatatype], &iovec_in, &iovec_in_len);

        newop->options = FF_IOVEC;
        newop->iovec_in = iovec_in_handle;
        newop->iovec_out = iovec_out_handle;
        newop->length = iovec_out_len;

        me.start = iovec_in;
        me.length = iovec_in_len;
        me.options |= PTL_IOVEC;

        printf("iovec buff: (in: %p, out: %p); len: (in: %u, out: %u), data size: %i; options: %i\n", iovec_in, iovec_out, iovec_in_len, iovec_out_len, length, newop->options);

    }else{
        newop->options = 0;
        newop->buff = buff2;
        newop->length = length;
        me.start = buff1;
        me.length = length;
    }

    newop->_operator = _operator;
    newop->datatype = datatype;
    newop->type = COMPUTATION;
    newop->depcount = 0;
    newop->locked = 0;
    newop->rndvct = PTL_CT_NONE;
    newop->_depct = PTL_CT_NONE;
#ifdef FF_ONECT
    newop->outdegree=0;
#endif
    newop->tag = tag;

    FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &me, PTL_PRIORITY_LIST, NULL, &meh));

    return newoph;
}



int ff_post_computation(ff_op * comp, ptl_handle_ct_t depct, int threshold){

    ptl_md_t op;
    ptl_handle_md_t oph;

    op.length = comp->length;
    op.ct_handle = comp->ct;
    op.eq_handle = eventqueue;
    op.options = PTL_MD_EVENT_CT_ACK;

    printf("posting atomic: options: %i, and: %i, res: %i\n", comp->options, comp->options & FF_IOVEC, (comp->options & FF_IOVEC) == FF_IOVEC);
    if ((comp->options & FF_IOVEC) == FF_IOVEC){
        op.start = ff_storage_get(comp->iovec_out);
        printf("posting atomic: iovec: %p\n", op.start);
        op.options |= PTL_IOVEC;
    }else{
        op.start = comp->buff;
    }

    FF_PCALL(PtlMDBind(ni_logical, &op, &oph));

    comp->handle=oph;
    comp->md = oph;


    if (depct==PTL_CT_NONE){
        //printf("atomic\n");
        FF_PCALL(PtlAtomic(oph, 0, op.length, PTL_ACK_REQ, (ptl_process_t) {.rank=comp->peer}, logical_pt_index, COMP_MSG| (ptl_match_bits_t) comp->tag, 0, NULL, 0, comp->_operator, comp->datatype));
    }else{
        //printf("triggered_atomic: depct: %i; depcount: %i; threshold: %i; op->length: %i; peer: %i; operator: %i; datatype: %i\n", depct, comp->depcount, threshold, op.length, comp->peer, comp->_operator, comp->datatype);
        FF_PCALL(PtlTriggeredAtomic(oph, 0, op.length, PTL_ACK_REQ, (ptl_process_t) {.rank=comp->peer}, logical_pt_index, COMP_MSG | (ptl_match_bits_t) comp->tag, 0, NULL, 0, comp->_operator, comp->datatype, depct, threshold));
    }

    return FF_SUCCESS;
}

void ff_op_free_computation(ff_op * comp){
    if ((comp->options & FF_IOVEC) == FF_IOVEC){
        ff_storage_delete(comp->iovec_out);
        ff_storage_delete(comp->iovec_in);
    }
}
