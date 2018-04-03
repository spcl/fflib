/*
 * Copyright (c) 2006 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 * Copyright (c) 2006 The Technical University of Chemnitz. All
 *                    rights reserved.
 *
 * Authors:
 *    Peter Gottschling <pgottsch@osl.iu.edu>
 *    Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#include <fftw3.h>
#include <string.h>
#include <stdlib.h>
/* stupid MPICH2 hack macro */
#define MPICH_IGNORE_CXX_SEEK 
#include <mpi.h>
//#include <nbc.h>
#include <math.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include "communication_patterns.hpp"

extern "C"{
#include "ff.h"
#include "ff_collectives.h"
#include "liblsb.h"
}

/* the timer array components :) */
#define ALL 0
#define INIT 1
#define WAIT 2
#define TEST 3
#define PACK 4
#define FFT 5
/* has to be at the end ... indicates size */
#define GES 6

#define TTOTAL 0
#define TCALL 1
#define TTEST 2
#define TWAIT 3

/* restore inline behavior for non-gcc compilers */
#ifndef __GNUC__
#define __inline__ inline 
#endif

#define WARMUP 10

const char * comm_type_str[] = {"FFT_MPI", "FFT_NBC", "FFT_OFF"};
const char * comm_ptrn_str[] = {"FFT_NORMAL", "FFT_PIPE", "FFT_TILE", "FFT_WIN", "FFT_WINTILE"};

class comm_t {
  public:
    typedef enum {FFT_MPI=0, FFT_NBC, FFT_OFF} comm_type; 
    typedef enum {FFT_NORMAL=0, FFT_PIPE, FFT_TILE, FFT_WIN, FFT_WINTILE} comm_ptrn;

    comm_type nbc;
    comm_ptrn trans;
    int max_n;          // max # of elements in distributed dimension
    int low;
    int high;
    int my_n;           // my # of elements in distributed dimension
    int p;
    int rank;
    int check;
    int N;                // elements per dimension
    int testint;
    int testsize;
    int reps;
    int nbplanes;    
    int last_comp_z; 
    int last_comm_z;

    comm_t (int argc, char **argv) : last_comp_z(0), last_comm_z(0) {
      char *optchars = "cn:t:r:p:a:b:", c;
      extern int optind, opterr, optopt;
      extern char *optarg;
  
      MPI_Comm_size(MPI_COMM_WORLD, &p);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);

      /* default input parameters */
      N = 320;
      check = 0;
      testsize = 0;
      reps = 1;
      nbplanes = 1; /* communicate so many planes non-blocking */


      nbc = comm_t::FFT_MPI; /* communication mode */
      trans = comm_t::FFT_NORMAL; /* communication pattern */
      /* get user options */
      while ( (c = getopt(argc, argv, optchars)) >= 0 ) {
        switch (c) {
          case '?': /* unrecognized or badly used option */
           if (!strchr(optchars, optopt))
             continue; /* unrecognized option */
           printf("Option %c requires an argument", optopt);
          case 'n': /* N */
            N = atoi(optarg);
            break;
          case 'c': /* check */
            check = 1;
            break;
          case 't': /* testsize - test each <n> bytes */
            testsize = atoi(optarg);
            break;
          case 'r': /* repetitions */
            reps = atoi(optarg);
            break;
          case 'p': /* non-blocking planes */
            nbplanes = atoi(optarg);
            break;
          case 'a':
            nbc = (comm_t::comm_type) atoi(optarg);
            break;
          case 'b':
            trans = (comm_t::comm_ptrn) atoi(optarg);
            break;
        }
      }

      /* input error checking */
      if(N % p != 0 ) {
        if(!rank) printf("N (=%i) must be divisible by p (=%i) - aborting\n", N, p);
        //MPI_Finalize();
        ff_finalize();
        exit(1);
      }
      
      /* input error checking */
      if(N % nbplanes != 0 ) {
        if(!rank) printf("N (=%i) must be divisible by nbplanes (=%i) - aborting\n", N, nbplanes);
        //MPI_Finalize();
        ff_finalize();
        exit(1);
      }
      
      if(N > 32736) { 
        printf("more than 32736 requests -> aborting\n"); 
        //MPI_Finalize(); 
        ff_finalize();
        exit(1); 
      }

      /* calculate distribution parameters */
      max_n = (N + p - 1) / p;
      low = rank*max_n;
      high = (rank+1)*max_n;
      if(high > N) high=N;
      my_n = high-low;

