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

int main(int argc, char ** argv){


    if (argc!=3) {
        printf("Usage: %s <count> <iterations>\n", argv[0]);
        exit(-1);
    }


    ff_init(&argc, &argv);

    int root=0;
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    int count = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    int error=0, gerror;

    int * data = malloc(sizeof(int)*count);
    for (int i=0; i<count; i++) data[i] = (i+1)*(rank);

    int * recv = NULL;

    printf("[Rank %i] sending: %i\n", rank, *data);
    recv = (int *) malloc(sizeof(int)*count);
    int * tmp = (int *) malloc(sizeof(int)*count*csize);

    

    for (int it=0; it<iterations; it++){
        memset(recv, 0, sizeof(int)*count);


        ff_schedule_h sched = ff_allreduce(data, recv, count, it+1, FF_SUM, FF_INT32_T);

        ff_schedule_post(sched, 1);
        ff_schedule_wait(sched);
        ff_schedule_free(sched);

        int ranksum=(csize*(csize-1))/2; 
        for (int i=0; i<count; i++) { 
            if (recv[i]!=ranksum*(i+1)) {
                printf("[Rank %i] error on field %i: expected: %i; received: %i\n", rank, i, ranksum*(i+1), recv[i]);
                error=1;
            }
        }
    }


    FF_QUICK(ff_barrier(0));


    FF_QUICK(ff_reduce(&error, &gerror, 1, root, 0, FF_MAX, FF_INT32_T));

    if (rank==root) {
        if (gerror) printf("Failed\n");
        else printf("Completed\n");
    }

    ff_finalize();
}

