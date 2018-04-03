#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define HRT_ARCH 2
#include "hrtimer/hrtimer.h"

//#define HRT
#include <mpi.h>

#ifdef BFF
#include "ff.h"
#include "ff_collectives.h"
#endif

#ifdef P4
#include "p4coll.h"
#endif

#define SEED1 123



//If you want the SOLO benchmark
//#define SOLO





#ifndef RUNS
#define RUNS 150
#endif

#ifndef WARMUP
#define WARMUP 20
#endif

#define ROOT 0
#define BROADCAST 1
#define SCATTER 2
#define ALLGATHER 3
#define FF_SOLO_SCATTER 7
#define FF_SOLO_BROADCAST 8
#define ISCATTER 9
#define IBROADCAST 10
#define IALLGATHER 11
#define ALLREDUCE 12
#define IALLREDUCE 13
//#define TAG 100


#ifdef HRT
#define TIMERINIT() HRT_INIT(0, g_timerfreq)
#define TIME(X) HRT_GET_TIMESTAMP((X));
#define USEC(T1,T2,T) { HRT_GET_ELAPSED_TICKS(T1, T2, &ticks); T = HRT_GET_USEC(ticks); }
#define bbtime HRT_TIMESTAMP_T
#else
#ifdef BFF
#define TIMERINIT() 
//MPI_Init(&argc, &argv)
#else
#define TIMERINIT() 
#endif
#define TIME(X) X = MPI_Wtime();
#define USEC(T1,T2,T) { T = (T2-T1)*1e6; }
#define bbtime double
#endif


#ifdef BMPI
#define BARRIER MPI_Barrier(MPI_COMM_WORLD);
#define GETRANK(R) MPI_Comm_rank(MPI_COMM_WORLD, R);
#define GETSIZE(R) MPI_Comm_size(MPI_COMM_WORLD, R);
#define REQ MPI_Request
#define NULLBUFF MPI_IN_PLACE
#else
//#define BARRIER ff_barrier();
#define BARRIER MPI_Barrier(MPI_COMM_WORLD);
#define GETRANK(R) *R=ff_get_rank();
#define GETSIZE(R) *R=ff_get_size();
#define REQ ff_schedule_h
#define NULLBUFF NULL
#endif

unsigned long long ticks;
unsigned long long g_timerfreq;

#if defined(SOLO) && defined(BFF)
ff_container_h container;
#endif

MPI_File createFilePerProcess(char* dir, char *prefixName) {
        MPI_File filehandle;
        char line[256];
        int ier;
        char fn[200];
        char timestr[25];
        //init output file
 
        time_t mytime;
        struct tm* tm_info;
        mytime = time(NULL);
        tm_info = localtime(&mytime);
        strftime(timestr, 25, "%Y-%m-%d_%H-%M", tm_info);
        snprintf(&fn[0], 100, "%s%s-%s.pid-%d.out\0",dir, prefixName, timestr, getpid());
        ier = MPI_File_open(MPI_COMM_SELF, &fn[0], MPI_MODE_EXCL + MPI_MODE_CREATE + MPI_MODE_WRONLY, MPI_INFO_NULL, &filehandle);
 
        if (ier != MPI_SUCCESS) {
                fprintf(stderr,"Error at open file %s for writing the timing values. The file probably already exists.\nWill now abort.\n", fn);
                MPI_Abort(MPI_COMM_WORLD, 1);
        }
        return filehandle;
}


double do_work(REQ sched, double duration){
    bbtime t1, start;
    double c = 0;
    double total=0;
    TIME(start);
    int term=0;
    //duration += duration*0.2;
    while (total<duration){
        c = c*2;
        TIME(t1);
        USEC(start, t1, total);
    }
    return total;
}

void * create_buffer(int size, int count){
    void * buff = malloc(size*count);
    if (buff==NULL){
        printf("ERROR: failed memory allocation\n");
        exit(1);
    }
    return buff;
}

