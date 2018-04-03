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
 * Torsten Hoefler <htor@inf.eth.ch>
 */


#include "ff.h"
#include "ff_impl.h"
#include "ff_storage.h"

#define FF_MPI_INIT
#ifndef FF_MPI_INIT
#include <support.h>
#else
#include <mpi.h>
#endif


static void * overflow_buff;
//static void * overflow_buff;
static ptl_handle_me_t overflow_buff_handle[FF_UQ_SIZE];
//static ptl_handle_me_t overflow_buff_handle;
ptl_handle_me_t unexpected_ctrl_msg_handle;

/**TEST*/
//ptl_handle_ct_t testct;
/**END TEST*/

int ctrlmsg;
ptl_handle_md_t regmd_handle;
int num_procs;

int ff_init(int * argc, char *** argv){

    //int num_procs;

    ptl_me_t unexpected_ctrl_msg;
    //ptl_handle_me_t unexpected_ctrl_msg_handle;

    //Init Portals
    FF_PCALL(PtlInit());

#ifndef FF_MPI_INIT
    //Init support lib (it deals with pmi etc.)
    FF_PCALL(libtest_init());
    num_procs = libtest_get_size();
#else
    MPI_Init(argc, argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
#endif

    printf("[INIT] num_procs (from MPI): %i\n", num_procs);
    FF_PCALL(PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL, &ni_logical));

    //Barrier inside libtest_get_mapping
    //int i;
    //ptl_process_t * pmap = libtest_get_mapping(ni_logical);
    //for (i=0; i<num_procs; i++) printf("i: %i; Rank: %i;\n", i, pmap[i].phys.pid);
#ifndef FF_MPI_INIT
    FF_PCALL(PtlSetMap(ni_logical, num_procs, libtest_get_mapping(ni_logical)));
#else
    //printf("start MAPPING\n");
    ptl_process_t myid;
    PtlGetPhysId(ni_logical, &myid);
    uint32_t sndbuff[2] = {myid.phys.pid, myid.phys.nid};
    uint32_t * rcvbuff = (uint32_t *) malloc(sizeof(uint32_t)*num_procs*2);
    MPI_Allgather(sndbuff, 2, MPI_UNSIGNED, rcvbuff, 2, MPI_UNSIGNED, MPI_COMM_WORLD);

    ptl_process_t * mymap = (ptl_process_t *) malloc(sizeof(ptl_process_t)*num_procs);
    int j=0;
    for (int i=0; i<num_procs; i++){
        mymap[i].phys.pid = rcvbuff[j++];
        mymap[i].phys.nid = rcvbuff[j++];
    }

    FF_PCALL(PtlSetMap(ni_logical, num_procs, mymap));
    //printf("MAPPING DONE\n");
#endif

    //Allocate the event queue
    FF_PCALL(PtlEQAlloc(ni_logical, EQ_COUNT, &eventqueue));

    //Allocate a portal table entry
    FF_PCALL(PtlPTAlloc(ni_logical, 0, eventqueue, PTL_PT_ANY, &logical_pt_index));
    //CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY, &logical_pt_index));
    FF_PCALL(PtlGetId(ni_logical, &myself));

    //Allocate buffer for unexpected control messages (rendez-vous)
    unexpected_ctrl_msg.start         = &ctrlmsg;
    unexpected_ctrl_msg.length        = sizeof(int);
    unexpected_ctrl_msg.uid           = PTL_UID_ANY;
    unexpected_ctrl_msg.match_id.rank = PTL_RANK_ANY;
    unexpected_ctrl_msg.match_bits    = CTRL_MSG_MATCH;
    unexpected_ctrl_msg.ignore_bits   = CTRL_MSG_MASK;
    unexpected_ctrl_msg.options       = PTL_ME_OP_PUT | PTL_ME_EVENT_UNLINK_DISABLE | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE;
    unexpected_ctrl_msg.ct_handle     = PTL_CT_NONE;
    /**TEST*/
    //PtlCTAlloc(ni_logical, &(unexpected_ctrl_msg.ct_handle));
    //PtlCTAlloc(ni_logical, &(testct));
    //PtlTriggeredCTInc(testct, 1, unexpected_ctrl_msg.ct_handle, 1)
    /**END TEST*/
    FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &unexpected_ctrl_msg, PTL_OVERFLOW_LIST, &unexpected_ctrl_msg_handle, &unexpected_ctrl_msg_handle));


    //Allocate buffer for unexpected messages (eager)
    //FIXME: check what happens when an unexpected message is fetched (if the offset is adjusted or not) (probably not).
    
    FF_CHECK_NULL(overflow_buff = malloc(FF_UQ_LENGTH*FF_UQ_SIZE));
    for (int i=0; i<FF_UQ_SIZE; i++){
        //FF_CHECK_NULL(overflow_buff[i] = malloc(sizeof(char)*FF_UQ_LENGTH));
        FF_CHECK_FAIL(append_overflow(i));
    }
   

    ptl_md_t regmd;
    regmd.start = 0;
    regmd.length = (ptl_size_t)-1;
    regmd.options = 0x0;
    regmd.eq_handle = PTL_EQ_NONE;
    regmd.ct_handle = PTL_CT_NONE;
    FF_PCALL(PtlMDBind(ni_logical, &regmd, &regmd_handle));
    
    //ff_op_init();

    allocated=NULL;
    ids=1;
    ops=NULL;
    schedules=NULL;

