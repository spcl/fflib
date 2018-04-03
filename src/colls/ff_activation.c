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

#include "ff_collectives_impl.h"

int ctrlmsg;

int ff_activation_bintree(ff_schedule_h sched, ff_op_h * hook, ff_peer root, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    int vrank;
    RANK2VRANK(rank, vrank, root);

    *hook = ff_op_nop_create(FF_DEP_OR);

    if (rank==root) ff_schedule_set_user_dep(sched, *hook);

    FF_CHECK_FAIL(ff_schedule_add(sched, *hook));

    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;

	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer = vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) && (vrank < pow(2,r))) {

            //printf("[Rank %i] send to %i of %i bytes with tag: %i\n", ff_get_rank(), peer, unitsize*count, tag);
			lastsend = ff_op_create_send(&ctrlmsg, sizeof(ctrlmsg), peer, tag);
			ff_op_setopt(lastsend, FF_SHADOW_TAG);

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
			//printf("[Rank %i] ACTIVATION BINTREE receive from %i with tag: %i; ctrlmsg: %i (%p)\n", ff_get_rank(), peer, tag, ctrlmsg, &ctrlmsg);
			lastrecv = ff_op_create_recv(&ctrlmsg, sizeof(ctrlmsg), peer, tag);
			ff_op_setopt(lastrecv, FF_SHADOW_TAG);

			ff_schedule_add(sched, lastrecv);

            //the receive is the "indipendent" action.
			ff_schedule_set_indep(sched, lastrecv);
			ff_op_hb(lastrecv, *hook);
		}
	}

	return FF_SUCCESS;
}



int ff_activation_tree(ff_schedule_h sched, ff_op_h * hook, ff_tag tag){
	int mask = 0x1;
	int dst;
    int rcvoff=0, sndoff=0;

    //ctrlmsg=24;
    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    //ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);
    //!!!!!memcpy(recvbuff + rank*unitsize, sndbuff, unitsize);


    int logn = (int) log2(comm_size);

    ff_op_h * recvs = (ff_op_h *) malloc(sizeof(ff_op_h)*logn);

    int i = 0;
    //ff_op_h lastrecv=FF_OP_NONE;
    //ff_op_h lastsend=FF_OP_NONE;
    ff_op_h newsend;

    //activation tree
    while (mask < comm_size) {
        dst = rank^mask;

        if (dst < comm_size) {
            rcvoff = dst >> i;
            rcvoff <<= i;

            sndoff = rank >> i;
            sndoff <<= i;

            FF_CHECK_FAIL(newsend = ff_op_create_send(&ctrlmsg, sizeof(ctrlmsg), dst, tag));
            //printf("[Rank %i] activation; send to: %i; op:%i\n", rank, dst, newsend);

            //printf("send setopt\n");
            ff_op_setopt(newsend, FF_DEP_OR | FF_DONT_WAIT | FF_SHADOW_TAG);
            for (int k=0; k<i; k++) ff_op_hb(recvs[k], newsend);
            //printf("i: %i\n", i);
            // It's user-dependent but it has the FF_DEP_OR specified!
            ff_schedule_set_user_dep(sched, newsend);
            //ff_schedule_set_indep(sched, newsend);

            //printf("[Rank %i] send to %i; size %i; off: %i: i: %i; lastrecv: %i; lastsend: %i;  \n", rank, dst, mask, sndoff, i, lastrecv, lastsend);

            FF_CHECK_FAIL(recvs[i] = ff_op_create_recv(&ctrlmsg, sizeof(ctrlmsg), dst, tag));
            ff_schedule_set_indep(sched, recvs[i]);
            //printf("recv setopt\n");
            ff_op_setopt(recvs[i], FF_DONT_WAIT | FF_SHADOW_TAG);


            //printf("[Rank %i] recv from %i; size %i; off: %i; receive: %i\n", (int) rank, dst, mask, rcvoff, recvs[i]);

            ff_schedule_add(sched, newsend);
            ff_schedule_add(sched, recvs[i]);
        }

        i++;
        mask <<= 1;
	}

	*hook = ff_op_nop_create(FF_DEP_OR);


    for (int k=0; k<i; k++) ff_op_hb(recvs[k], *hook);

    /*If the rank is the calling process, then the hook is already satisfied. This is necessary because 
    the schedule is user-activated at the initator node. At the non-initiator nodes there is no problem
    since the dependency are in "OR" for this hook: when it will be activeted through the activation 
    message, the hook will be activated as well. */
    //printf("remove rank==0 in ff_activatiation.c\n");
    ff_schedule_set_user_dep(sched, *hook); 
    FF_CHECK_FAIL(ff_schedule_add(sched, *hook));


    return FF_SUCCESS;
}


