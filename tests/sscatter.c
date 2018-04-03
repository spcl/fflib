#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <mpi/mpi.h>
#include <unistd.h>

#include <time.h>

#include "ff.h"
#include "ff_collectives.h"
#include "test_utils.h"

#define SEED1 123





ff_schedule_h create_singleactivated_scatter(ff_sched_info info){

    int n = ff_get_size();
    int * sndbuff = ((int **) info.userdata)[0];
    int * recbuff = ((int **) info.userdata)[1];
    int * tmp = ((int **) info.userdata)[2];
    //printf("[Rank %i] created: %p; size: %i; tag: %i\n", ff_get_rank(), recbuff, info.unitsize*info.count*n, info.tag + opcount);
    //for (int i=0; i<info.count*n; i++) recbuff[i]=100-opcount;
    return  ff_scatter3(sndbuff, recbuff, info.count, info.unitsize, info.root, info.tag, tmp);

    //return ff_scatter3(sndbuff, recbuff, info.count, info.unitsize, info.root, info.tag);
}



int main(int argc, char ** argv){

    ff_init();

    ff_barrier();
    int root=0;
    int gdb=0;
    //while (gdb==0) {;}
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    ff_container_h container;
    ff_schedule_h sched;

    int count=1;
    int * sndbuff = NULL;
    int * rcvbuff = NULL;
    int * tmpbuff = NULL;
    if (rank!=root) {
        rcvbuff = (int *) malloc(sizeof(int)*count);
        *rcvbuff=0;
        tmpbuff = (int *) malloc(sizeof(int)*count*csize);
    }
    else sndbuff = (int *) malloc(sizeof(int)*csize*count);

    int * buffs[3];
    buffs[0] = sndbuff;
    buffs[1] = rcvbuff;
    buffs[2] = tmpbuff;
    ff_sched_info info;
    info.count=count;
    info.unitsize=sizeof(int);
    info.tag = 10;
    info.userdata = (void *) buffs;
    info.root=root;


    FF_CHECK_FAIL(container = ff_container_create(info, create_singleactivated_scatter));

    int adegree=10;
    //if (rank==root) adegree=1;
    for (int i=0; i<adegree; i++) ff_container_increase_async(container);
    FF_CHECK_FAIL(ff_container_start(container));


    srand(time(NULL)*rank);



    int k = 1;
    int n = k*adegree*10;

    int data = 0;
    for (int i=1; i<=n; i++){
        printf("starting iteration %i\n", i);
        int f = (rank == 1) ? 1000 : 1;
        if (rank!=root) {
            usleep(1000000);
        }


        if (rank==root){
            for (int j=0; j<csize; j++) sndbuff[j] = i*(j+1);
            sched = ff_container_get_next(container);
            //printf("root i: %i; n: %i; sending to 1: %i; sched: %i\n", i, n, sndbuff[1], sched);
            if (sched!=FF_FAIL){

                ff_schedule_satisfy_user_dep(sched);
                //ff_schedule_start(sched);

                printf("[Rank %i] waiting schedule; datas sent: %i\n", rank, *sndbuff);
                ff_container_wait(container);
                ff_container_increase_async(container);
            }else{
                printf("[Rank %i] EMPTY CONTAINER!!!\n", rank);
            }
        }else{

            if (data!=*rcvbuff){
                data=*rcvbuff;
                printf("[Rank %i] new value: %i\n", rank, data);
            }
    
            printf("flushing\n");
            int c = ff_container_flush(container);
            if (c>0) printf("[Rank %i] flushed: %i\n", rank, c);
            for (int j=0; j<c; j++) FF_CHECK_FAIL(ff_container_increase_async(container));
            printf("it: %i; starting new iteration\n", i);
        }

    }

    printf("Completed\n");

    ff_barrier();
    ff_finalize();

    printf("finalized\n");
    
}

