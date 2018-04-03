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


int tcount = 0;
ff_schedule_h create_singleactivated_gather(ff_sched_info info){

    int n = ff_get_size();
    int * sndbuff = ((int **) info.userdata)[0];
    int * recbuff = ((int **) info.userdata)[1];
    //printf("[Rank %i] created: %p; size: %i; tag: %i\n", ff_get_rank(), recbuff, info.unitsize*info.count*n, info.tag + opcount);
    //for (int i=0; i<info.count*n; i++) recbuff[i]=100-opcount;

    return ff_solo_gather(sndbuff, recbuff, info.count, info.unitsize, info.root, info.tag + tcount++);
}



int main(int argc, char ** argv){

    ff_init(&argc, &argv);

    int root=0;
    //int gdb=0;
    //while (gdb==0) {;}
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    ff_container_h container;
    ff_schedule_h sched;

    int count=10;
    int * sndbuff = (int *) malloc(sizeof(int)*count);
    int * rcvbuff = NULL;
    if (rank==root) rcvbuff = (int *) malloc(sizeof(int)*csize*count);


    int * buffs[2];
    buffs[0] = sndbuff;
    buffs[1] = rcvbuff;
    ff_sched_info info;
    info.count=count;
    info.unitsize=sizeof(int);
    info.tag = 1;
    info.userdata = (void *) buffs;
    info.root=root;


    FF_CHECK_FAIL(container = ff_container_create(info, create_singleactivated_gather));

    int adegree=10;
    //if (rank==root) adegree=1;
    for (int i=0; i<adegree; i++) ff_container_increase_async(container);
    FF_CHECK_FAIL(ff_container_start(container));


    srand(time(NULL)*rank);



    int k = 500;
    int n = k*adegree*1;


    for (int i=1; i<=n; i++){
        int f = (rank % 2 == 0) ? 100 : 1000;
        if (root!=rank) usleep(50 + (rand() % f));

        for (int j=0; j<count; j++) sndbuff[j] = i*(j+1);

        if (rank==root && (i%2==0)){
            sched = ff_container_get_next(container);
            printf("root i: %i; n: %i;\n", i, n);
            if (sched!=FF_FAIL){

                ff_schedule_satisfy_user_dep(sched);
                //ff_schedule_start(sched);

                printf("[Rank %i] waiting schedule\n", rank);
                ff_schedule_wait(sched);
                printf("[Rank %i] i: %i;  sched: %i; rcvbuff: ", rank, i, sched);

                for (int j=0; j<csize*count; j++){
                    printf("%i ", rcvbuff[j]);
                }
                printf("\n");


                //ff_container_increase_async(container);
            }else{
                printf("[Rank %i] EMPTY CONTAINER!!!\n", rank);
            }
        }

    
        int c = ff_container_flush(container);
        if (c>0) printf("[Rank %i] flushed: %i\n", rank, c);
        for (int j=0; j<c; j++) ff_container_increase_async(container);
        
    }


    //FF_QUICK(ff_barrier(0));


    printf("[Rank %i] Entering barrier\n", rank);
    ff_schedule_h barrier = ff_barrier(0);
    ff_schedule_post(barrier, 1);

    while (ff_schedule_test(barrier)!=FF_SUCCESS){
        //ff_eq_poll();
        int c = ff_container_flush(container);
        if (c>0) printf("[Rank %i] termination flushing: %i\n", rank, c);
        for (int j=0; j<c; j++) ff_container_increase_async(container);
    }

    printf("[Rank %i] Completed\n", rank);
    
    ff_finalize();
}


/*
    ff_init();

    int root=0;
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();
    int * data;
    ff_schedule_h sched;
    if (rank==root){
        data = (int *) malloc(sizeof(int)*(csize));
        for (int i=0; i<csize; i++) data[i] = 0;
        //sched = gather_binomial_tree(root, rank, csize, NULL, 0, data, sizeof(int)*(csize), sizeof(int), 1);
        sched = ff_oneside_gather(root, NULL, data, sizeof(int), 1, 1);
    }else{
        data = (int *) malloc(sizeof(int));
        *data = rank;
        //sched = gather_binomial_tree(root, rank, csize, &data, sizeof(int), NULL, 0, sizeof(int), 1);
        sched = ff_oneside_gather(root, data, NULL, sizeof(int), 1, 1);
    }



    ff_schedule_post(sched, 1);
    ff_schedule_wait(sched);

    if (rank==root){
        printf("[Rank %i] recv:", (int) rank);
        for (int i=0; i<csize; i++){
            printf(" %i", data[i]);
        }
        printf("\n");
    }

    ff_finalize();
    free(data);

*/
