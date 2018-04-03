#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "p4coll.h"
#include <math.h>
#include <support.h>



ptl_handle_ni_t ni_logical;
ptl_handle_eq_t eventqueue;
ptl_pt_index_t logical_pt_index;
ptl_process_t myself;
int rank;
int csize;
ptl_handle_me_t uhandler;
void * udata;
int init;

int check_overflow(void * data, int size){
    ptl_event_t event;
    while (PtlEQGet(eventqueue, &event)==PTL_OK){

        if (event.ni_fail_type==PTL_NI_DROPPED){
            printf("DUMMYLIB: Error: message dropped\n");

        }else if (event.ni_fail_type!=PTL_NI_OK){
            printf("DUMMYLIB: NI FAIL: %i\n", event.ni_fail_type);
        }

        if (event.type==PTL_EVENT_PUT_OVERFLOW){
            memcpy(data, event.start, size);
            return 1;
        }         
    }
    return 0;
}

int p4_get_rank(){
    return rank;
}  

int p4_get_size(){
    return csize;
}


void p4_barrier(){
    libtest_barrier();
}


void p4_init(){
    

    PtlInit();

    //Init support lib (it deals with pmi etc.)
    libtest_init();

    int num_procs = libtest_get_size();

    PtlNIInit(PTL_IFACE_DEFAULT, PTL_NI_MATCHING | PTL_NI_LOGICAL, PTL_PID_ANY, NULL, NULL, &ni_logical);

    PtlSetMap(ni_logical, num_procs, libtest_get_mapping(ni_logical));

    //Allocate the event queue
    PtlEQAlloc(ni_logical, 100, &eventqueue);

    //Allocate a portal table entry
    PtlPTAlloc(ni_logical, 0, eventqueue, PTL_PT_ANY, &logical_pt_index);
    //CHECK_RETURNVAL(PtlPTAlloc(ni_logical, 0, PTL_EQ_NONE, PTL_PT_ANY, &logical_pt_index));
    PtlGetId(ni_logical, &myself);

    rank = myself.rank;
    csize = num_procs;

    init=0;
    //Append overflow buffer
    udata = malloc(ULENGTH);
    p4_restore_uqueue();
    init=1;
    libtest_barrier();
}



void p4_restore_uqueue(){  
    //if (init==1) PtlMEUnlink(uhandler);
    int res;
    ptl_me_t unexpected_msg;
    unexpected_msg.start          = udata;
    unexpected_msg.length         = ULENGTH;
    unexpected_msg.ct_handle      = PTL_CT_NONE;
    unexpected_msg.uid            = PTL_UID_ANY;
    unexpected_msg.options        = PTL_ME_OP_PUT | PTL_ME_MANAGE_LOCAL | PTL_ME_NO_TRUNCATE | PTL_ME_MAY_ALIGN | PTL_ME_IS_ACCESSIBLE | PTL_ME_EVENT_LINK_DISABLE;
    unexpected_msg.min_free       = 0;
    unexpected_msg.match_bits     = 1;
    unexpected_msg.ignore_bits    = 0xFFFFFFFF;
    unexpected_msg.match_id.rank  = PTL_RANK_ANY;
    if ((res=PtlMEAppend(ni_logical, logical_pt_index, &unexpected_msg, PTL_OVERFLOW_LIST, NULL, &uhandler)) != PTL_OK){
	printf("Error allocating shadow buffer: %i\n", res);
    }
	
}



ptl_handle_md_t p4_send(int target, void * data, int size, int tag, ptl_handle_ct_t * ct, ptl_handle_ct_t depct, int threshold){
    ptl_md_t msg;
    ptl_handle_md_t msgh;

    msg.start = data;
    msg.length = size;
    msg.options = PTL_MD_EVENT_CT_SEND;
    PtlCTAlloc(ni_logical, ct);
    msg.ct_handle = *ct;
    msg.eq_handle = PTL_EQ_NONE;
    PtlMDBind(ni_logical, &msg, &msgh);
    
    if (depct==PTL_CT_NONE){

        PtlPut(msgh, 0, size, PTL_NO_ACK_REQ, (ptl_process_t) {.rank = target}, logical_pt_index, tag, 0, NULL, 0);   
    }else{
        //printf("triggered put\n");
        PtlTriggeredPut(msgh, 0, size, PTL_NO_ACK_REQ, (ptl_process_t) {.rank = target}, logical_pt_index, tag, 0, NULL, 0, depct, threshold);   
    }

    return msgh;
}