      /* testint defines the interval relative to the number of z-planes (N)
       * to test that we have a test for each 2kb */
      if(testsize)
        testint = N/(2*8*max_n*max_n/testsize+1) + 1;
      else
        testint = 0;

      if(rank == 0) {
        printf("%i repetitions of N=%i, testsize: %i, testint %i, tests: %i, max_n: %i \n", reps, N, testsize, testint, testint?N/testint:0, max_n);
        printf("approx. size: %f MB\n", (float)N*(float)N*(float)N/(float)p*(float)sizeof(double)/1024.0/1024.0 * 2.0 /* complex */ * 2.0 /* in+out array */ * 2.0 /* a2ar+a2as */ );
      }
    }
  
};


class buffer_t
{
public:
  buffer_t(comm_t *comm, int tile = 1) : comm(comm), tile(tile), tile_phase(0) {
    int size= 2 * tile * comm->max_n * comm->max_n * comm->p;
    //a2as = new double[size];
    if(MPI_SUCCESS != MPI_Alloc_mem( size*sizeof(double), MPI_INFO_NULL, &a2as)) {
      printf("Error allocating memory");
    }
    std::fill(a2as, a2as + size, 0);
    //a2ar = new double[size];
    if(MPI_SUCCESS != MPI_Alloc_mem( size*sizeof(double), MPI_INFO_NULL, &a2ar)) {
      printf("Error allocating memory");
    }
    std::fill(a2ar, a2ar + size, 0);
    tag=0;
  }

  ~buffer_t() {
    free(a2as);
    free(a2ar);
  }

  int tile_size() const {
    return tile;
  }

  void next_tile() {
    if (tile_phase >= tile) throw "Try to increase beyond last tile in buffer.\n"; 
    tile_phase++;
  }

  void reset_tile() {
    tile_phase= 0;
  }

  // pack plane z in into buffer for <tile_phase>'s part of buffer
  void pack(double *test, int z) {
    for(int y=0; y<comm->N; y++) {
      int index= (y * tile + tile_phase) * 2 * comm->max_n;
      for(int x=0; x<comm->my_n; x++) { 
  if(index > 2 * tile * comm->max_n * comm->N) std::cout << "index overflow!!!!" << std::endl;
	a2as[index++] = test[0 + 2 * (y + comm->N * (z + comm->N * x))];
	a2as[index++] = test[1 + 2 * (y + comm->N * (z + comm->N * x))]; } }
  }

  // unpacks buffer into z planes from <from_z> to <from_z>+tile-1
  void unpack(double *out, int from_z) {
    for (int proc= 0; proc < comm->p; proc++) {
      int proc_begin= comm->max_n * proc, proc_end= std::min(comm->max_n * (proc+1), comm->N); // fraction from proc
      for (int y= 0; y < comm->my_n; y++) 
	for (int z= from_z, in_tile= 0; in_tile < tile_phase; in_tile++, z++) {
	  int index= ((proc * comm->max_n + y) * tile + in_tile) * 2 * comm->max_n;
	  for(int x= proc_begin; x < proc_end; x++) {
	    out[0 + 2 * (x + comm->N * (z + comm->N * y))] = a2ar[index++];
	    out[1 + 2 * (x + comm->N * (z + comm->N * y))] = a2ar[index++]; } } }
  }

  double   *a2ar, *a2as;
  MPI_Request handle;
  ff_schedule_h sched;
  int tag;

private:
  comm_t   *comm;
  int      tile, tile_phase;
};
 

class communicate_t {
  public:
    communicate_t(comm_t *comm, double *out, double *t) : comm(comm), out(out), t(t) {
      index = 0;
    }

    ~communicate_t() {
    }

