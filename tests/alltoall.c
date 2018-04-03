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



void printarr(int * data, int csize, int count, int rank, int it){
    char strres[1024];
    int strc = sprintf(strres, "[Rank %i] it: %i; received: ", rank, it);
    for (int i=0; i<csize*count; i++){
        if (i%count==0) strc += sprintf(strres+strc, "| ");
        strc += sprintf(strres+strc, "%i ", data[i]);
    }
    printf("%s\n", strres);
}

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

    int * data = malloc(sizeof(int)*count*csize);
    for (int i=0; i<count*csize; i++) data[i] = i*rank;

    
    printarr(data, csize, count, rank, -1);
    int * recv = (int *) malloc(sizeof(int)*count*csize);

    for (int it=0; it<iterations; it++){
        memset(recv, 0, sizeof(int)*count*csize);
        
        //printf("[Rank %i] sending (%i, %i)\n", rank, data[0], data[1]);

        ff_schedule_h sched = ff_alltoall(data, recv, count, sizeof(int), it+1);

        ff_schedule_post(sched, 1);
        ff_schedule_wait(sched);
        ff_schedule_free(sched);
    
        printarr(recv, csize, count, rank, it); 
            
        int l1 = rank*count;
        for (int i=0; i<csize; i++){
            for (int j=0; j<count; j++){
                if (recv[i*count+j] != (l1+j)*i){
                    printf("[Rank %i] Error: i: %i; j: %i; expected: %i; received: %i; count: %i\n", rank, i, j, (l1+j)*i, recv[i*count+j], count);
                    error=1;
                }
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
    return 0;
}

