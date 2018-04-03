
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <mpi/mpi.h>

#include "ff.h"
#include "ff_collectives.h"
#include "test_utils.h"

#define SEED1 123



int opcount = 0;

void * data = NULL; 
ff_schedule_h create_broadcast(ff_sched_info info){
    //void * data = NULL;
    if (data==NULL) {FF_CHECK_NULL(data = malloc(info.count * info.unitsize));}
    //printf("[Rank %i] buffer: %p\n", ff_get_rank(), data);
    return ff_broadcast(data, info.count, info.unitsize, info.root, info.tag + opcount++);
}


int main(int argc, char ** argv){


    ff_init();


    int root = 0;
    int count = 10;
    int unitsize = sizeof(int);

    int rank = ff_get_rank();
    int commsize = ff_get_size();

    int * sndbuff, * rcvbuff;


    /*if (rank==0) {
        int k = 0;
        while (k==0) {}
    }*/

    ff_tag tag = 10;
    ff_sched_info info;
    info.root=root;
    info.count=count;
    info.unitsize=unitsize;
    info.tag=tag;
    ff_container_h container = ff_container_create(info, create_broadcast);
    int adegree = 10;

    //Set-up multiple versions
    for (int i=0; i<adegree; i++) ff_container_increase_async(container);

    //Start the container: all the schedule are posted and the first one (i.e., the head) is activated
    FF_CHECK_FAIL(ff_container_start(container));



    for (int j=1; j<=adegree; j++){
        //Set-up send buffers if it's the root
        if (rank==root){
            //get the next "usable" schedule
            ff_schedule_h sched = ff_container_get_next(container);

            //get the buffers
            ff_schedule_get_buffers(sched, (void **) &sndbuff, (void **) &rcvbuff);

            //fill the send buffer
            for (int i=0; i<count; i++) sndbuff[i] = i*j;

            //sendbuffer is ready => satisfy user dependecy
            FF_CHECK_FAIL(ff_schedule_satisfy_user_dep(sched));

            
        }

        //wait the next completed schedule
        ff_schedule_h completed = ff_container_wait(container);

        //get the buffers for reading the result
        ff_schedule_get_buffers(completed, (void **) &sndbuff, (void **) &rcvbuff);
        for (int i=0; i<count; i++){
            if (rcvbuff[i]!=i*j) printf("[Rank %i] schedule: %i; expected: %i; received: %i\n", rank, j, i*j, rcvbuff[i]);
        }
    }

    ff_finalize();
}