double blocking_lat(int op, int size, int count, int tag, void ** sndbuff, void ** rcvbuff){

    //void * rcvbuff = NULL, * sndbuff = NULL;
    int rank, csize;
    GETRANK(&rank);
    GETSIZE(&csize);
   
    bbtime t1, t2;
    double lat;

#ifdef BMPI
    MPI_Request req;
    MPI_Status status;
#else
    ff_schedule_h sched;
#endif

    //printf("blocking_lat: sndbuff: %x; rcvbuff: %x\n", *sndbuff, *rcvbuff);
    switch(op){

        case BROADCAST:
#ifdef BFF
        case IBROADCAST:
#endif
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count);
            TIME(t1);
#ifdef BFF
            sched = ff_broadcast(*sndbuff, count, size, ROOT, tag);
            ff_schedule_post(sched, 1);
            ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Bcast(*sndbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD);
#endif
            TIME(t2);
            break;

        case SCATTER:
#ifdef BFF
        case ISCATTER:
#endif
            if (rank==ROOT) { 
                if(*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
                *rcvbuff = NULLBUFF;
            }else {
                //if (*tmp==NULL) *tmp = create_buffer(size, count*csize);   
                if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count);
                *sndbuff = NULL;

            }
            TIME(t1);
#ifdef BFF
            sched = ff_scatter(*sndbuff, *rcvbuff, count, size, ROOT, tag);
            ff_schedule_post(sched, 1);
            ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Scatter(*sndbuff, size*count, MPI_BYTE, *rcvbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD);
#endif
            TIME(t2);
            break;

        case ALLGATHER:
#ifdef BFF
        case IALLGATHER:
#endif
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count*csize);
            //else{printf("reusing buffer\n");}
            TIME(t1);
#ifdef BFF
            //printf("sched creation\n");
            sched = ff_allgather(*sndbuff, *rcvbuff, count, size, tag);
            //printf("sched posting\n");
            ff_schedule_post(sched, 1);
            //printf("sched waiting\n");
            ff_schedule_wait(sched);
            //printf("complete\n");
#elif defined(BMPI)
            MPI_Allgather(*sndbuff, count*size, MPI_BYTE, *rcvbuff, count*size, MPI_BYTE, MPI_COMM_WORLD);
#endif
            TIME(t2);
            break;


       case ALLREDUCE:
#ifdef BFF
       case IALLREDUCE:
#endif
            if (*sndbuff==NULL) *sndbuff = create_buffer(sizeof(int32_t), count);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(sizeof(int32_t), count);
            //if (*tmp==NULL) *tmp = create_buffer(sizeof(int32_t), count*csize);
            //printf("sndbuff: %x; rcvbuff: %x;\n", sndbuff, rcvbuff);
            TIME(t1);
#ifdef BFF
            sched = ff_allreduce(*sndbuff, *rcvbuff, count, tag, FF_SUM, FF_INT32_T);
            //ff_schedule_post(sched, 1);
            //ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Allreduce(*sndbuff, *rcvbuff, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
#endif
            TIME(t2);
            break;

#ifdef BMPI
        case IBROADCAST:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count);
            TIME(t1);
            MPI_Ibcast(*sndbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, &status);
            TIME(t2);
            break;

        case ISCATTER:
            if (rank==ROOT) {
                if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
                *rcvbuff = MPI_IN_PLACE;
            }else{
                //if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*log2(csize));   
                if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count);
            }
            TIME(t1);
            MPI_Iscatter(*sndbuff, size*count, MPI_BYTE, *rcvbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, &status);
            TIME(t2);
            break;

        case IALLGATHER:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
            if (*rcvbuff==NULL) *rcvbuff =create_buffer(size, count*csize);

            TIME(t1);
            MPI_Iallgather(*sndbuff, count*size, MPI_BYTE, *rcvbuff, count*size, MPI_BYTE, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, &status);
            TIME(t2);
            break;

        case IALLREDUCE:
            if (*sndbuff==NULL) *sndbuff = create_buffer(sizeof(int32_t), count);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(sizeof(int32_t), count);
            TIME(t1);
            MPI_Iallreduce(*sndbuff, *rcvbuff, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &req);
            MPI_Wait(&req, &status);
            TIME(t2);
            break;

