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
#define TAG 1

int main(int argc, char ** argv){

    if (argc!=2) {
        printf("Usage: %s<iterations>\n", argv[0]);
        exit(-1);
    }

    ff_init(&argc, &argv);
    
    int rank = ff_get_rank();
    int commsize = ff_get_size();

    int iterations = atoi(argv[1]);
    ff_schedule_h sched;

    for (int i=0; i<iterations; i++){
        
        printf("[Rank %i] enter\n", rank);
        //create the schedule. NB. here the schedule is not yet posted
        FF_CHECK_FAIL(sched = ff_barrier(TAG));
        //post the schedule
        ff_schedule_post(sched, 1);

        //wait for completion
        ff_schedule_wait(sched);
        printf("[Rank %i] exit\n", rank);
        ff_schedule_free(sched);
    }

    ff_finalize();
}

