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

#define ROOT 0

int main(int argc, char ** argv){


    ff_init(&argc, &argv);

     
    if (argc!=3) {
        printf("Usage: %s <count> <iterations>\n", argv[0]);
        exit(-1);
    }

    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    int * data;
    int recv;
    ff_schedule_h sched;

    int error=0;

    int count = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    void *sndbuff, *rcvbuff; 
    
    if (rank==ROOT){
        data = (int *) malloc(sizeof(int)*csize*count);
        for (int i=0; i<csize*count; i++) {
            data[i] = i;
        }
        sndbuff=data;
        rcvbuff=NULL;
    }else{
        data = (int *) malloc(sizeof(int)*count);
        sndbuff=NULL;
        rcvbuff=data;
    }


    
    for (int i=0; i<iterations; i++){
    
        sched = ff_scatter(sndbuff, rcvbuff, count, sizeof(int), ROOT, 1);    

        ff_schedule_post(sched, 1);
        //printf("[Rank %i] waiting\n");
        ff_schedule_wait(sched);
        //printf("[Rank %i] waiting ok\n");

        ff_schedule_free(sched);
        if (rank!=ROOT){
            for (int j=0; j<count; j++){
                if (data[j]!=rank*count + j) {
                    printf("[Rank %i] Error: expected: %i; received: %i\n", rank, rank*count+j, data[j]); 
                    error=1;
                }
            }
        }
    }
    

    free(data);
    ff_barrier(0);

    int gerror=0;
    printf("[Rank %i] start reduce\n", rank);
    sched = ff_reduce(&error, &gerror, 1, ROOT, 2, FF_MAX, FF_INT32_T);
    ff_schedule_post(sched, 1);
    ff_schedule_wait(sched);
    printf("[Rank %i] reduce ok\n", rank);

    if (rank==ROOT){
        if (gerror) printf("Failed\n");
        else printf("Success\n");
    }   

    ff_finalize();
}