#endif
        default: printf("ERROR: op not supported\n");

    }


    USEC(t1, t2, lat);

#ifdef BFF
    //printf("free\n");
    ff_schedule_free(sched);
#endif
    return lat;

}




void nonblocking_cc(int op, int size, int count, double duration, int tag, double * call_time, double * post_time, double * test_time, double * wait_time, void ** sndbuff, void ** rcvbuff){
    //void * rcvbuff = NULL, * sndbuff = NULL;
    int rank, csize;
    GETRANK(&rank);
    GETSIZE(&csize);

    int flag;
    double test_time_i;

    bbtime t1, t2, t3, t4;

#ifdef BMPI
    MPI_Request sched;
    MPI_Status status;
    *post_time = 0;
#else
    ff_schedule_h sched;
#endif    

    duration *= 3.5;
    //printf("op: %i; duration: %f\n", op, duration);
    switch(op){

    case IBROADCAST:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count);
#ifdef BFF
            TIME(t1)
            sched = ff_broadcast(*sndbuff, count, size, ROOT, tag);
            TIME(t2)
            USEC(t1, t2, *call_time);
            TIME(t1)
            ff_schedule_post(sched, 1);
            TIME(t2)
            USEC(t1, t2, *post_time);
#else
            TIME(t1)
            MPI_Ibcast(*sndbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
            TIME(t2)
            USEC(t1, t2, *call_time);
#endif           
            *test_time = do_work(sched, duration);
            TIME(t1)
#ifdef BFF
            ff_schedule_wait(sched);
#else
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, *wait_time);
            break;

    case ISCATTER:
            if (rank==ROOT) { 
                if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
                *rcvbuff = NULLBUFF;
            }else {
                //if (*tmp==NULL) *tmp = create_buffer(size, count*csize);   
                if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count);
            }
#ifdef BFF
            TIME(t1)
            sched = ff_scatter(*sndbuff, *rcvbuff, count, size, ROOT, tag);
            TIME(t2)
            USEC(t1, t2, *call_time);
            TIME(t1)
            ff_schedule_post(sched, 1);
            TIME(t2)
            USEC(t1, t2, *post_time);
#else
            TIME(t1)
            MPI_Iscatter(*sndbuff, size*count, MPI_BYTE, *rcvbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
            TIME(t2)
            USEC(t1, t2, *call_time);
#endif           
            *test_time = do_work(sched, duration);
            TIME(t1)
#ifdef BFF
            ff_schedule_wait(sched);
#else
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, *wait_time);
            break;



    case IALLGATHER:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count*csize);
            
#ifdef BFF
            TIME(t1)
            sched = ff_allgather(*sndbuff, *rcvbuff, count, size, tag);
            TIME(t2)
            USEC(t1, t2, *call_time);
            TIME(t1)
            ff_schedule_post(sched, 1);
            TIME(t2)
            USEC(t1, t2, *post_time);
#else
            TIME(t1)
            MPI_Iallgather(*sndbuff, count*size, MPI_BYTE, *rcvbuff, count*size, MPI_BYTE, MPI_COMM_WORLD, &sched);
            TIME(t2)
            USEC(t1, t2, *call_time);
#endif           
            *test_time = do_work(sched, duration);
            TIME(t1)
#ifdef BFF
            ff_schedule_wait(sched);
#else
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, *wait_time);
            break;

    case IALLREDUCE:
            if (*sndbuff==NULL) *sndbuff = create_buffer(sizeof(int32_t), count);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(sizeof(int32_t), count);
            //if (*tmp==NULL) *tmp = create_buffer(sizeof(int32_t), count*csize);
            TIME(t1);