    void operator() (buffer_t* buffer) {
      int ops, counters;

      int size= 2 * buffer->tile_size() * comm->max_n * comm->max_n;
      // std::cout << size << " sent\n";
      LSB_Res(TCALL);
      if(comm->nbc == comm_t::FFT_NBC) {
        MPI_Ialltoall(buffer->a2as, size, MPI_DOUBLE, buffer->a2ar, size, MPI_DOUBLE, MPI_COMM_WORLD, &buffer->handle);
        //MPI_Alltoall(buffer->a2as, size, MPI_DOUBLE, buffer->a2ar, size, MPI_DOUBLE, MPI_COMM_WORLD);
      } else if (comm->nbc == comm_t::FFT_OFF){
        //printf("comm: FFT_OFF; size: %i\n", size);
        buffer->sched = ff_alltoall(buffer->a2as, buffer->a2ar, 1, sizeof(double), buffer->tag++);
        //printf("schedule posting\n");
        ff_schedule_post(buffer->sched, 1);
        //printf("getting schedule stat\n");
        ff_schedule_stat(buffer->sched, &ops, &counters);
        //printf("[Rank %i] ops: %i; counter: %i;\n", ff_get_rank(), ops, counters);     

      } else {
        MPI_Alltoall(buffer->a2as, size, MPI_DOUBLE, buffer->a2ar, size, MPI_DOUBLE, MPI_COMM_WORLD);
        /*NBC_Handle handle;
        NBC_Ialltoall(buffer->a2as, size, MPI_DOUBLE, buffer->a2ar, size, MPI_DOUBLE, MPI_COMM_WORLD, &handle);
        NBC_Wait(&handle);*/
      }
      LSB_Rec(TCALL, TCALL);
      index=0;
    }

  void wait(int from_z, buffer_t* buffer) {
    //std::cout << "waiting" << std::endl;
    LSB_Res(TWAIT);
    if(comm->nbc == comm_t::FFT_NBC) 
      MPI_Wait(&buffer->handle, MPI_STATUS_IGNORE);
    else if (comm->nbc == comm_t::FFT_OFF){
      ff_schedule_wait(buffer->sched);
      ff_schedule_free(buffer->sched);
    }
    LSB_Rec(TWAIT, TWAIT);
    buffer->unpack(out, from_z);
    buffer->reset_tile();
  }

  void test(buffer_t* buffer) {
    //std::cout << "testing" << std::endl;
    int flag;
    LSB_Res(TTEST);
    if(comm->nbc == comm_t::FFT_NBC) 
      MPI_Test(&buffer->handle, &flag, MPI_STATUS_IGNORE);
    else if (comm->nbc == comm_t::FFT_OFF)
      ff_schedule_test(buffer->sched);
    LSB_Rec(TTEST, TTEST);
  }
 
  private:
    comm_t *comm;
    double *out;
    int index;
    double *t;
};


class compute_t {
  public:
    compute_t (comm_t *comm, double *test) : comm(comm), test(test) {
    }
    
    void operator() (double *ptr) {     
#if 1
      fftw_plan p1d;
      p1d = fftw_plan_dft_1d(comm->N, (fftw_complex*)ptr, (fftw_complex*)ptr, FFTW_FORWARD, FFTW_ESTIMATE);
      fftw_execute(p1d);
      fftw_destroy_plan(p1d);
#endif
    }

    void operator() (int z, buffer_t* buffer) {
      for(int x=0; x<comm->my_n; x++) 
	      (*this)(test + (2 * comm->N * (z + comm->N * x))) ;
      comm->last_comp_z= z;
      buffer->pack(test, z);
      buffer->next_tile();
    }

  private:
    comm_t *comm; 
    double *test;
};

/* forward declaration */
static int fft_3d_noblock(double *out, double *temp, double *t, comm_t *comm);
static int fft_3d_block(double *out, double *temp, double *t, comm_t *comm);
static void do_check(double *out, double *in, double *test, double *t, comm_t *comm);
static void do_3dfft(comm_t& comm, double *test, double *out, double *t, int it);
  
/* transfers index from 3d to 1d array */
static __inline__ int ind(int x, int y, int z, int r, comm_t *comm) {

  return r + 2 * (z + comm->N * (y + comm->N * x));
}

static __inline__ void reset_timers(double *t) {
  int i;

  /* reset timers */
  for(i=0; i<GES; i++) t[i] = 0;
}

