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
    int gdb=0;
    //while (gdb==0) {;}


    if (argc!=3) {
        printf("Usage: %s <count> <iterations>\n", argv[0]);
        exit(-1);
    }

    ff_init(&argc, &argv);

    int root=0;
    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();
    int * data;

    int count = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    ff_schedule_h sched;

    int *sndbuff=NULL, *rcvbuff=NULL;
    int error=0, gerror;

    if (rank==root){
        rcvbuff = (int *) malloc(sizeof(int)*csize*count);
    }else{
        sndbuff = (int *) malloc(sizeof(int)*count);
        for (int i=0; i<count; i++) sndbuff[i] = i*rank;
    }

    for (int it=0; it<iterations; it++){
        if (rank==root) memset(rcvbuff, 0, csize*count);
        sched = ff_gather(sndbuff, rcvbuff, count, sizeof(int), root, it+1);
        ff_schedule_post(sched, 1);
        ff_schedule_wait(sched);
        ff_schedule_free(sched);

        if (rank==root){
            for (int i=0; i<csize*count; i++){
                if (rcvbuff[i] != ((int) (i/count))*(i%count)){
                    printf("[Rank %i] Error: i: %i; expected: %i; received: %i\n", rank, i, ((int) (i/count))*(i%count), rcvbuff[i]);
                    error=1;
                }
            }
        }
    }

    FF_QUICK(ff_barrier(0));

    sched = ff_reduce(&error, &gerror, 1, root, 0, FF_MAX, FF_INT32_T);
    ff_schedule_post(sched, 1);
    ff_schedule_wait(sched);

    if (rank==root) {
        if (gerror) printf("Failed\n");
        else printf("Completed\n");
    }

    ff_finalize();
    free(data);
}