#ifdef BFF
            TIME(t1)
            sched = ff_allreduce(*sndbuff, *rcvbuff, count, tag, FF_SUM, FF_INT32_T);
            TIME(t2)
            USEC(t1, t2, *call_time);
            TIME(t1)
            ff_schedule_post(sched, 1);
            TIME(t2)
            USEC(t1, t2, *post_time);
#else
            TIME(t1)
            MPI_Iallreduce(*sndbuff, *rcvbuff, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &sched);
            TIME(t2)
            USEC(t1, t2, *call_time);
#endif

            *test_time=0;
            do{           
                do_work(sched, duration);
                TIME(t1);
                MPI_Test(&sched, &flag, MPI_STATUS_IGNORE);  
                TIME(t2);
                USEC(t1, t2, test_time_i);
                *test_time += test_time_i;
            }while(!flag);



            TIME(t1)
#ifdef BFF
            ff_schedule_wait(sched);
#else
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, *wait_time);

            break;




    }
 //	if (sndbuff!=NULL) free(sndbuff);
    //if (rcvbuff!=NULL && rcvbuff!=MPI_IN_PLACE) free(rcvbuff);

        //printf("test time: %f; total_time: %f; duration: %f; overhead: %f\n", test_time, total_time, duration, overhead);
       
#ifdef BFF
    //printf("free\n");
    ff_schedule_free(sched);
#endif        



}

double nonblocking_ov(int op, int size, int count, double duration, int tag, void ** sndbuff, void ** rcvbuff){

    //void * rcvbuff = NULL, * sndbuff = NULL;
    int rank, csize;
    GETRANK(&rank);
    GETSIZE(&csize);

    double total_time, test_time=0, overhead;
    bbtime t1, t2, t3, t4;

    int flag;
    


#ifdef BMPI
    MPI_Request sched;
    MPI_Status status;
#else
    ff_schedule_h sched;
#endif    

    duration *= 3.5;
    //printf("op: %i; duration: %f\n", op, duration);
    switch(op){

        case IBROADCAST:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count);

            TIME(t1);
#ifdef BFF
            sched = ff_broadcast(*sndbuff, count, size, ROOT, tag);
            ff_schedule_post(sched, 1);
#else
            MPI_Ibcast(*sndbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
#endif
            test_time = do_work(sched, duration);
#ifdef BFF
            ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, total_time);
            break;

        case ISCATTER:
            if (rank==ROOT) { 
                if(*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
                *rcvbuff = NULLBUFF;
            }else{
                //if (*tmp==NULL) *tmp = create_buffer(size, count*csize);
                if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count);
            }
            TIME(t1);
#ifdef BFF
            sched = ff_scatter(*sndbuff, *rcvbuff, count, size, ROOT, tag);
            ff_schedule_post(sched, 1);
#elif defined(BMPI)
            MPI_Iscatter(*sndbuff, size*count, MPI_BYTE, *rcvbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
#endif
            test_time = do_work(sched, duration);
#ifdef BFF
            ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, total_time);
            break;


        case IALLGATHER:
            if (*sndbuff==NULL) *sndbuff = create_buffer(size, count*csize);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(size, count*csize);
            TIME(t1);
#ifdef BFF
            sched = ff_allgather(*sndbuff, *rcvbuff, count, size, tag);
            ff_schedule_post(sched, 1);
#else
            MPI_Iallgather(*sndbuff, count*size, MPI_BYTE, *rcvbuff, count*size, MPI_BYTE, MPI_COMM_WORLD, &sched);
#endif
            test_time = do_work(sched, duration);
#ifdef BFF
            ff_schedule_wait(sched);
#else
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, total_time);
            break;

        case IALLREDUCE:
            if (*sndbuff==NULL) *sndbuff = create_buffer(sizeof(int32_t), count);
            if (*rcvbuff==NULL) *rcvbuff = create_buffer(sizeof(int32_t), count);
            //if (*tmp==NULL) *tmp = create_buffer(sizeof(int32_t), count*csize);
            TIME(t1);
