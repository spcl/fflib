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


#ifndef __FF_COLLECTIVES_H__
#define __FF_COLLECTIVES_H__

ff_schedule_h ff_barrier(ff_tag tag);

ff_schedule_h ff_broadcast(void * data, int count, ff_size_t unitsize, ff_peer root, ff_tag tag);
ff_schedule_h ff_reduce(void * sndbuff, void * rcvbuff, int count, ff_peer root, ff_tag tag, ff_operator_t _operator, ff_datatype_t datatype);
ff_schedule_h ff_gather(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag);
ff_schedule_h ff_scatter(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag);

ff_schedule_h ff_allgather(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag);
ff_schedule_h ff_allreduce(void * sndbuff, void * recvbuff, int count, ff_tag tag, ff_operator_t _operator, ff_datatype_t datatype);
ff_schedule_h ff_alltoall(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag);

ff_schedule_h ff_alltoallv(void * sndbuff, int * sendcounts, int * sdispls, void * recvbuff, int * recvcounts, int * rdispls, ff_size_t unitsize, ff_tag tag);


ff_schedule_h ff_solo_allgather(void * sndbuff, void * recvbuff, int count, ff_size_t unitsize, ff_tag tag);
ff_schedule_h ff_solo_allreduce(void * sndbuff, void * recvbuff, int count, ff_tag tag, ff_operator_t _operator, ff_datatype_t datatype);
ff_schedule_h ff_solo_gather(void * sndbuff, void * rcvbuff, int count, ff_size_t unitsize, ff_peer root, ff_tag tag);


#endif //__FF_COLLECTIVES_H__
