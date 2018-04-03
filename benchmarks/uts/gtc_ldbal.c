#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <mpi.h>
#include <armci.h>

#include "uts_dm.h"
#include <tc.h>

#define SHRB_SIZE 1000000
#define STATS_TAG 12345

static StealStack ss;
static gtc_t gtc;

task_class_t uts_tclass;



// Utility functions
void ss_abort(int error) {
    MPI_Abort(MPI_COMM_WORLD, error);
}

char * ss_get_par_description() {
    return "Scioto Task Collection";
}

int ss_get_thread_num() {
    return gtc_mythread(gtc);
}

int ss_get_num_threads() {
    return gtc_nthreads(gtc);
}


void task_nop(gtc_t gtc, task_t *t) {
  // This implementation operates directly on the task descriptors, this
  // should never get called.
}


// Initialization and termination
StealStack* ss_init(int *argc, char ***argv) {
    MPI_Init(argc, argv);
    ARMCI_Init();

    uts_tclass = gtc_task_class_register(task_nop);
    gtc = gtc_create(sizeof(Node), 1, SHRB_SIZE, MPI_COMM_WORLD);
   
    ss.nNodes     = 0;
    ss.nLeaves    = 0;
    ss.nAcquire   = 0;
    ss.nRelease   = 0;
    ss.nSteal     = 0;
    ss.nFail      = 0;

    ss.maxStackDepth = 0;
    ss.maxTreeDepth  = 0;

    return &ss;
}

void ss_finalize() {
    gtc_destroy(gtc);
    
    ARMCI_Finalize();
    MPI_Finalize();
}

int ss_start(int work_size, int chunk_size) {
    tc_t *tc = gtc_lookup(gtc);

    ss.work_size  = work_size;
    ss.chunk_size = chunk_size;

    gtc_set_chunk_size(gtc, chunk_size);

    TC_START_TIMER(tc, tc_process);
    ss_setState(&ss, SS_WORK);

    return 1;
}

void ss_stop() {
    int processed, spawned;
    tc_t *tc = gtc_lookup(gtc);

    TC_STOP_TIMER(tc, tc_process);

    MPI_Reduce(&tc->tasks_completed, &processed, 1, MPI_INT, MPI_SUM, 0, tc->comm);
    MPI_Reduce(&tc->tasks_spawned, &spawned, 1, MPI_INT, MPI_SUM, 0, tc->comm);

    if (tc->mythread == 0) {
      printf("GTC internal check: %d tasks spawned, %d tasks processed, %s.\n", spawned, processed,
          (processed == spawned) ? "PASS" : "FAIL");
    }

    ss.nSteal   = tc->tasks_stolen;
    ss.nRelease = tc->tasks_stolen;
    //ss.nRelease = tc->shared_rb->nrelease;
    //ss.nReacquire = tc->shared_rb->nreacquire;
    if (verbose > 1)
        gtc_print_stats(gtc);
    return;
}


// Work movement functions
void ss_put_work(StealStack *s, void* t) {
    gtc_add(gtc, ss_get_thread_num(), t, 0, 0);
}


int ss_get_work(StealStack *s, void* work_buf) {
    int got_task;
    tc_t *tc = gtc_lookup(gtc);

    // If there's not much work available locally, try to expand more
    // around the top of the tree.
    //if (gtc_tasks_avail(tc) < 2*tc->chunk_size)
    //    got_task = gtc_get_buf(gtc, 1, work_buf);
    //else
        got_task = gtc_get_buf(gtc, 0, work_buf);

    if (!got_task)
        return STATUS_TERM;

    tc->tasks_completed++;
    s->nNodes++;

    return STATUS_HAVEWORK;
}


// Stats gathering functions
int ss_gather_stats(StealStack *s, int *count) {
    int i;
    MPI_Status status;

    *count = ss_get_num_threads();

    /* Gather stats onto thread 0 */
    if (ss_get_thread_num() > 0) {
        MPI_Send(&ss, sizeof(StealStack), MPI_BYTE, 0, STATS_TAG, MPI_COMM_WORLD);
        return 0;
    } else {
        memcpy(s, &ss, sizeof(StealStack));
        for(i = 1; i < *count; i++) {
            MPI_Recv(&(s[i]), sizeof(StealStack), MPI_BYTE, i, STATS_TAG, MPI_COMM_WORLD, &status);
        }
    }

    return 1;
}
