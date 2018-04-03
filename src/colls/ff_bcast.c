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

ff_schedule_h ff_broadcast(void * data, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    //int vrank;
    //RANK2VRANK(rank, vrank, root);


    int mask = 0x1;

    ff_schedule_h sched;
    FF_CHECK_FAIL(sched = ff_schedule_create(0, data, data));

    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;
    ff_op_h newsend  = FF_OP_NONE;
   

    int blocksize = unitsize*count;

    //int mysize = ceil(log2(comm_size));
    int myroot = root;
    int src, dst;

    ff_op_h hook = ff_op_nop_create(0);
    ff_schedule_add(sched, hook);

   

    if (rank!=root){
        int lg = (int) floor(log2(rank));
        if (lg==log2(rank)) {
            //printf ("root child\n");
            myroot=0;
            //mysize = rank;
        }else {
            myroot = pow(2, lg);
            //mysize = rank - myroot;
        }
        //printf("allocating: %i\n", blocksize*mysize);
       
    }


    while (mask < comm_size){

        if ((mask & rank) == 0){
            src = rank | mask;
            if (src < comm_size){

                FF_CHECK_FAIL(newsend = ff_op_create_send(data, blocksize, src, tag));
                if (rank==root) ff_schedule_set_user_dep(sched, newsend);

                ff_schedule_add(sched, newsend);
                ff_op_hb(hook, newsend);
                if (lastsend!=FF_OP_NONE) ff_op_hb(newsend, lastsend);
                lastsend = newsend;
            }
        }else{

            dst = rank ^ mask;
            dst = (dst + root) % comm_size;
            FF_CHECK_FAIL(lastrecv = ff_op_create_recv(data, blocksize, dst, tag));
            ff_schedule_add(sched, lastrecv);

            ff_op_hb(lastrecv, hook);
            
            //the receive is the "indipendent" action.
            ff_schedule_set_indep(sched, lastrecv);
            break;
        }

        mask <<= 1;
    }

    return sched;
}

/*
ff_schedule_h ff_broadcast_old(void * data, int count, ff_size_t unitsize, ff_peer root, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    int vrank;
    RANK2VRANK(rank, vrank, root);


	ff_schedule_h sched;
    if (rank==root) sched = ff_schedule_create(0, data, data);
    else sched = ff_schedule_create(0, NULL, data);

    //printf("root: %i; rank: %i; comm_size: %i; datasize: %i; tag: %i\n", root, rank, comm_size, datasize, tag);

	///Goal::t_id recv = -1, send = -1;

    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;

	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer = vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) && (vrank < pow(2,r))) {

            //printf("[Rank %i] send to %i of %i bytes with tag: %i\n", ff_get_rank(), peer, unitsize*count, tag);
			lastsend = ff_op_create_send(data, unitsize*count, peer, tag);

			//if rank is the root then it must wait for posting of the send buffer
			if (rank==root) {
                ff_schedule_set_user_dep(sched, lastsend);
                ff_schedule_set_indep(sched, lastsend);
            }
			//printf("COLL: send; rank: %i; vrank:%i; r: %i; peer: %i\n", rank, vrank, r, peer);
			ff_schedule_add(sched, lastsend);

		}


        if (lastsend!=FF_OP_NONE && lastrecv!=FF_OP_NONE){
            ff_op_hb(lastrecv, lastsend);
        }


		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) && (vrank < pow(2, r+1))) {
			//recv = goal->Recv(datasize, peer);
			//printf("[Rank %i] receive from %i of %i bytes with tag: %i\n", ff_get_rank(), peer, unitsize*count, tag);
			lastrecv = ff_op_create_recv(data, unitsize*count, peer, tag);
			ff_schedule_add(sched, lastrecv);

            //the receive is the "indipendent" action.
			ff_schedule_set_indep(sched, lastrecv);
		}
	}

	return sched;
}
*/
