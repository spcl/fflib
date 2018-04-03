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

ff_schedule_h ff_alltoall(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();
    ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);

    ff_op_h lastsend, lastrecv;

    ff_size_t blocksize = unitsize*count;
    memcpy(recvbuff + rank*blocksize, sndbuff + rank*blocksize, blocksize);
    
    /* linear algorithm */
    for (int i=0; i<comm_size; i++){
        if (i==rank) { continue; }

        //printf("[Rank %i] sending to %i; receiving from: %i; i: %i\n", rank, i, i, i);
        FF_CHECK_FAIL(lastrecv = ff_op_create_recv(recvbuff + i*blocksize, blocksize, i, tag));
        FF_CHECK_FAIL(lastsend = ff_op_create_send(sndbuff + i*blocksize, blocksize, i, tag));

        FF_CHECK_FAIL(ff_schedule_add(sched, lastrecv));
        FF_CHECK_FAIL(ff_schedule_add(sched, lastsend));
    }
	
    return sched;
}


ff_schedule_h ff_alltoallv(void * sndbuff, int * sendcounts, int * sdispls, void * recvbuff, int * recvcounts, int * rdispls, ff_size_t unitsize, ff_tag tag){

    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();
    ff_schedule_h sched = ff_schedule_create(0, sndbuff, recvbuff);

    ff_op_h lastsend, lastrecv;
    int srecv, ssend, drecv, dsend;

    //printf("commsize: %i: local rcv offset: %i; local snd offset: %i; localre\n", comm_size);
    //ff_size_t blocksize = unitsize*count;
    memcpy(recvbuff + rdispls[rank]*unitsize, sndbuff + sdispls[rank]*unitsize, recvcounts[rank]*unitsize);    

    /* linear algorithm */
    for (int i=0; i<comm_size; i++){
        if (i==rank) { continue; }
        drecv = rdispls[i]*unitsize;
        dsend = sdispls[i]*unitsize;
        srecv = recvcounts[i]*unitsize;
        ssend = sendcounts[i]*unitsize;

        //printf("[Rank %i] sending to %i; receiving from: %i; i: %i\n", rank, i, i, i);
        printf("[Rank %i] recv offset: %i; size: %i; send offset: %i; size: %i\n", rank, drecv, srecv, dsend, ssend);
        FF_CHECK_FAIL(lastrecv = ff_op_create_recv(recvbuff + drecv, srecv, i, tag));
        FF_CHECK_FAIL(lastsend = ff_op_create_send(sndbuff + dsend, ssend, i, tag));

        FF_CHECK_FAIL(ff_schedule_add(sched, lastrecv));
        FF_CHECK_FAIL(ff_schedule_add(sched, lastsend));
    }
	
    return sched;
}