#ifdef BFF
            sched = ff_allreduce(*sndbuff, *rcvbuff, count, tag, FF_SUM, FF_INT32_T);
            ff_schedule_post(sched, 1);
#elif defined(BMPI)
            MPI_Iallreduce(*sndbuff, *rcvbuff, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &sched);
#endif


            do{           
                test_time += do_work(sched, duration);
                MPI_Test(&sched, &flag, MPI_STATUS_IGNORE);  
            }while(!flag);

#ifdef BFF
            ff_schedule_wait(sched);
#elif defined(BMPI)
            MPI_Wait(&sched, &status);
#endif
            TIME(t2);
            USEC(t1, t2, total_time);
    
            break;
        }

#ifdef BFF
        //printf("free\n");
        ff_schedule_free(sched);
#endif
        overhead = total_time - test_time;

        //printf("test time: %f; total_time: %f; duration: %f; overhead: %f\n", test_time, total_time, duration, overhead);
        return overhead;
}



#ifdef BFF


#ifdef SOLO
ff_schedule_h create_singleactivated_scatter(ff_sched_info info){
    int rank = ff_get_rank();
    int csize = ff_get_size();
   
    void * sndbuff = ((void **) info.userdata)[0];
    void * rcvbuff = ((void **) info.userdata)[1];
    void * tmpbuff = ((void **) info.userdata)[2];

    
    
    return ff_scatter(sndbuff, rcvbuff, info.count, info.unitsize, info.root, info.tag++);
}


ff_schedule_h create_singleactivated_broadcast(ff_sched_info info){
    int rank = ff_get_rank();
    void * sndbuff = ((void **) info.userdata)[0];
    return ff_broadcast(sndbuff, info.count, info.unitsize, info.root, info.tag++);
}

#endif

double solo_bench(ff_sched_info collinfo, int op, double duration){

//#ifdef BMPI
    //MPI_Request sched;
    //MPI_Status status;
//#else
    ff_schedule_h sched;
//#endif
    bbtime t1, t2;
    double total_time=0;
    double it_time=0;
    int rank;
    GETRANK(&rank);


#ifndef SOLO
    void * sndbuff = ((void **) collinfo.userdata)[0];
    void * rcvbuff = ((void **) collinfo.userdata)[1];
#endif    

    //double rnd = duration*(rand % 2.0);

    srand(SEED1 + rank*1000);
    double delayfactor = 1 + ((double)rand()/(double)RAND_MAX);
    //duration = (rank==ROOT) ? 0 : duration + 2*delayfactor*duration;
    duration = duration + 2*delayfactor*duration;

    BARRIER
    
    TIME(t1);
    for (int i=0; i<RUNS; i++){
        //printf("it it: %i\n", i);
        ////TIME(t1);
#ifndef SOLO
        if (op==IBROADCAST){
            //MPI_Ibcast(sndbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
            sched = ff_broadcast(sndbuff, collinfo.count, collinfo.unitsize, collinfo.root, collinfo.tag++);
        }else{
            //MPI_Iscatter(sndbuff, size*count, MPI_BYTE, rcvbuff, size*count, MPI_BYTE, ROOT, MPI_COMM_WORLD, &sched);
            sched = ff_scatter(sndbuff, rcvbuff, collinfo.count, collinfo.unitsize, collinfo.root, collinfo.tag++);            
        }

        ff_schedule_post(sched, 1);
#else
        if (rank==ROOT){
            sched = ff_container_get_next(container);
            ff_schedule_satisfy_user_dep(sched);
        }else{
            sched = ff_container_get_head(container);
        }
#endif

        do_work(sched, duration);

#ifndef SOLO
        ff_schedule_wait(sched);
#else
        ff_container_wait(container);
        ff_container_increase_async(container);
#endif
    
        ////TIME(t2);
        ////USEC(t1, t2, it_time);
        ////total_time += it_time;

        ff_schedule_free(sched);

    }
    //return RUNS*1000 / total_time;

    TIME(t2);
    USEC(t1, t2, total_time);
    return total_time;
}