#ifdef FF_MEM_REUSE
    ff_storage_init();
#endif

    //libtest_barrier();
    ff_init_barrier();

    return FF_SUCCESS;

}



int append_overflow(int i){
    ptl_me_t unexpected_msg;
    unexpected_msg.start          = overflow_buff + i*FF_UQ_LENGTH;
    unexpected_msg.length         = FF_UQ_LENGTH;
    unexpected_msg.ct_handle      = PTL_CT_NONE;
    unexpected_msg.uid            = PTL_UID_ANY;
    unexpected_msg.options        = PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL | PTL_ME_NO_TRUNCATE | PTL_ME_IS_ACCESSIBLE | PTL_ME_EVENT_LINK_DISABLE | PTL_ME_EVENT_COMM_DISABLE;
    //unexpected_msg.min_free       = FF_UQ_LENGTH/2;
    unexpected_msg.min_free       = FF_RNDVZ_THRESHOLD;
    unexpected_msg.match_bits     = DATA_MSG;
    unexpected_msg.ignore_bits    = CTRL_MSG_MASK;
    unexpected_msg.match_id.rank  = PTL_RANK_ANY;

    int r = PtlMEAppend(ni_logical, logical_pt_index, &unexpected_msg, PTL_OVERFLOW_LIST, (void *) (intptr_t) i, &(overflow_buff_handle[i]));
    if (r!=PTL_OK){
        printf("FFCOLL: error while appending overflowbuffer: %i; idx: %i; FF_UQ_LENGTH %i; FF_UQ_SIZE: %i; overflow_buff global start: %p; overflow_buff start %p\n", r, i, FF_UQ_LENGTH, FF_UQ_SIZE, overflow_buff, overflow_buff + i*FF_UQ_LENGTH);
        exit(-1);
    }
       
    return FF_SUCCESS;
}


/*
int append_overflow(int i){
    ptl_me_t unexpected_msg;
    unexpected_msg.start          = overflow_buff;
    unexpected_msg.length         = FF_UQ_LENGTH*FF_UQ_SIZE;
    unexpected_msg.ct_handle      = PTL_CT_NONE;
    unexpected_msg.uid            = PTL_UID_ANY;
    unexpected_msg.options        = PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL | PTL_ME_NO_TRUNCATE | PTL_ME_MAY_ALIGN | PTL_ME_IS_ACCESSIBLE | PTL_ME_EVENT_LINK_DISABLE;
    unexpected_msg.min_free       = 0;
    unexpected_msg.match_bits     = DATA_MSG;
    unexpected_msg.ignore_bits    = CTRL_MSG_MASK;
    unexpected_msg.match_id.rank  = PTL_RANK_ANY;

    FF_PCALL(PtlMEAppend(ni_logical, logical_pt_index, &unexpected_msg, PTL_OVERFLOW_LIST, NULL, &overflow_buff_handle));

    return FF_SUCCESS;
}*/


int ff_init_barrier(){
#ifndef FF_MPI_INIT
    libtest_barrier();
#else
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    return FF_SUCCESS;
}


int ff_finalize(){
    ff_eq_poll();
    int rank = ff_get_rank();
    //libtest_barrier();
    int err;
    for (int i=0; i<FF_UQ_SIZE; i++){
        if (overflow_buff_handle[i] != PTL_INVALID_HANDLE){
            //FF_PCALL(PtlMEUnlink(overflow_buff_handle[i]));
            if ((err=PtlMEUnlink(overflow_buff_handle[i]))!=PTL_OK){
                printf("[Rank %i] unlink error (%i)\n", ff_get_rank(), err);
                //exit(-1);
                //free(overflow_buff[i]);
            }
        }
    }
    free(overflow_buff);
    
    FF_PCALL(PtlMEUnlink(unexpected_ctrl_msg_handle));

#ifdef FF_MPI_INIT
    MPI_Finalize();
#endif
    
    if (ff_get_rank()==0) PtlNIFini(ni_logical);
    PtlFini();
    
    return FF_SUCCESS;
}


/*
struct ibv_pd * ff_get_pd(){
   return _getPD(ni_logical);
}
*/
ff_rank_t ff_get_rank(){
    return myself.rank;
}

ff_size_t ff_get_size(){
    return num_procs; //libtest_get_size();
}



void ff_print_events(){
    ptl_event_t event;
    printf("Event queue:\n");
    while (PtlEQGet(eventqueue, &event)==PTL_OK){
        printf("EVENT TYPE: %i\n", event.type);
    }
}

