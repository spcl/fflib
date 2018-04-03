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

ff_schedule_h ff_solo_allreduce(void * sndbuff, void * recvbuff, int count, ff_tag tag, ff_operator_t operator, ff_datatype_t datatype){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();

    ff_op_h hook;
    ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);
    ff_activation_tree(sched, &hook, tag);

    int mask = 0x1;
	int dst;
    int rcvoff=0, sndoff=0;

    int logn = (int) log2(comm_size);
 
    ff_size_t unitsize = ff_atom_type_size[datatype];
    ff_size_t blocksize = unitsize*count;
    void * tmp = malloc(blocksize*(logn));



    ff_op_h copy_snd = ff_op_create_send(sndbuff, blocksize, rank, tag);
    ff_op_h copy_rcv = ff_op_create_recv(recvbuff, blocksize, rank, tag);


    //for the lock
    
    ff_op_setopt(copy_snd, FF_BUF_DEP | FF_SHADOW_TAG);
    ff_op_setopt(copy_rcv, FF_SHADOW_TAG);

    ff_op_hb(hook, copy_snd);
    ff_op_hb(hook, copy_rcv);


    FF_CHECK_FAIL(ff_schedule_add(sched, copy_rcv));
    FF_CHECK_FAIL(ff_schedule_add(sched, copy_snd));

    

    int i = 0;
    ff_op_h lastrecv=FF_OP_NONE;
    ff_op_h lastsend=FF_OP_NONE;
    ff_op_h lastcomp=FF_OP_NONE;

    ff_op_h newsend;


    while (mask < comm_size) {
        dst = rank^mask;

        if (dst < comm_size) {
            rcvoff = dst >> i;
            rcvoff <<= i;

            sndoff = rank >> i;
            sndoff <<= i;

            //FF_CHECK_FAIL(newsend = ff_op_create_send(recvbuff + sndoff*blocksize, mask*blocksize, dst, tag));
            FF_CHECK_FAIL(newsend = ff_op_create_send(recvbuff, blocksize, dst, tag));


            //printf("[Rank %i] send to %i; size %i; off: %i: i: %i; lastrecv: %i; lastsend: %i;\n", (int)rank, dst, mask, sndoff, i, lastrecv, lastsend);

            if (lastcomp!=FF_OP_NONE) {
                //printf("[Rank %i] dependency %i->%i\n", rank, lastrecv, newsend);
                //ff_op_wyait(lastrecv);
                ff_op_hb(lastcomp, newsend);
                ff_op_hb(lastsend, newsend);
            }else{
                //printf("[Rank %i] binding send to copy_rcv\n", rank);
                ff_op_hb(copy_rcv, newsend);
                //ff_op_hb(hook, newsend);
            }

            lastsend=newsend;

            //FF_CHECK_FAIL(lastrecv = ff_op_create_recv(recvbuff + rcvoff*blocksize, mask*blocksize, dst, tag));
            FF_CHECK_FAIL(lastrecv = ff_op_create_recv(tmp + (i)*blocksize, blocksize, dst, tag));
            //printf("[Rank %i] creating computation\n", rank);
            FF_CHECK_FAIL(lastcomp = ff_op_create_computation(recvbuff, blocksize, tmp + (i)*blocksize, blocksize, operator, datatype, tag));

            ff_op_hb(lastrecv, lastcomp);
            ff_op_hb(copy_rcv, lastcomp);

            
            //printf("[Rank %i] recv from %i; size %i; off: %i; receive: %i\n", (int) rank, dst, mask, rcvoff, lastrecv);

            FF_CHECK_FAIL(ff_schedule_add(sched, lastsend));
            FF_CHECK_FAIL(ff_schedule_add(sched, lastrecv));
            FF_CHECK_FAIL(ff_schedule_add(sched, lastcomp));
        }

        i++;
        mask <<= 1;
	}

    return sched;

}