int main(int argc, char **argv) {
  double *in, *out, *test;
  int x,y,z,i=0;
  int check;
  double t[GES]; /* check timer array components ... */
        
  //MPI_Init(&argc, &argv);
  ff_init(&argc, &argv);

  LSB_Init("3dfft", 0);

  comm_t comm(argc, argv);

  /* malloc 3-D double array if size NxNxN */
  if(comm.check) {
    in = new double[comm.N*comm.N*comm.N*2];
    out = new double[comm.N*comm.N*comm.N*2];
    test = new double[comm.N*comm.N*comm.N*2];
  } else {
    out = new double[comm.N*comm.N*comm.N/comm.p*2];
    test = new double[comm.N*comm.N*comm.N/comm.p*2];
  }
  
  if(comm.check) {
    /* initialize input arrays */
    for(x=0; x<comm.N; x++) 
      for(y=0; y<comm.N; y++) 
        for(z=0; z<comm.N; z++) {
          out[ind(x,y,z,0,&comm)] = z+10*y+100*x;
          out[ind(x,y,z,1,&comm)] = z+10*y+100*x+1000;
        }
    
  } else {
    /* initialize input arrays */
    for(x=0; x<comm.max_n; x++) 
      for(y=0; y<comm.N; y++) 
        for(z=0; z<comm.N; z++) {
          out[ind(x,y,z,0,&comm)] = z+10*y+100*x;
          out[ind(x,y,z,1,&comm)] = z+10*y+100*x+1000;
        }
  }


  /* bcast input arrays */
  if(comm.check) {
    MPI_Bcast(out, comm.N*comm.N*comm.N*2, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    memcpy(in, out, 2*comm.N*comm.N*comm.N*sizeof(double));
    /* distribute out-array, because nodes expect their part at pos 0 */
    memcpy(out, in+comm.rank*2*comm.N*comm.N*comm.max_n, 2*comm.N*comm.N*comm.max_n*sizeof(double));
  }


  
  LSB_Set_Rparam_int("RANK", comm.rank);
  LSB_Set_Rparam_string("CTYPE", comm_type_str[comm.nbc]);
  LSB_Set_Rparam_string("CPTRN", comm_ptrn_str[comm.trans]);
  do_3dfft(comm, test, out, t, comm.reps);
  
  if (comm.rank==0){
    //printf("COMM_TYPE: %s; COMM_PTRN: %s; %lf - 1x%ibyte\n", comm_type_str[comm.nbc], comm_ptrn_str[comm.trans], t[ALL], 2*comm.max_n*comm.max_n*comm.N*8);
    if(comm.check) do_check(out, in, test, t, &comm);
  }
  if(comm.check) memcpy(out, in+comm.rank*2*comm.N*comm.N*comm.max_n, 2*comm.N*comm.N*comm.max_n*sizeof(double));

  

  if(comm.check) delete [] in; 
  delete [] out; 
  delete [] test;

  LSB_Finalize();
  //MPI_Finalize();
  ff_finalize();
}

static void do_3dfft(comm_t& comm, double *test, double *out, double *t, int r) {
  double *tmp, *ptr;
  int x,y,z,i=0;
  int index;
  int peer, tmplow, tmphigh;
  //NBC_Handle handle;
  compute_t     compute(&comm, test);

  reset_timers(t);

  int buff_count=1;
  int tile= 10;
  int win= 3;
  
  int buff_tile=comm.N;

  /**** ALLOCATE BUFFER ****/
  switch(comm.trans) {
    case comm_t::FFT_NORMAL: 
        buff_count=1;
        buff_tile=comm.N;
        break;
    case comm_t::FFT_PIPE: 
        buff_count=2;
        buff_tile=1;
        break;
    case comm_t::FFT_TILE: 
        buff_count=2;
        buff_tile=tile;
        break;
    case comm_t::FFT_WIN:
        buff_count=win+1;
        buff_tile=2;
        break;
    case comm_t::FFT_WINTILE:
        buff_count=win+1;
        buff_tile=tile;
        break;
  }


  std::vector<buffer_t*>  buffer_vector;
  for (int i= 0; i < buff_count; i++) { 
     buffer_t* single_buffer = new buffer_t(&comm, buff_tile); // with tiling 2
     buffer_vector.push_back(single_buffer);
  }

  /************ MEASUREMENT STARTS HERE *****************/
  //t[ALL] -= MPI_Wtime();

  for (int i=0; i<r + WARMUP; i++){

    LSB_Res(TTOTAL);


    /* transform all x-y-lines - parallel in x - array is contiguous in z-dir */

    for(x=0; x<comm.my_n; x++) 
      for(y=0; y<comm.N; y++) {
        ptr = out + (2 * comm.N * (y + comm.N * x));
        compute(ptr);
    }

    /* rearrange array to be contiguous in y-dir - rotate index xyz -> xzy */
    for(x=0; x<comm.my_n; x++) 
      for(z=0; z<comm.N; z++) {
        for(y=0; y<comm.N; y++) {
          test[0 + 2 * (y + comm.N * (z + comm.N * x))] = out[0 + 2 * (z + comm.N * (y + comm.N * x))];
          test[1 + 2 * (y + comm.N * (z + comm.N * x))] = out[1 + 2 * (z + comm.N * (y + comm.N * x))];
      } }
     

    // ##############################
    // Begining of sophisticated part
    // ##############################

    communicate_t communicate(&comm, out, t);

    switch(comm.trans) {
      case comm_t::FFT_NORMAL: 
        nbc::normal(comm.N, compute, communicate, buffer_vector[0]);
        break;

      case comm_t::FFT_PIPE: 
        nbc::pipeline(comm.N, compute, communicate, buffer_vector);
        break;

      case comm_t::FFT_TILE: 
        nbc::pipeline_tiled(comm.N, compute, communicate, buffer_vector);
        break;

      case comm_t::FFT_WIN: 
        nbc::pipeline_window(comm.N, compute, communicate, buffer_vector);
        break;
        
      case comm_t::FFT_WINTILE: 
        nbc::pipeline_tiled_window(comm.N, compute, communicate, buffer_vector);
        break;
    
       default:
        printf("No valid communication pattern specified. Aborting\n");
        exit(-1);
    }

    // #########################
    // End of sophisticated part
    // #########################


    /* transform all y-z-lines */
    for(y=0; y<comm.my_n; y++)  
      for(z=0; z<comm.N; z++) {
        ptr = out + (2 * comm.N * (z + comm.N * y));
        compute(ptr);
      }

    if (i>=WARMUP) LSB_Rec(TTOTAL, TTOTAL);
  } /*for*/
  
  /***************** MEASUREMENT ENDS HERE ******************/

  std::vector<buffer_t*>::iterator buffer_iter = buffer_vector.begin();
  while (buffer_iter != buffer_vector.end()) delete *buffer_iter++;

  if(comm.check) {
    /* gather distributed y-z lines - this makes problems for certain numbers of procs!!! */
    //MPI_Gather(out+(comm.rank*2*comm.max_n*comm.N*comm.N), 2*comm.max_n*comm.N*comm.N, MPI_DOUBLE, test, 2*comm.max_n*comm.N*comm.N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(out, 2*comm.max_n*comm.N*comm.N, MPI_DOUBLE, test, 2*comm.max_n*comm.N*comm.N, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  }
}

static void do_check(double *out, double *in, double *test, double *t, comm_t *comm) {
  double diff=0.0;
  fftw_plan p3d;
  int x,y,z;
  
  reset_timers(t);

  /* rearrange array to normal structure - rotate index yzx -> xyz */
  for(x=0; x<comm->N; x++) {
    for(z=0; z<comm->N; z++) {
      for(y=0; y<comm->N; y++) {
        out[0 + 2 * (z + comm->N * (y + comm->N * x))] = test[0 + 2 * (x + comm->N * (z + comm->N * y))];
        out[1 + 2 * (z + comm->N * (y + comm->N * x))] = test[1 + 2 * (x + comm->N * (z + comm->N * y))];
  } } }

  t[ALL] -= MPI_Wtime();
  /* plan a 3-d FFT to check result */
  p3d = fftw_plan_dft_3d(comm->N, comm->N, comm->N, (fftw_complex*)in, (fftw_complex*)test, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(p3d); 
  fftw_destroy_plan(p3d);
  t[ALL] += MPI_Wtime();
  diff = 0.0;
  
  for(x=0; x<comm->N; x++) 
    for(y=0; y<comm->N; y++) 
      for(z=0; z<comm->N; z++) {
        if(comm->N<=2) {
          printf("%lf - %lf : ", out[0 + 2 * (z + comm->N * (y + comm->N * x))], test[0 + 2 * (z + comm->N * (y + comm->N * x))]);
          printf("%lf - %lf\n", out[1 + 2 * (z + comm->N * (y + comm->N * x))], test[1 + 2 * (z + comm->N * (y + comm->N * x))]);
        }
        diff += fabs(out[0 + 2 * (z + comm->N * (y + comm->N * x))]-test[0 + 2 * (z + comm->N * (y + comm->N * x))]);
        diff += fabs(out[1 + 2 * (z + comm->N * (y + comm->N * x))]-test[1 + 2 * (z + comm->N * (y + comm->N * x))]);
      }
  printf("#### serial time: %lf - diff: %lf\n", t[ALL], diff);
}