void ff_eq_poll(){
    ptl_event_t event;
#ifdef DEBUG
    ptl_ct_event_t ev;
#endif

    int res;
    while ((res=PtlEQGet(eventqueue, &event))!=PTL_EQ_EMPTY){
        //printf ("event: %i\n", event.type);   

        if (res!=PTL_OK){
            printf("FFCOL: FATAL: EVENTQUEUE: ERROR: %i!\n", res);
            exit(-1);
        }
        if (event.ni_fail_type==PTL_NI_DROPPED){
            printf("FFCOLL: FATAL: Error: message dropped\n");
            exit(-1);
        }else if (event.ni_fail_type!=PTL_NI_OK){
		    printf("FFCOLL: FATAL: NI FAIL: %i; EVENT TYPE: %i; len: %i\n", event.ni_fail_type, event.type, (int) event.mlength);
            exit(-1);
	    }

        switch (event.type){
            case PTL_EVENT_AUTO_UNLINK:
                //if (event.user_ptr==&unexpected_ctrl_msg_handle) printf("UNLINK!!!\n");
                //The overflow buffers has been unlinked, but not all the related events have been processed
                overflow_buff_handle[(intptr_t) event.user_ptr] = PTL_INVALID_HANDLE;
                //ff_op * op;
                //ff_storage_get(event.user_ptr, &op);
                PDEBUG(printf("[Rank %i] UNLINK unexpected buffer.\n", ff_get_rank());)
                
                
                break;
            case PTL_EVENT_AUTO_FREE:
                //The PTL_EVENT_AUTO_FREE event is posted after all other events associated with the buffer have been posted to the event queue.
                PDEBUG(printf("[Rank %i] AUTOFREE\n", ff_get_rank());)
           

                //append_overflow((intptr_t) event.user_ptr);
                break;
            case PTL_EVENT_PUT_OVERFLOW:
                PDEBUG(printf("[Rank %i] ##OVERFLOW!! buff: %p; tag: %x\n", ff_get_rank(), event.start, event.match_bits);)

                //A receive was posted and an unexpected message was matched (could be either a control or eager message)
                //if (1==1 || (event.match_bits & DATA_MSG) == DATA_MSG) {
                    //printf("[Rank %i] ##OVERFLOW: completing receive\n", ff_get_rank());
                ff_complete_receive(event);
                //printf("complete receive\n");
                //}
                if ((event.match_bits & DATA_MSG) == DATA_MSG){

                    //printf("EVLOOP: data overflow \n");
                    int idx = (int) ((event.start - overflow_buff)/FF_UQ_LENGTH);

                    //printf("[Rank %i] ##OVERFLOW!! ev.start: %p; tag: %i; idx: %i obuff: from %p to %p\n", ff_get_rank(), event.start, event.match_bits, idx, overflow_buff, overflow_buff + FF_UQ_LENGTH*FF_UQ_SIZE);

                    //if (event.start - overflow_buff + idx*FF_UQ_LENGTH >= FF_UQ_LENGTH/2){
                    if (overflow_buff_handle[idx] == PTL_INVALID_HANDLE){
                        PDEBUG(printf("[Rank %i] RE-APPENDING OVERFLOW BUFFER: %i\n", ff_get_rank(), idx));
                        append_overflow(idx);
                    }
                }

                break;
            case PTL_EVENT_ATOMIC_OVERFLOW:
                PDEBUG(printf ("ATOMIC OVERFLOW!!!\n");)
                break;
            case PTL_EVENT_ATOMIC:
                PDEBUG(printf("[Rank %i] ATOMIC EVENT; buff: %x\n", ff_get_rank(), event.start);)
                break;
            case PTL_EVENT_SEND:
                PDEBUG(printf("[Rank %i] ##Put completed: user_ptr: %x; mlength; %i;; peer: %i; tag: %i; ct: %i\n", ff_get_rank(), event.user_ptr, event.mlength, ((ff_op *) event.user_ptr)->peer, ((ff_op *) event.user_ptr)->tag, ((ff_op *) event.user_ptr)->ct);)
                break;
            case PTL_EVENT_PUT:
#ifdef DEBUG
                //PtlCTGet(((ff_op *) event.user_ptr)->ct, &ev);
                printf("[Rank %i] ##PUT EVENT; initiator: %i; user_ptr: %x; size: m: %i, r: %i; buff: %p; tag: %i; ct: %i (%i; %i)\n", ff_get_rank(), (int) event.initiator.rank, event.user_ptr, event.mlength, event.rlength, event.start, event.match_bits, ((ff_op *) event.user_ptr)->ct, ev.success, ev.failure);
#endif
                break;
            default: PDEBUG(printf("------------------\nDEFAULT EVENT: %i\n----------------\n", event.type));
        }
    }
}


/*
int check_overflow(ff_op op){

    ptl_event_t event;

    //TODO: check better to be sure to not keep a different overflow msg.
    if (PtlEQGet(eventqueue, &event)==PTL_OK && event.type==PTL_EVENT_PUT_OVERFLOW && event.match_bits==DATA_MSG | (ptl_match_bits_t) op.tag){
        //copy the message in the request buffer
        memcpy(op.buff, event.start, event.mlength>op.size ? op.size : event.mlength);
    }

    return FF_SUCCESS;
}
*/








