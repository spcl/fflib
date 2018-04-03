#ifndef _P4COLL_H_
#define _P4COLL_H_

#include <portals4.h>

#define ULENGTH 1024*1024*50

typedef struct _p4coll_info{
    ptl_handle_ct_t * cts;
    int ctcount;
}p4coll_info;


void p4_init();
ptl_handle_md_t p4_send(int target, void * data, int size, int tag, ptl_handle_ct_t * ct, ptl_handle_ct_t depct, int threshold);
ptl_handle_me_t p4_receive(int initiator, void * data, int size, int tag, ptl_handle_ct_t * ct);
p4coll_info * p4_allgather(void * sndbuff, void * recvbuff, int count, int unitsize, int tag);

void p4_wait(p4coll_info * info);

int p4_get_rank(); 

int p4_get_size();

void p4_barrier();

void p4_free(p4coll_info * info);


void p4_restore_uqueue();

#endif