#endif


double max(double a, double b){ return (a>b) ? a : b; }
int compare_entries(const void *a, const void *b){
      const double *da = (const double *) a;
      const double *db = (const double *) b;

      return (*da > *db) - (*da < *db);
}


int main(int argc, char ** argv){

    double vlat[RUNS];
    double voverhead[RUNS];

    double call_time[RUNS];
    double post_time[RUNS];
    double wait_time[RUNS];
    double test_time[RUNS];

#ifdef BMPI
    double vnbclat[RUNS];
#endif

#ifdef BFF
    double vsolo[RUNS];
    double _solo;
#endif

    if (argc!=4){
        printf("Usage: %s op size count\n", argv[0]);
        exit(1);
    }

    //Init the timer
    TIMERINIT();

    //Init MPI. I need it for the barrier and I/O. 
#ifdef BMPI
    MPI_Init(&argc, &argv);
#endif

#if defined(P4)
    p4_init();
#elif defined(BFF)
    //printf("aaaaaaaaaaaaaaa\n");
    ff_init(&argc, &argv);
#endif
    
    printf("Init ok\n");
    int rank, csize;
    GETRANK(&rank);
   
    GETSIZE(&csize);
    int op = atoi(argv[1]);
    int size = atoi(argv[2]);
    int count = atoi(argv[3]);

    double lat, ov, nbclat, maxlat=INT_MIN;
    int op1, op2;
   




    if (op==1) { op1=BROADCAST; op2=IBROADCAST; }
    else if (op==2) { op1=SCATTER; op2=ISCATTER; }
    else if (op==3) { op1=ALLGATHER; op2=IALLGATHER;}
    else if (op==4) { op1=ALLREDUCE; op2=IALLREDUCE;}

    int gdb=0;
    //while(gdb==0){;}
    double _test_time, _call_time, _wait_time, _post_time;



    void * dsndbuff = NULL; //create_buffer(size, count*csize);
    void * drcvbuff = NULL; //create_buffer(size, count*csize);
    


    BARRIER
    int tag=50;
    if (rank==ROOT) printf("Starting blocking\n");
    for (int i=0; i<RUNS + WARMUP; i++){
	    //printf("blocking: %i\n", i);

        lat = blocking_lat(op1, size, count, 0, &dsndbuff, &drcvbuff);
        BARRIER
#ifdef BMPI
        nbclat = blocking_lat(op2, size, count, 0, &dsndbuff, &drcvbuff);
        BARRIER
#endif

        if (i>=WARMUP) {
            vlat[i-WARMUP] = lat;
#ifdef BMPI
            vnbclat[i-WARMUP] = nbclat;
            maxlat = max(maxlat, nbclat);
#else
            maxlat = max(maxlat, lat);
#endif

        }
    }

    BARRIER
    if (rank==ROOT) printf("Starting call/post/wait bench\n");
    for (int i=0; i<RUNS + WARMUP; i++){
	    //printf("cpw it: %i\n", i);
        nonblocking_cc(op2, size, count, maxlat, 0, &_call_time, &_post_time, &_test_time, &_wait_time, &dsndbuff, &drcvbuff);
        BARRIER
        if (i>=WARMUP){
            call_time[i-WARMUP] = _call_time;
            post_time[i-WARMUP] = _post_time;
            wait_time[i-WARMUP] = _wait_time;
            test_time[i-WARMUP] = _test_time;
        }
    }
    BARRIER
    if (rank==ROOT) printf("Starting non-bloking overhead\n");

    /* NON BLOCKING OVERHEAD */
    for (int i=0; i<RUNS + WARMUP; i++){
	    //printf("nbc i: %i\n", i);
        ov = nonblocking_ov(op2, size, count, maxlat, 0, &dsndbuff, &drcvbuff);
        BARRIER
        if (i>=WARMUP) voverhead[i-WARMUP] = ov;
    }
    fflush(stdout);
    BARRIER
    if (rank==ROOT) printf("Non-blocking ok\n");


#ifdef BFF
    if (op2==IBROADCAST || op2==ISCATTER){

        ff_sched_info collinfo;
        collinfo.count=count;
        collinfo.unitsize=size;
        collinfo.tag = tag; 
        collinfo.root=0;
#ifdef SOLO
        int adegree=20;
#endif
        void * sbuffers[3];

        sbuffers[1] = malloc(size*count);
        if (op2==IBROADCAST){
            sbuffers[0] = malloc(size*count);
            sbuffers[2] = NULL;
        }else{
            sbuffers[0] = malloc(size*count*csize);
            sbuffers[2] = malloc(size*count*csize);
        }    
        collinfo.userdata = sbuffers;


    /*srand(SEED1 + rank*1000);
    double delayfactor = 1 + ((double)rand()/(double)RAND_MAX);
    double duration = (rank==ROOT) ? 0 : maxlat + 2*delayfactor*maxlat;
    printf("maxlat: %f; delay: %f; duration: %f\n", maxlat, delayfactor, duration);*/
#ifdef SOLO
        container = ff_container_create(collinfo, (op2==ISCATTER) ? create_singleactivated_scatter : create_singleactivated_broadcast);
        for (int i=0; i<adegree; i++) ff_container_increase_async(container);
        ff_container_start(container);
#endif

        for (int i=0; i<RUNS + WARMUP; i++){
            _solo = solo_bench(collinfo, op2, maxlat);
            if (i>=WARMUP) vsolo[i] = _solo;
        }



    }
#endif

    /* PRINT ON FILE */
    int pes;
    GETSIZE(&pes);
    char line[512];
    MPI_File filehandle = NULL;
    int k;
    double mflops;
    char* exec = "collbench";
    char* dir = "results/";
#ifdef BMPI
#ifdef P4
    char * type = "P4MPI";
    char * ovstr = "P4MPIOV";
    char * nbcstr = "P4MPINBC";
    char * prefix = "P4";
#else
    char* type="MPI";
    char * ovstr = "MPIOV";
    char * nbcstr = "MPINBC";
    char * prefix = "MPI";
#endif
#else
    char* type="FFLIB";
    char * ovstr = "FFOV";
    char * prefix = "FF";
#endif
    snprintf(&line[0], 100, "%s_OP_%d_PEs_%d_%d_iter_%d_size_%d_count_%d", type, op, pes, rank, RUNS, size, count);
    
    filehandle = createFilePerProcess(dir, &line[0]);
    //snprintf(line, 512, "%s\t%s\t%s\t%s\t%s\t%s\n", "PEs", "LATFF", "LATMPI", "NBCMPILAT", "OVFF", "OVMPI");
    //MPI_File_write(filehandle, line, strlen(line), MPI_CHAR, MPI_STATUS_IGNORE);
 
    if (op==4) size=sizeof(int32_t);

    for (k = 0; k < RUNS; k++) {
        //PEs TYPE VALUE RANK OP RUNS 
        snprintf(&line[0], 512, "%i\t%i\t%d\t%s\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, type, vlat[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);

#ifdef BMPI
        snprintf(&line[0], 512, "%i\t%i\t%d\t%s\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, nbcstr, vnbclat[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);
#endif
        snprintf(&line[0], 512, "%i\t%i\t%d\t%s\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, ovstr, voverhead[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);

	    snprintf(&line[0], 512, "%i\t%i\t%d\t%sCALL\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, prefix, call_time[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);

	    snprintf(&line[0], 512, "%i\t%i\t%d\t%sPOST\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, prefix, post_time[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);

	    snprintf(&line[0], 512, "%i\t%i\t%d\t%sWAIT\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, prefix, wait_time[k], op, RUNS, size, count);
        MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);

#ifdef BFF
        if (op2==IBROADCAST || op2==ISCATTER){
	        snprintf(&line[0], 512, "%i\t%i\t%d\t%sSOLO\t%f\t%d\t%d\t%d\t%d\n", k, rank, pes, prefix, vsolo[k], op, RUNS, size, count);
            MPI_File_write(filehandle, &line[0], strlen(&line[0]), MPI_CHAR, MPI_STATUS_IGNORE);
        }
#endif
        
    }
    MPI_File_close(&filehandle);


    //compute local mean for overhead:
    double ovmean, ovmedian;
    for (int i=0; i<RUNS; i++){
        ovmean += voverhead[i];
    }
    ovmean /= RUNS;

    qsort(voverhead, RUNS, sizeof(double), compare_entries);
    ovmedian = voverhead[RUNS/2];
    

    void * sndbuff = (rank==0) ? MPI_IN_PLACE : vlat;
    MPI_Reduce(sndbuff, vlat, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank!=0) sndbuff=voverhead;
    MPI_Reduce(sndbuff, voverhead, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank!=0) sndbuff=&ovmedian;
    MPI_Reduce(sndbuff, &ovmedian, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);


//call,post,test,wait times
    if (rank!=0) sndbuff=call_time;
    MPI_Reduce(sndbuff, call_time, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank!=0) sndbuff=wait_time;
    MPI_Reduce(sndbuff, wait_time, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank!=0) sndbuff=test_time;
    MPI_Reduce(sndbuff, test_time, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank!=0) sndbuff=post_time;
    MPI_Reduce(sndbuff, post_time, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
#ifdef BFF
    if (op2==IBROADCAST || op2==ISCATTER){
        if (rank!=0) sndbuff=vsolo;
        MPI_Reduce(sndbuff, vsolo, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    }
#endif

#ifdef BMPI
    if (rank!=0) sndbuff=vnbclat;
    MPI_Reduce(sndbuff, vnbclat, RUNS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
#endif

    if (rank!=0) sndbuff=&ovmean;
    MPI_Reduce(sndbuff, &ovmean, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    
    if (rank==0){
        qsort(vlat, RUNS, sizeof(double), compare_entries);
        qsort(voverhead, RUNS, sizeof(double), compare_entries);
        qsort(call_time, RUNS, sizeof(double), compare_entries);
        qsort(test_time, RUNS, sizeof(double), compare_entries);
        qsort(wait_time, RUNS, sizeof(double), compare_entries);
        qsort(post_time, RUNS, sizeof(double), compare_entries);
        printf("latency: %f; call_time: %f; post_time: %f; wait_time: %f; test_time: %f ", vlat[RUNS/2], call_time[RUNS/2], post_time[RUNS/2], wait_time[RUNS/2], test_time[RUNS/2]);

#ifdef BMPI
        qsort(vnbclat, RUNS, sizeof(double), compare_entries);
        printf("nbclat: %f; ", vnbclat[RUNS/2]);
#endif

#ifdef BFF
        if (op2==IBROADCAST || op2==ISCATTER){
            qsort(vsolo, RUNS, sizeof(double), compare_entries);
            printf("solo: %f; ", vsolo[RUNS/2]);
        }
#endif
        printf("overhead: max/median: %f; mean/mean: %f; median/mean: %f\n", voverhead[RUNS/2], ovmean/pes, ovmedian/pes);
    }


#ifdef BFF
    ff_finalize();
#else
    MPI_Finalize();
#endif

    return 0;
}


