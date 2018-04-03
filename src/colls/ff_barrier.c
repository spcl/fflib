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

ff_schedule_h ff_barrier(ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t p = ff_get_size();

    ff_schedule_h sched;
    FF_CHECK_FAIL(sched = ff_schedule_create(0, NULL, NULL));

    int maxround = (int) ceil(log2(p)-1);
    
    ff_op_h * lastrecv = (ff_op_h *) malloc(sizeof(ff_op_h)*maxround);
    ff_op_h lastsend = FF_OP_NONE;
    
    int sendpeer, recvpeer, round=-1;

    

    char * tmp = (char *) ff_malloc(sizeof(char)*2);
    ff_schedule_settmpbuff(sched, tmp);
        

    do{
        round++;
        sendpeer = (rank + (1<<round)) % p;
        recvpeer = (rank - (1<<round)) % p;
        
        FF_CHECK_FAIL(lastsend = ff_op_create_send(&tmp[0], sizeof(char), sendpeer, tag));
        //if (lastrecv!=FF_OP_NONE) ff_op_hb(lastrecv, lastsend);
        for (int i=0; i<round; i++) ff_op_hb(lastrecv[i], lastsend);

        //printf("[Rank %i] maxround: %i; send to %i depends on: %i\n", rank, maxround, sendpeer, lastrecv);
        FF_CHECK_FAIL(lastrecv[round] = ff_op_create_recv(&tmp[1], sizeof(char), recvpeer, tag));
        //printf("[Rank %i] recv from %i\n", rank, recvpeer);
        ff_schedule_add(sched, lastsend);
        ff_schedule_add(sched, lastrecv[round]);

    }while(round<maxround);

    free(lastrecv);    

    return sched;
}

