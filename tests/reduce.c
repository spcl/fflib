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



int main(int argc, char ** argv){

    ff_init(&argc, &argv);

    if (argc!=3) {
        printf("Usage: %s <count> <iterations>\n", argv[0]);
        exit(-1);
    }
      
    int root=0;
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    int count = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    int * data = malloc(sizeof(int)*count);
    for (int i=0; i<count; i++) data[i] = (i+1)*rank;

    int * recv = NULL;

    if (root==rank){
        recv = (int *) malloc(sizeof(int)*count);
        memset(recv, 0, sizeof(int)*count);
    }

    printf("[Rank %i] started\n", ff_get_rank());
   
    int error = 0;
    for (int i=0; i<iterations; i++){
        if (root==rank) memset(recv, 0, sizeof(int)*count);
        ff_schedule_h sched = ff_reduce(data, recv, count, root, i+1, FF_SUM, FF_INT32_T);
        ff_schedule_post(sched, 1);
        ff_schedule_wait(sched);
        ff_schedule_free(sched);

        if (root==rank) {
            int ranksum=(csize*(csize-1))/2; 
            for (int i=0; i<count; i++) { 
                if (recv[i]!=ranksum*(i+1)) {
                    printf("[Rank %i] error on field %i: expected: %i; received: %i\n", rank, i, ranksum*(i+1), recv[i]);
                    error=1;
                }
            }
        }
    }


    printf("[Rank %i] barrier\n", rank);
    FF_QUICK(ff_barrier(iterations+1));
    printf("[Rank %i] barrier OK\n", rank);

    ff_finalize();
}

