

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

ff_schedule_h create_singleactivated_allreduce(ff_sched_info info){

    int n = ff_get_size();
    uint64_t * sndbuff = ((uint64_t **) info.userdata)[0];
    uint64_t * recbuff = ((uint64_t **) info.userdata)[1];
    //printf("[Rank %i] created: %p; size: %i; tag: %i\n", ff_get_rank(), recbuff, info.unitsize*info.count*n, info.tag + opcount);
    //for (int i=0; i<info.count*n; i++) recbuff[i]=100-opcount;

    return ff_oneside_allreduce(sndbuff, recbuff, info.count, info.unitsize, info.tag + opcount++, info._operator, info.datatype);

}



int main(int argc, char ** argv){
    MPI_Init(&argc, &argv);
    ff_init();

    //int gdb=0;
    //while (gdb==0) {;}
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    ff_container_h container;
    ff_schedule_h sched;

    //int bufn = 100;

    uint64_t sndbuff = -1;
    uint64_t rcvbuff; // = (int *) malloc(sizeof(int)*csize);
    //memset(rcvbuff, 0, sizeof(int)*csize);
    //int gdb=0;
    //while (gdb==0) {;}


    //FF_CHECK_FAIL(sched = ff_oneside_allgather(&sndbuff, rcvbuff, 1, sizeof(int), 1));
    //here we need a container!!!!!
    //void * bufarr[2];
    //bufarr[0] = (void*) &sndbuff;
    //bufarr[1] = (void*) rcvbuff;

    uint64_t * buffs[2];
    buffs[0] = &sndbuff;
    buffs[1] = &rcvbuff;
    ff_sched_info info;
    info.count=1;
    info.unitsize=sizeof(uint64_t);
    info.tag = 1;
    info.userdata = (void *) buffs;
    info._operator = FF_MIN;
    info.datatype = FF_UINT64_T;

    FF_CHECK_FAIL(container = ff_container_create(info, create_singleactivated_allreduce));

    //sched = create_singleactivated_allgather(info);

    //ff_schedule_h sched2= create_singleactivated_allgather(info);


    int adegree=2;
    for (int i=0; i<adegree; i++) ff_container_increase_async(container);
    FF_CHECK_FAIL(ff_container_start(container));


    //ff_schedule_post(sched, 0);
    //ff_schedule_post(sched2, 0);

    srand(time(NULL)*rank);

    char strbuff[2048];

    int k = 1;
    int n = k*adegree;

    int l = sprintf(strbuff, "[Rank %i]\n", rank);

    for (int i=1; i<=n; i++){
        int f = (rank % 2 == 0) ? 100 : 1000;
        usleep(50 + (rand() % f));

        //sched = ff_container_get_next(container);

        /*if (ff_container_trylock(container)){
            sndbuff = i+1;
            ff_container_unlock(container);
        }else{
            printf("[Rank %i] ### schedule lock failed ###\n", rank);
        }*/
        sndbuff=i;
        ///ff_barrier();
        if ((i%k==0)){
            //if (i==n-1) sched=sched2;
            sched = ff_container_get_next(container);
            
            if (sched!=FF_FAIL){
                
                ff_schedule_satisfy_user_dep(sched);
                //ff_schedule_start(sched);

                printf("[Rank %i] waiting schedule\n", rank);
                //l += sprintf(strbuff + l, "\ti: %i; ", i);
                ff_schedule_wait(sched);
                //if (i==n-1) sleep(1);
                //ff_schedule_get_buffers(sched, NULL, (void **) &rcvbuff);
                printf("[Rank %i] i: %i;  sched: %i; rcvbuff: %i\n", rank, i, sched, rcvbuff);


               
                //printf("i: %i; %s\n", i, strbuff);
            }else{
                printf("[Rank %i] EMPTY CONTAINER!!!\n", rank);
            }

                   
        }

        int fs = ff_container_flush(container);
        if (fs>0) printf("flushed: %i\n", fs);
        for (int r=0; r<fs; r++) ff_container_increase_async(container);
        
    }


    MPI_Request req;
    MPI_Status status;
    int test = 0;
    MPI_Ibarrier(MPI_COMM_WORLD, &req);
    int mytest = 1;
    while (!test){
        int fs = ff_container_flush(container);
        if (fs>0) printf("termination. flushed: %i\n", fs);
        for (int r=0; r<fs; r++) ff_container_increase_async(container);
        MPI_Test(&req, &test, &status);
    }

    printf("%s", strbuff);
    printf("Completed\n");
    ff_barrier();
    ff_finalize();
    MPI_Finalize();
}
