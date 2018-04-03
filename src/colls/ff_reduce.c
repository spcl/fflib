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



ff_schedule_h ff_reduce(void * sndbuff, void * rcvbuff, int count, ff_peer root, ff_tag tag, ff_operator_t operator, ff_datatype_t datatype){
  int vrank, vpeer, peer, maxr, r;
    
  ff_peer rank = ff_get_rank();
  ff_size_t p = ff_get_size();
  ff_size_t unitsize = ff_atom_type_size[datatype];

  ff_schedule_h sched = ff_schedule_create(0, sndbuff, rcvbuff);

  ff_op_h hook = ff_op_nop_create(0);
  ff_schedule_add(sched, hook);

  RANK2VRANK(rank, vrank, root);
  maxr = (int)ceil((log2(p)));

  int ccount=1;
  int blocksize = count*unitsize;

  //tmp buff allocation
  int nchild = ceil(log(vrank==0 ? p : vrank)) + 1;
  //printf("[Rank %i] vrank: %i; p: %i; unitsize: %i; count: %i; nchild: %i; tmpsize: %i\n", rank, vrank, p, unitsize, count, nchild, unitsize*count*nchild);
  void * tmp = ff_malloc(blocksize*nchild);
  ff_schedule_settmpbuff(sched, tmp);


  //the root reduces in the rcvbuff, others in the tmpbuff.
  void * acc = (rank==root) ? rcvbuff : tmp; 
  if (sndbuff!=NULL) memcpy(acc, sndbuff, blocksize);
  

  ff_op_h recv, send, comp;

  for(r=1; r<=maxr; r++) {
    if((vrank % (1<<r)) == 0) {
      // we have to receive this round 
      vpeer = vrank + (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)
      if(peer<p) {

        //printf("[Rank %i] receving in %i; size: %i; from: %i\n", rank, blocksize*ccount, blocksize, peer);
        recv = ff_op_create_recv(tmp+blocksize*ccount, blocksize, peer, tag);
        comp = ff_op_create_computation(acc, blocksize, tmp+blocksize*ccount, blocksize, operator, datatype, tag);
        ff_op_hb(recv, comp);
        //ff_op_hb(recv, hook);
        ff_op_hb(comp, hook);
        ff_schedule_add(sched, recv);
        ff_schedule_add(sched, comp);
        ccount++;
      }
    } else {
      // we have to send this round 
      vpeer = vrank - (1<<(r-1));
      VRANK2RANK(peer, vpeer, root)
      //printf("[Rank %i] sending to %i; size: %i\n", rank, peer, blocksize);
      send = ff_op_create_send(acc, blocksize, peer, tag);
      ff_op_hb(hook, send);
      ff_schedule_add(sched, send);
        
      // leave the game //
      break;
    }
  }
  return sched;
}

/*

//ATTENTION: the content of the sendbuff is modified (it's used as accumulator).
ff_schedule_h ff_reduce(void * sndbuff, void * rcvbuff, int count, ff_peer root, ff_tag tag, ff_operator_t operator, ff_datatype_t datatype, void * tmp){
    ff_peer rank = ff_get_rank();
    ff_size_t comm_size = ff_get_size();
    ff_size_t unitsize = ff_atom_type_size[datatype];

    //printf("[Rank %i] sndbuff: %p; rcvbuff: %p; tmp: %p; csize: %i\n", rank, sndbuff, rcvbuff, tmp, comm_size);
    int vrank;
    RANK2VRANK(rank, vrank, root);

    ff_schedule_h sched = ff_schedule_create(0, sndbuff, rcvbuff);
    
	//ff_schedule_h sched;
    //if (rank==root) sched = ff_schedule_create(0, data, data);
    //else sched = ff_schedule_create(0, NULL, data);
    

    //printf("root: %i; rank: %i; comm_size: %i; datasize: %i; tag: %i\n", root, rank, comm_size, datasize, tag);

	///Goal::t_id recv = -1, send = -1;

    ff_op_h lastrecv = FF_OP_NONE;
    ff_op_h lastsend = FF_OP_NONE;
    ff_op_h lastsum = FF_OP_NONE;

    ff_size_t blocksize = unitsize*count;


    if (rcvbuff==NULL) rcvbuff = sndbuff;

    int nchild = ceil(log(vrank==0 ? comm_size : vrank)) - 1;
    
    if (tmp==NULL) { printf("REDUCE: allocating tmp buff\n"); FF_CHECK_NULL(tmp = (void *) malloc(blocksize*(nchild))); }

    int ccount = 0;

	for (int r = 0; r < ceil(log2(comm_size)); r++) {
		int vpeer = vrank+(int)pow(2,r);
		int peer;
		VRANK2RANK(peer, vpeer, root);
		if ((vrank+pow(2,r) < comm_size) && (vrank < pow(2,r))) {

            //printf("[Rank %i] recv from %i of %i bytes with tag: %i\n", ff_get_rank(), peer, blocksize, tag);


			//lastsend = ff_op_create_send(data, unitsize*count, peer, tag);

            lastrecv = ff_op_create_recv(tmp + ccount*blocksize, blocksize, peer, tag);
            lastsum = ff_op_create_computation(rcvbuff, blocksize, tmp + ccount*blocksize, blocksize, operator, datatype, tag);
            ff_op_hb(lastrecv, lastsum);

            //the sum cannot take place until the sndbuff is specified
            //ff_schedule_set_user_dep(sched, lastsum);

            ccount++;

            ff_schedule_set_indep(sched, lastrecv);

			//printf("COLL: send; rank: %i; vrank:%i; r: %i; peer: %i\n", rank, vrank, r, peer);
			ff_schedule_add(sched, lastrecv);
			ff_schedule_add(sched, lastsum);

		}


        if (lastsum!=FF_OP_NONE && lastsend!=FF_OP_NONE){
            ff_op_hb(lastsum, lastsend);
        }


		vpeer = vrank-(int)pow(2, r);
		VRANK2RANK(peer, vpeer, root);
		if ((vrank >= pow(2,r)) && (vrank < pow(2, r+1))) {
			//recv = goal->Recv(datasize, peer);
			//printf("[Rank %i] vrank: %i; r: %i; root: %i; comm_size: %i; send to %i (%i) of %i bytes with tag: %i\n", ff_get_rank(), vrank, r, root, comm_size, peer, vpeer, blocksize, tag);
			//lastrecv = ff_op_create_recv(data, unitsize*count, peer, tag);

			lastsend = ff_op_create_send(sndbuff, blocksize, peer, tag);
            ff_op_setopt(lastsend, FF_DONT_WAIT);
			ff_schedule_add(sched, lastsend);

            ff_schedule_set_user_dep(sched, lastsend);
		}
	}

	return sched;

}
*/