ptl_handle_me_t p4_receive(int initiator, void * data, int size, int tag, ptl_handle_ct_t * ct){

    ptl_me_t me;
    ptl_handle_me_t data_handle;

    me.start = data;
    me.length = size;
    PtlCTAlloc(ni_logical, ct);
    me.ct_handle = *ct;

    me.options = PTL_ME_OP_PUT | PTL_ME_USE_ONCE | /* PTL_ME_EVENT_CT_OVERFLOW |*/ PTL_ME_EVENT_CT_COMM | PTL_ME_EVENT_UNLINK_DISABLE;
    
    me.match_id = (ptl_process_t) { .rank=initiator};
    me.match_bits = tag;
    me.uid = PTL_UID_ANY;

    PtlMEAppend(ni_logical, logical_pt_index, &me, PTL_PRIORITY_LIST, NULL, &data_handle);
    if (check_overflow(data, size)){
        //printf("OVERFLOW\n");
        PtlCTInc(*ct, (ptl_ct_event_t) {1, 0});
    }    

    return data_handle;
}

p4coll_info * p4_allgather(void * sndbuff, void * recvbuff, int count, int unitsize, int tag){

 

    int mask = 0x1;
    int dst;
    int rcvoff=0, sndoff=0;


    int blocksize = unitsize*count;
    memcpy(recvbuff + rank*blocksize, sndbuff, unitsize);


    int i = 0;
    int cti = 0;    
    int logn = log2(csize);

    p4coll_info * info = malloc(sizeof(p4coll_info));
    info->ctcount = logn*2;
    info->cts = malloc(sizeof(ptl_handle_ct_t)*info->ctcount);    

    while (mask < csize) {
        dst = rank^mask;

        if (dst < csize) {
            rcvoff = dst >> i;
            rcvoff <<= i;

            sndoff = rank >> i;
            sndoff <<= i;

            //FF_CHECK_FAIL(newsend = ff_op_create_send(recvbuff + sndoff*blocksize, mask*blocksize, dst, tag));
            ptl_handle_ct_t depct = (cti>0) ? info->cts[cti-1] : PTL_CT_NONE;
            p4_send(dst, recvbuff + sndoff*blocksize, mask*blocksize, tag, &(info->cts[cti]), depct, 1);

            //printf("[Rank %i] send to %i; size %i; off: %i: i: %i; cti: %i\n", (int)rank, dst, mask, sndoff, i, cti);
            cti++;

            //FF_CHECK_FAIL(lastrecv = ff_op_create_recv(recvbuff + rcvoff*blocksize, mask*blocksize, dst, tag));
            p4_receive(dst, recvbuff + rcvoff*blocksize, mask*blocksize, tag, &(info->cts[cti]));
            

            //printf("[Rank %i] recv from %i; size %i; off: %i; i: %i; cti: %i\n", (int) rank, dst, mask, rcvoff, i, cti);
            cti++;
        }

        i++;
        mask <<= 1;
    }

    //printf("logn: %i; cti: %i\n", logn, cti);
    return info;

}


void p4_wait(p4coll_info * info){
    ptl_ct_event_t ev;
    for (int i=0; i<info->ctcount; i++){
        //printf("waiting for %i (%i)\n", i, info->cts[i]);
        PtlCTWait(info->cts[i], 1, &ev);
    }
}

void p4_free(p4coll_info * info){
    for (int i=0; i<info->ctcount; i++){
        PtlCTFree(info->cts[i]);
    }

}
