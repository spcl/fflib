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

#include "ff_collectives_impl.h"

//NOTE: FF_SHADOW_TAG IS DISABLED
ff_schedule_h ff_scatter(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){
    void * tmp = NULL;
    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    int vrank;
    RANK2VRANK(rank, vrank, root);


    int mask = 0x1;

	ff_schedule_h sched;
    FF_CHECK_FAIL(sched = ff_schedule_create(0, sndbuff, rcvbuff));

    //printf("root: %i; rank: %i; comm_size: %i; datasize: %i; tag: %i\n", root, rank, comm_size, datasize, tag);
    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;
    ff_op_h newsend  = FF_OP_NONE;
    ff_op_h local_copy_rcv, local_copy_snd;

    int blocksize = unitsize*count;

    int mysize = ceil(log2(comm_size));
    int myroot = root;
    int src, dst;

    ff_op_h hook = ff_op_nop_create(0);
    ff_schedule_add(sched, hook);

   

    if (sndbuff==NULL){
        if (rank==root) {
            //printf("Scatter: root with rcvbuff=NULL\n");
            return -1;
        }
        int lg = (int) floor(log2(rank));
        if (lg==log2(rank)) {
            //printf ("root child\n");
            myroot=0;
            mysize = rank;
        }else {
            myroot = pow(2, lg);
            mysize = rank - myroot;
            //printf("scatter: rank: %i; log: %i; myroot: %i; mysize: %i\n", rank, (int) log2(rank), myroot, mysize);
        }
        //printf("allocating: %i\n", blocksize*mysize);
        if (tmp==NULL) {
            sndbuff = ff_malloc(blocksize*mysize);
            //printf("tmpbuff: %x\n", sndbuff);
            ff_schedule_settmpbuff(sched, sndbuff);
        }else sndbuff=tmp;

    }

    //if (rank==root) rcvbuff = sndbuff;

	while (mask < comm_size){

		if ((mask & rank) == 0){


            src = rank | mask;
            if (src < comm_size){

                //printf("[Rank %i] recv from %i of %i bytes with tag: %i\n", ff_get_rank(), peer, unitsize*count, tag);

                int off = src - rank;
                //printf("rank: %i; send to %i; size: %i; off: %i\n", rank, src, off*blocksize, off*blocksize);
                FF_CHECK_FAIL(newsend = ff_op_create_send(sndbuff + off*blocksize, off*blocksize, src, tag));
                if (rank==root) ff_schedule_set_user_dep(sched, newsend);

                //ff_op_hb(lastrecv, lastsend);
                //printf("COLL: send; rank: %i; vrank:%i; r: %i; peer: %i\n", rank, vrank, r, peer);
                ff_schedule_add(sched, newsend);
                ff_op_hb(hook, newsend);
                if (lastsend!=FF_OP_NONE) ff_op_hb(newsend, lastsend);
                lastsend = newsend;

            }
        }else{

            dst = rank ^ mask;
            dst = (dst + root) % comm_size;
            //printf("recv from %i; size: %i\n", dst, blocksize*mysize);
			//lastsend = ff_op_create_send(rcvbuff, blocksize*mysize, dst, tag);
			FF_CHECK_FAIL(lastrecv = ff_op_create_recv(sndbuff, blocksize*mysize, dst, tag));
			ff_schedule_add(sched, lastrecv);
            //ff_op_hb(lastrecv, hook);

            if (sndbuff!=NULL && rcvbuff!=NULL){
                FF_CHECK_FAIL(local_copy_rcv = ff_op_create_recv(rcvbuff, blocksize, rank, tag));
                //ff_op_setopt(local_copy_rcv, FF_SHADOW_TAG);

                FF_CHECK_FAIL(local_copy_snd = ff_op_create_send(sndbuff, blocksize, rank, tag));
                //ff_op_setopt(local_copy_snd, FF_SHADOW_TAG);

                ff_op_hb(lastrecv, local_copy_rcv);
                ff_op_hb(lastrecv, local_copy_snd);

                ff_op_hb(local_copy_rcv, hook);

               
                ff_schedule_add(sched, local_copy_rcv);
                ff_schedule_add(sched, local_copy_snd);
            }else{
                ff_op_hb(lastrecv, hook);
            }

            //the receive is the "indipendent" action.
			ff_schedule_set_indep(sched, lastrecv);
			break;
        }



        mask <<= 1;
	}

	return sched;
}

