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

static inline ff_schedule_h _ff_gather(ff_schedule_h sched, ff_op_h activation, void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag);

ff_schedule_h ff_gather(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){
	ff_schedule_h sched = ff_schedule_create(0, sndbuff, rcvbuff);
    return _ff_gather(sched, FF_OP_NONE, sndbuff, rcvbuff, count, unitsize, root, tag);
}

ff_schedule_h ff_solo_gather(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){
    ff_op_h activation;
	ff_schedule_h sched = ff_schedule_create(0, sndbuff, rcvbuff);
    ff_activation_bintree(sched, &activation, root, tag);
    return _ff_gather(sched, activation, sndbuff, rcvbuff, count, unitsize, root, tag);
}


static inline ff_schedule_h _ff_gather(ff_schedule_h sched, ff_op_h activation, void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){


    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    //int vrank;
    //RANK2VRANK(rank, vrank, root);


    int mask = 0x1;


    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;

    int blocksize = unitsize*count;

    int mysize = ceil(log2(comm_size));
    int myroot = root;
    int src, dst;

    ff_op_h hook = ff_op_nop_create(0);
    ff_schedule_add(sched, hook);

    if (rcvbuff==NULL){
        if (rank==root) {
            //printf("Gather: root with rcvbuff=NULL\n");
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
        }
        //printf("gathre: rank: %i; log: %i; myroot: %i; mysize: %i\n", rank, (int) log2(rank), myroot, mysize);
        rcvbuff = ff_malloc(blocksize*mysize);
        ff_schedule_settmpbuff(sched, rcvbuff);

    }

    if (sndbuff!=NULL && rcvbuff!=NULL) {
        //printf("copying: rank %i; val: %i; bs: %i\n", rank, * ((int *)sndbuff), blocksize);

        //the memcpy must be offloaded for solo-collectives
        if (activation==FF_OP_NONE){ memcpy(rcvbuff, sndbuff, blocksize); }
        else{/*for solo-gather */
            ff_op_h copy_snd = ff_op_create_send(sndbuff, blocksize, rank, tag);
            ff_op_h copy_rcv = ff_op_create_recv(rcvbuff, blocksize, rank, tag);

            ff_op_hb(activation, copy_snd);
            ff_op_hb(activation, copy_rcv);

            //the send cannot be performed before the copy is finished
            ff_op_hb(copy_rcv, hook);

            ff_schedule_add(sched, copy_snd);
            ff_schedule_add(sched, copy_rcv);
        }
    }
	while (mask < comm_size){

		if ((mask & rank) == 0){

            src = rank | mask;
            if (src < comm_size){

                //printf("[Rank %i] send to %i of %i bytes with tag: %i\n", ff_get_rank(), peer, unitsize*count, tag);
                //lastsend = ff_op_create_send(data, unitsize*count, peer, tag);

                int off = src - rank;
                //printf("rank: %i; recv from %i; size: %i; off: %i\n", rank, src, off*blocksize, off*blocksize);
                lastrecv = ff_op_create_recv(rcvbuff + off*blocksize, off*blocksize, src, tag);

                //ff_op_hb(lastrecv, lastsend);
                //printf("COLL: send; rank: %i; vrank:%i; r: %i; peer: %i\n", rank, vrank, r, peer);
                ff_schedule_add(sched, lastrecv);
                ff_op_hb(lastrecv, hook);

                //the receive is independent
                ff_schedule_set_indep(sched, lastrecv);
            }
        }else{

            dst = rank ^ mask;
            dst = (dst + root) % comm_size;
            //printf("send to %i; size: %i\n", dst, blocksize*mysize);
			lastsend = ff_op_create_send(rcvbuff, blocksize*mysize, dst, tag);
			ff_schedule_add(sched, lastsend);
            ff_op_hb(hook, lastsend);
            
			break;
        }



        mask <<= 1;
	}

	return sched;
}

