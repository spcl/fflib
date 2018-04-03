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

ff_schedule_h ff_allreduce(void * sndbuff, void * recvbuff, int count, ff_tag tag, ff_operator_t operator, ff_datatype_t datatype){
  int r, maxr, vpeer, peer, root, p;


  ff_schedule_h sched;
  ff_peer rank, vrank;

  ff_size_t unitsize = ff_atom_type_size[datatype];
  ff_size_t blocksize = unitsize*count;

  rank = ff_get_rank();
  p = ff_get_size();

  sched = ff_schedule_create(0, sndbuff, recvbuff);

  //tmp buff 
  int nchild = ceil(log(vrank==0 ? p : vrank)) + 1;
  
  //printf("datatype: %i; PTL_DOUBLE: %i; size: %i; blocksize: %i; nchild: %i; vrank: %i; p: %i\n", datatype, PTL_DOUBLE, ff_atom_type_size[datatype], blocksize, nchild, vrank, p); 

  void * tmp = ff_malloc(blocksize*nchild);

  //printf("tmpbuff: %x; nchild: %i\n", tmp, nchild);

  ff_schedule_settmpbuff(sched, tmp);
    

  root = 0; /* this makes the code for ireduce and iallreduce nearly identical - could be changed to improve performance */
  RANK2VRANK(rank, vrank, root);
  maxr = (int)ceil((log2(p)));
  ff_op_h recv=FF_OP_NONE;
  ff_op_h send=FF_OP_NONE;
  ff_op_h comp=FF_OP_NONE;
  ff_op_h hook;
  FF_CHECK_FAIL(hook = ff_op_nop_create(0));
  FF_CHECK_FAIL(ff_schedule_add(sched, hook));

  memcpy(recvbuff, sndbuff, blocksize);

  
  


  for(r=1; r<=maxr; r++) {
    if((vrank % (1<<r)) == 0) {
      /* we have to receive this round */
      vpeer = vrank + (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)
      if(peer<p) {
        //printf("up: recv from %i\n", peer);

        if (tmp + r*blocksize + blocksize > tmp + nchild*blocksize) printf("ERROR!!!\n");
        //printf("recv; tmpbuff: %x; size: %i; (end: %x) recvbuff: %x\n", tmp + r*blocksize, blocksize, tmp+blocksize*nchild, recvbuff);
        FF_CHECK_FAIL(recv = ff_op_create_recv(tmp + r*blocksize, blocksize, peer, tag));
        FF_CHECK_FAIL(comp = ff_op_create_computation(recvbuff, blocksize, tmp + r*blocksize, blocksize, operator, datatype, tag));
        ff_op_hb(recv, comp);
        ff_op_hb(comp, hook);
        //ff_op_hb(recv, hook); //todel
        //comp=recv; //todel
        FF_CHECK_FAIL(ff_schedule_add(sched, recv));
        FF_CHECK_FAIL(ff_schedule_add(sched, comp));
      }
    } else {
      /* we have to send this round */
      vpeer = vrank - (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)

      FF_CHECK_FAIL(send = ff_op_create_send(recvbuff, blocksize, peer, tag));

      if (comp!=FF_OP_NONE) {
        ff_op_hb(hook, send);
      }else{
        ff_schedule_set_indep(sched, send);
        //set user dep
      }
      FF_CHECK_FAIL(ff_schedule_add(sched, send));
      break;
    }
  }

  /* this is the Bcast part - copied with minor changes from nbc_ibcast.c
   * changed: buffer -> recvbuf  */
  RANK2VRANK(rank, vrank, root);

  recv=FF_OP_NONE;

  /* receive from the right hosts  */
  if(vrank != 0) {
    for(r=0; r<maxr; r++) {
      if((vrank >= (1<<r)) && (vrank < (1<<(r+1)))) {
        VRANK2RANK(peer, vrank-(1<<r), root);
        //printf("bcast: recv from: %i\n", peer);
        FF_CHECK_FAIL(recv = ff_op_create_recv(recvbuff, blocksize, peer, tag));
        FF_CHECK_FAIL(ff_schedule_add(sched, recv));
      }
    }
  }

  if (recv==FF_OP_NONE) recv=hook;

  /* now send to the right hosts */
  for(r=0; r<maxr; r++) {
    if(((vrank + (1<<r) < p) && (vrank < (1<<r))) || (vrank == 0)) {
      VRANK2RANK(peer, vrank+(1<<r), root);
      FF_CHECK_FAIL(send = ff_op_create_send(recvbuff, blocksize, peer, tag));
      ff_op_hb(recv, send);
      FF_CHECK_FAIL(ff_schedule_add(sched, send));
    }
  }
  /* end of the bcast */

  return sched;
}


