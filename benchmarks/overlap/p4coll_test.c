#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include "p4coll.h"

#define RUNS 100

int main(int argc, char * argv[]){


    if (argc!=2){
        printf("Usage: %s <size>\n", argv[0]);
    }
    p4_init();

    int size = atoi(argv[1]);

    int rank = p4_get_rank();
    int csize = p4_get_size();

   
    void * sndbuff = malloc(size); 
    void * rcvbuff = malloc(size*csize);
    p4coll_info * info;

  
    for (int i=0; i<RUNS; i++){    
        info = p4_allgather(sndbuff, rcvbuff, 1, size, 2);
        p4_wait(info);
    }

    
/*
    char str[512];
    int p = sprintf(str, "[Rank %i] ", rank);
    for (int i=0; i<csize; i++){
        p += sprintf(str+p, "%i ", rcvbuff[i]);
    }
    str[p] = '\0';

    printf("%s\n", str);
*/
    return 0;
}
