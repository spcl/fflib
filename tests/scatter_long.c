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


    ff_init();


    int root=0;

    int gdb=0;
    //while (gdb==0){;}

    ff_peer rank = ff_get_rank();
    ff_size_t csize = ff_get_size();

    int count = 2048;
    int it=100;
    void * data=NULL;
    int recv;
    ff_schedule_h sched;
    int tag = 1;
    int msize = count*sizeof(char);

    printf("starting\n");
    for (int i=0; i<it; i++){
        if (rank==root) { 
            data = malloc(msize*csize);
            printf("data size on root: %i\n", msize*csize);
        }else{ data = malloc(msize);}
        if (rank==root){
            sched = ff_scatter3(data, NULL, 1, msize, root, tag++);
        }else{
            data = malloc(msize);
            sched = ff_scatter3(NULL, &data, 1, msize, root, tag++);
        }
        printf("sched: %i\n", sched);
    
        if (data==NULL) printf("memory error\n");

        ff_schedule_post(sched, 1);
        ff_schedule_wait(sched);
        printf("free sched: %i\n", sched);
        //ff_schedule_free(sched);
    }



    //scatter_binomial_tree(ff_peer root, ff_peer rank, ff_size_t comm_size, char * sndbuff, ff_size_t sndsize, char * rcvbuff, ff_size_t rcvsize, ff_size_t unitsize, ff_tag tag);
    ff_barrier();

    ff_finalize();
}
