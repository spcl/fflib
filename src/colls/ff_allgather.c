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

static inline ff_schedule_h _ff_allgather(ff_schedule_h sched, ff_op_h activation, void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag);


ff_schedule_h ff_allgather(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag){
    ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);
    return _ff_allgather(sched, FF_OP_NONE, sndbuff, recvbuff, count, unitsize, tag);
}

ff_schedule_h ff_solo_allgather(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag){
    ff_op_h activation;
    ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);
    printf("[Rank %i] activation tree\n", ff_get_rank());
    ff_activation_tree(sched, &activation, tag);
    return _ff_allgather(sched, activation, sndbuff, recvbuff, count, unitsize, tag);

}

static inline ff_schedule_h _ff_allgather(ff_schedule_h sched, ff_op_h activation, void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    int mask = 0x1;
    int dst;
    int rcvoff=0, sndoff=0;

    ff_size_t blocksize = unitsize*count;

    ff_op_h hook = FF_OP_NONE;

    if (activation==FF_OP_NONE){
        memcpy(recvbuff + rank*blocksize, sndbuff, blocksize);
    }else{/* solo case */
        ff_op_h copy_snd = ff_op_create_send(sndbuff, blocksize, rank, tag);
        ff_op_h copy_rcv = ff_op_create_recv(recvbuff + rank*blocksize, blocksize, rank, tag);

        ff_op_hb(activation, copy_snd);
        ff_op_hb(activation, copy_rcv);

        //the send cannot be performed before the copy is finished
        hook = copy_rcv;

        ff_schedule_add(sched, copy_rcv);
        ff_schedule_add(sched, copy_snd);
        
    }

    int i = 0;
    int logn = (int) ceil(log2(comm_size));
    //ff_op_h lastrecv=FF_OP_NONE;
    ff_op_h * lastrecv = (ff_op_h *) malloc(sizeof(ff_op_h)*logn);
    ff_op_h lastsend=FF_OP_NONE; 
    

    while (mask < comm_size) {
        dst = rank^mask;

        if (dst < comm_size) {
            rcvoff = dst >> i;
            rcvoff <<= i;

            sndoff = rank >> i;
            sndoff <<= i;
            
            FF_CHECK_FAIL(lastsend = ff_op_create_send(recvbuff + sndoff*blocksize, mask*blocksize, dst, tag));
            //printf("[Rank %i] send to %i; size %i; off: %i: i: %i;\n", (int)rank, dst, mask, sndoff, i);
            for (int k=0; k<i; k++) ff_op_hb(lastrecv[k], lastsend);
            
            //printf("[Rank %i] recv from %i; size: %i; off: %i\n", rank, dst, mask, rcvoff);
            FF_CHECK_FAIL(lastrecv[i] = ff_op_create_recv(recvbuff + rcvoff*blocksize, mask*blocksize, dst, tag));

            if (activation!=FF_OP_NONE){
                ff_op_hb(hook, lastsend);
                //the receive should depend from hook as well (now is broken due to Portals).
            }

            FF_CHECK_FAIL(ff_schedule_add(sched, lastsend));
            FF_CHECK_FAIL(ff_schedule_add(sched, lastrecv[i]));

            i++;
        }
        mask <<= 1;
	}
    return sched;
}

