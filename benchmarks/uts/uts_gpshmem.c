#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h> //#include <time.h> //sys/time.h for cray x1

#include "uts.h"

/* This is the preliminary GPShmem implementation.  So far, this is in a seperate file
 * because GPShmem requires that all symmetric objects be allocated explicitly.  Therefore,
 * global objects are not automatically symmetric under GPShmem!
 */
#include <gpshmem.h>
#define PARALLEL         1
#define COMPILER_TYPE    4
#define SHARED           
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAXTHREADS       64
#define LOCK_T           long
#define GET_NUM_THREADS  gpnumpes()
#define GET_THREAD_NUM   gpmype()
#define SET_LOCK_P(zlk)  gpshmem_set_lock(zlk, 0)
#define UNSET_LOCK_P(zlk) gpshmem_clear_lock(zlk, 0)
#define SET_LOCK(zlk)    gpshmem_set_lock(&(zlk), 0)
#define UNSET_LOCK(zlk)  gpshmem_clear_lock(&(zlk), 0)
//#define INIT_LOCK(zlk)   zlk = shmem_global_lock_alloc()
#define INIT_LOCK(zlk)   zlk = 0
//#define INIT_SINGLE_LOCK(zlk) zlk = shmem_global_lock_alloc()
#define BARRIER          gpshmem_barrier_all();

#define SMEMCPY(target,source,len,from_id)	gpshmem_getmem(target,source,len,from_id)
#define ALLOC					gpshmalloc
#define GET_P(target,source,from_id)		gpshmem_get(target,source,1,from_id)        
#define PUT_P(target,source,to_id)		gpshmem_put(target,source,1,to_id)
#define GET(target,source,from_id)		gpshmem_get(&(target),&(source),1,from_id)        
#define PUT(target,source,to_id)		gpshmem_put(&(target),&(source),1,to_id)

/*  These are debugging replacements for the above macros. */
/*
static void* ALLOC(int size)
{
  void *addr = gpshmalloc(size);
  printf("PE %d: Symmetric allocation %d bytes at 0x%x\n", GET_THREAD_NUM, size, addr);
  return addr;
}

#define SMEMCPY(target,source,len,from_id) \
	printf("PE %d: SMEMCPY(0x%x, 0x%x, %d, %d)\n", GET_THREAD_NUM, target, source, len, from_id); \
	gpshmem_getmem(target,source,len,from_id)

#define GET(target,source,from_id) \
	printf("PE %d: GET(0x%x, 0x%x, %d)\n", GET_THREAD_NUM, &(target), &(source), from_id); \
	gpshmem_get(&(target),&(source),1,from_id)

#define PUT(target,source,to_id) \
	printf("PE %d: GET(0x%x, 0x%x, %d)\n", GET_THREAD_NUM, &(target), &(source), to_id); \
	gpshmem_put(&(target),&(source),1,to_id)

//#define SET_LOCK(lock)    while (gpshmem_swap(&(lock), 1, 0)) gpshmem_wait(&(lock), 0)
//#define UNSET_LOCK(lock)  gpshmem_swap(&(lock), 0, 0)
*/

// Shmem helper function to match UPC lock allocation semantics
/*
long * shmem_global_lock_alloc() {    
    long *lock = (long *) shmalloc(sizeof(long));
    *lock = 0;
    return lock;
}
*/

#define ALLPUT(target,source,to_id)   gpshmem_put(target,source,1,to_id)
#define PUT_ALL(a,b)								\
	do {						 			\
		int _iter, _node; 						\
		for (_iter = 1; _iter < GET_NUM_THREADS; _iter++) {		\
			_node = (GET_THREAD_NUM + _iter) % GET_NUM_THREADS;	\
			ALLPUT((long*)a,(long*)b,_node);					\
		}								\
	} while(0)


/* Set a mutex lock.  PE is the id of the PE that owns that lock */
/* WARNING: Making either of these inline in gcc causes the program to deadlock. */
static void gpshmem_set_lock(LOCK_T *lock, int pe)
{
//	printf("gpshmem_set_lock(0x%x, %d)\n", lock, pe);

	if (pe == GET_THREAD_NUM) {
		// FIXME: We may not be able to do this ...
		while (gpshmem_swap(lock, 1, pe))
			gpshmem_wait(lock, 0);
	}
	else {
		while (gpshmem_swap(lock, 1, pe));
	}
}

/* Clear a mutex lock.  PE is the id of the PE that owns that lock */
static void gpshmem_clear_lock(LOCK_T *lock, int pe)
{
//	printf("gpshmem_clear_lock(0x%x, %d)\n", lock, pe);
	gpshmem_swap(lock, 0, pe);
}


/* Parallel execution parameters */
int doSteal   = 1;     // 1 => use work stealing
int chunkSize = 20;    // number of nodes to move to/from shared area
int cbint     = 1;     // Cancellable barrier polling interval


/***********************************************************
 *  StealStack types                                       *
 ***********************************************************/

#define MAXSTACKDEPTH 5000

/* stack of nodes */
struct stealStack_t
{
  int stackSize;     /* total space avail (in number of elements) */
  int workAvail;     /* elements available for stealing */
  int sharedStart;   /* index of start of shared portion of stack */
  int local;         /* index of start of local portion */
  int top;           /* index of stack top */
  int maxStackDepth;                      /* stack stats */ 
  int nNodes, maxTreeDepth;               /* tree stats  */
  int nAcquire, nRelease, nSteal, nFail;  /* steal stats */
  int idle_sessions;                      /* steal perf  */
  double work_time, search_time, idle_time; 
  double time; 
  LOCK_T * stackLock; /* lock for manipulation of shared portion */
  Node * stack;       /* addr of actual stack of nodes in local addr space */
  char str[1000];     /* holds text string for debugging info */
};
typedef struct stealStack_t StealStack;


/***********************************************************
 *                                                         *
 *  shared state                                           *
 *                                                         *
 ***********************************************************/

// shared access to each thread's stealStack
SHARED StealStack *stealStack;

// termination detection 
VOLATILE SHARED long *cb_cancel;
VOLATILE SHARED long *cb_count;
VOLATILE SHARED long *cb_done;
LOCK_T *cb_lock;


char * impl_getName() {
  char * name[] = {"Sequential C", "C/OpenMP", "UPC", "SHMEM", "PThreads"};
  return name[COMPILER_TYPE];
}


// construct string with all parameter settings 
int impl_paramsToStr(char *strBuf, int ind) {
  // search method
  ind += sprintf(strBuf+ind, "Execution strategy:  ");
  if (PARALLEL) {
    ind += sprintf(strBuf+ind, "Parallel search using %d threads\n", GET_NUM_THREADS);
    if (doSteal)
      ind += sprintf(strBuf+ind, "   Load balance by work stealing, chunk size = %d nodes\n",chunkSize);
    else
      ind += sprintf(strBuf+ind, "   No load balancing.\n");
  }
  else
    ind += sprintf(strBuf+ind, "Sequential search\n");
      
  ind += sprintf(strBuf+ind, "  CBarrier Interval: %d\n", cbint);

  return ind;
}


int impl_parseParam(char *param, char *value) {
  int err = 0;  // Return 0 on a match, nonzero on an error

  switch (param[1]) {
    case 'c':
      chunkSize = atoi(value); break;
    case 's':
      doSteal = atoi(value); 
      if (doSteal != 1 && doSteal != 0) 
	err = 1;
      break;
    case 'i':
      cbint = atoi(value); break;
    default:
      err = 1;
      break;
  }

  return err;
}

void impl_helpMessage() {
  printf("   -s  int   nonzero to enable work stealing\n");
  printf("   -c  int   chunksize for work sharing and work stealing\n");
  printf("   -i  int   set cbarrier polling interval\n");
}

/***********************************************************
 *                                                         *
 *  FUNCTIONS                                              *
 *                                                         *
 ***********************************************************/

/* 
 * StealStack
 *    Stack of nodes with sharing at the bottom of the stack
 *    and exclusive access at the top for the "owning" thread 
 *    which has affinity to the stack's address space.
 *
 *    * All operations on the shared portion of the stack
 *      must be guarded using the stack-specific lock.
 *    * Elements move between the shared and exclusive
 *      portion of the stack solely under control of the 
 *      owning thread. (ss_release and ss_acquire)
 *    * workAvail is the count of elements in the shared
 *      portion of the stack.  It may be read without 
 *      acquiring the stack lock, but of course its value
 *      may not be acurate.  Idle threads read workAvail in
 *      this speculative fashion to minimize overhead to 
 *      working threads.
 *    * Elements can be stolen from the bottom of the shared
 *      portion by non-owning threads.  The values are 
 *      reserved under lock by the stealing thread, and then 
 *      copied without use of the lock (currently space for
 *      reserved values is never reclaimed).
 *
 */

/* restore stack to empty state */
void ss_mkEmpty(StealStack *s) {
  s->sharedStart = 0;
  s->local  = 0;
  s->top    = 0;
  s->workAvail = 0;
}

/* fatal error */
void ss_error(char *str) {
  printf("*** [Thread %i] %s\n",GET_THREAD_NUM, str);
  exit(4);
}

/* initialize the stack */
void ss_init(StealStack *s, int nelts) {
  int nbytes = nelts * sizeof(Node);
  int iter;

  if (debug & 1)
    printf("Thread %d intializing stealStack %p, sizeof(Node) = %X\n", 
           GET_THREAD_NUM, s, sizeof(Node));

  // allocate stack in shared addr space with affinity to calling thread
  // and record local addr for efficient access in sequel
  s->stack = (Node *) ALLOC (nbytes);
  if (s->stack == NULL) {
    printf("Request for %d bytes for stealStack on thread %d failed\n",
           nbytes, GET_THREAD_NUM);
    ss_error("ss_init: unable to allocate space for nodes");
  }

  s->stackLock = (LOCK_T *) ALLOC (sizeof(LOCK_T) * GET_NUM_THREADS);

  for (iter = 0; iter < GET_NUM_THREADS; iter++) {
    INIT_LOCK(s->stackLock[iter]);
    if (debug & 1)
      printf("Thread %d init stackLock[%d] %p\n", GET_THREAD_NUM, iter, s->stackLock[iter]);
  }

  s->stackSize = nelts;
  s->nNodes = 0;
  s->maxStackDepth = 0;
  s->maxTreeDepth = 0;
  s->nAcquire = 0;
  s->nRelease = 0;
  s->nSteal = 0;
  s->nFail = 0;
  s->idle_sessions = 0;
  s->idle_time = s->work_time = s->search_time = 0.0;
  ss_mkEmpty(s);
}


/* local push */
void ss_push(StealStack *s, Node *c) {
  if (s->top >= s->stackSize)
    ss_error("ss_push: overflow");
  if (debug & 1)
    printf("ss_push: Thread %d, posn %d: node %s [%d]\n",
           GET_THREAD_NUM, s->top, rng_showstate(c->state.state, s->str), c->height);
  memcpy(&(s->stack[s->top]), c, sizeof(Node));
  s->top++;
  s->nNodes++;
  s->maxStackDepth = max(s->top, s->maxStackDepth);
  s->maxTreeDepth = max(s->maxTreeDepth, c->height);
}

/* local top:  get local addr of node at top */ 
Node * ss_top(StealStack *s) {
  Node *r;
  if (s->top <= s->local)
    ss_error("ss_top: empty local stack");
  r = &(s->stack[(s->top) - 1]);
  if (debug & 1) 
    printf("ss_top: Thread %d, posn %d: node %s [%d] nchild = %d\n",
           GET_THREAD_NUM, s->top - 1, rng_showstate(r->state.state, s->str),
           r->height, r->numChildren);
  return r;
}

/* local pop */
void ss_pop(StealStack *s) {
  Node *r;
  if (s->top <= s->local)
    ss_error("ss_pop: empty local stack");
  s->top--;
  r = &(s->stack[s->top]);
  if (debug & 1)
    printf("ss_pop: Thread %d, posn %d: node %s [%d] nchild = %d\n",
           GET_THREAD_NUM, s->top, rng_showstate(r->state.state, s->str), 
           r->height, r->numChildren);
}
  
/* local top position:  stack index of top element */
int ss_topPosn(StealStack *s)
{
  if (s->top <= s->local)
    ss_error("ss_topPosn: empty local stack");
  return s->top - 1;
}

/* local depth */
int ss_localDepth(StealStack *s) {
  return (s->top - s->local);
}

/* release k values from bottom of local stack */
void ss_release(StealStack *s, int k) {
  int i;

  //SET_LOCK(s->stackLock[GET_THREAD_NUM]);
  gpshmem_set_lock(&(s->stackLock[GET_THREAD_NUM]), GET_THREAD_NUM);
  if (s->top - s->local >= k) {
      s->local += k;
      s->workAvail += k;
      s->nRelease++;
  }
  else
    ss_error("ss_release:  do not have k vals to release");
  //UNSET_LOCK(s->stackLock[GET_THREAD_NUM]);
  gpshmem_clear_lock(&(s->stackLock[GET_THREAD_NUM]), GET_THREAD_NUM);
}

/* move k values from top of shared stack into local stack
 * return false if k vals are not avail on shared stack
 */
int ss_acquire(StealStack *s, int k) {
  int i;
  int avail;
  if (debug & 128) printf("Thread %d: Acquire Local Work\n", GET_THREAD_NUM);
  //SET_LOCK(s->stackLock[GET_THREAD_NUM]);
  gpshmem_set_lock(&(s->stackLock[GET_THREAD_NUM]), GET_THREAD_NUM);
  avail = s->local - s->sharedStart;
  if (avail >= k) {
      s->local -= k;
      s->workAvail -= k;
      s->nAcquire++;
  }
  //UNSET_LOCK(s->stackLock[GET_THREAD_NUM]);
  gpshmem_clear_lock(&(s->stackLock[GET_THREAD_NUM]), GET_THREAD_NUM);
  return (avail >= k);
}

/* steal k values from shared portion of victim thread's stealStack
 * onto local portion of current thread's stealStack.
 * return false if k vals are not avail in victim thread
 */
int ss_steal(StealStack *s, int victim, int k) {
  StealStack victimSS;
  int ok;
  
  if (s->sharedStart != s->top)
    ss_error("ss_steal: thief attempts to steal onto non-empty stack");

  if (s->top + k >= s->stackSize)
    ss_error("ss_steal: steal will overflow thief's stack");
  
  /* lock victim stack and try to reserve k elts */
  //SET_LOCK(stealStack->stackLock[victim]);
  gpshmem_set_lock(&(stealStack->stackLock[victim]), victim);
  SMEMCPY(&victimSS, stealStack, sizeof(StealStack), victim);  // Get remote steal stack
  if (victimSS.local - victimSS.sharedStart != victimSS.workAvail)
    ss_error("ss_steal: stealStack invariant violated");
  
  ok = victimSS.workAvail >= k;
  if (ok) {
    /* reserve a chunk */
      victimSS.sharedStart += k;
      victimSS.workAvail -= k;
      PUT(stealStack->sharedStart, victimSS.sharedStart, victim);
      PUT(stealStack->workAvail, victimSS.workAvail, victim);
  }
  //UNSET_LOCK(stealStack->stackLock[victim]);
  gpshmem_clear_lock(&(stealStack->stackLock[victim]), victim);
	
  /* if k elts reserved, move them to local portion of our stack */
  if (ok) {
    /* FIXME: You cannot do this in GPShmem - translation fails! */
    SHARED_INDEF Node * victimSharedStart;
    //victimSharedStart = victimSS.stack + victimSS.sharedStart - k;  // The start address of stealable work
    victimSharedStart = s->stack + victimSS.sharedStart - k;

    // Copy stealable work from victim to local stack
    SMEMCPY(&(s->stack[s->top]), victimSharedStart, k * sizeof(Node), victim);

    s->top += k;
    s->nSteal++;
  }
  else {
    s->nFail++;
    if (debug & 4) {
      printf("Thread %d failed to steal %d nodes from thread %d, ActAv = %d, sh = %d, loc =%d\n",
	     GET_THREAD_NUM, k, victim, victimSS.workAvail, victimSS.sharedStart, victimSS.local);
    }
  }
  
  return (ok);
} 

/* search other threads for work to steal */
int findwork(int k) {
  int i,v;
  int availWork;

  for (i = 1; i < GET_NUM_THREADS; i++) {
    v = (GET_THREAD_NUM + i) % GET_NUM_THREADS;
    if (debug & 128) printf("findwork(): PE %d looking for work on PE %d\n", GET_THREAD_NUM, v);
    GET(availWork, stealStack->workAvail, v);
    if (debug & 128) printf("findwork(): PE %d found %d nodes on PE %d\n", GET_THREAD_NUM, availWork, v);
    // The old way ... if (stealStack[v].workAvail >= k)
    if (availWork >= k)
      return v;
  }
  return -1;
}


void initNode(Node * child)
{
  child->type = -1;
  child->height = -1;
  child->numChildren = -1;    // not yet determined
}

// forward decl
void releaseNodes(StealStack *ss);

/* 
 * Generate all children of the parent
 *
 * details depend on tree type, node type and shape function
 */
void genChildren(Node * parent, Node * child, StealStack * ss) {
  int parentHeight = parent->height;
  int numChildren, childType;

  numChildren = uts_numChildren(parent);
  childType   = uts_childType(parent);

  // record number of children in parent
  parent->numChildren = numChildren;
  if (debug & 2) {
    char debug_str[256];
    printf("Gen:  Thread %d, posn %2d: node %s [%d] has %2d children\n",
           GET_THREAD_NUM, ss_topPosn(ss),
           rng_showstate(parent->state.state, &debug_str), 
           parentHeight, numChildren);
  }
  
  // construct children and push onto stack
  if (numChildren > 0) {
    int i, j;
    child->type = childType;
    child->height = parentHeight + 1;

    for (i = 0; i < numChildren; i++) {
      for (j = 0; j < computeGranularity; j++) {
        // TBD:  add parent height to spawn
        // computeGranularity controls number of rng_spawn calls per node
        rng_spawn(parent->state.state, child->state.state, i);
      }
      ss_push(ss, child);
      releaseNodes(ss);
    }
  }
}

double time_split(double * time) {
  double t_now = uts_wctime();
  double elapsed_time = t_now - *time;
  *time = t_now;
  return elapsed_time;
}

    
/*
 *  Parallel tree traversal
 *
 */

/* cancellable barrier */

/* initialize lock:  single thread under omp, all threads under upc */
void cb_init(){
  // INIT_SINGLE_LOCK(cb_lock);
  cb_lock = ALLOC(sizeof(LOCK_T));
  cb_cancel = ALLOC(sizeof(long));
  cb_count = ALLOC(sizeof(long));
  cb_done = ALLOC(sizeof(long));

  //INIT_LOCK(cb_lock);
  *cb_lock = 0;
  *cb_cancel = 0;
  *cb_count = 0;
  *cb_done = 0;
}

//  delay this thread until all threads arrive at barrier
//     or until barrier is cancelled
int cbarrier_wait() {

  if(debug & 32) printf("Thread %d: Entered cbarrier, now spinning\n", GET_THREAD_NUM);

  SET_LOCK_P(cb_lock);
    (*cb_count)++;
    PUT_ALL(cb_count, cb_count);
    if (*cb_count == GET_NUM_THREADS) {
      // FIXME: && (!cb_cancel) ?
      //printf("Thread %d: All nodes have arrived at the cbarrier.  cb_cancel = %d\n", GET_THREAD_NUM, *cb_cancel);
      *cb_done = 1;
      PUT_ALL(cb_done, cb_done);
    }
  UNSET_LOCK_P(cb_lock);

  // spin
  while (!(*cb_cancel) && !(*cb_done));
  if(debug & 32) printf("Thread %d: done spinning in cbarrier\n", GET_THREAD_NUM);

  SET_LOCK_P(cb_lock);
    (*cb_count)--;
    PUT_ALL(cb_count, cb_count);
    *cb_cancel = 0;
  UNSET_LOCK_P(cb_lock);

  if(debug & 32) printf("Thread %d: leaving cbarrier_wait.  cb_done = %d\n", GET_THREAD_NUM, *cb_done);

  return *cb_done;
}

/* Causes all threads waiting at the quiescent barrier, to be released. */
void cbarrier_cancel() {
  int k;

  SET_LOCK_P(cb_lock);

  *cb_cancel = 1;
  PUT_ALL(cb_cancel, cb_cancel);

  UNSET_LOCK_P(cb_lock);

}

void releaseNodes(StealStack *ss){
  if (doSteal) {
    if (ss_localDepth(ss) > 2 * chunkSize) {
      if (debug & 128) printf("PE %d releasing %d nodes. They had %d nodes available to steal.\n", GET_THREAD_NUM, chunkSize, ss->workAvail);
      ss_release(ss, chunkSize);
      cbarrier_cancel();
    }
  }
}

/* 
 * parallel search of UTS trees using work stealing 
 * 
 *   Note: tree size is measured by the number of
 *         push operations
 */
void parTreeSearch(StealStack *ss) {
  int done = 0;
  Node * parent;
  Node child;

  /* template for children */
  initNode(&child);

  /* tree search */
  while (done == 0) {
    /* local work */
    while (ss_localDepth(ss) > 0) {		
      /* examine node at stack top */
      parent = ss_top(ss);
      if (parent->numChildren < 0){
	// first time visited, construct children and place on stack
	genChildren(parent,&child,ss);

      }
      else {
	// second time visit, process accumulated statistics and pop
        ss_pop(ss);
      }
      
      // release some nodes for stealing, if enough are available
      // and wake up quiescent threads
      releaseNodes(ss);
    }
    
    /* local work exhausted on this stack - resume tree search if able
     * to re-acquire work from shared portion of this thread's stack
     */
    if (ss_acquire(ss, chunkSize)) {
	if (debug & 128) printf("Thread %d stole %d nodes from itself\n", GET_THREAD_NUM, chunkSize);
      continue;
    }

    /* no work left in this thread's stack */
    ss->work_time += time_split(&ss->time);

    /* try to steal work from another thread's stack */
    if (doSteal) {
      int goodSteal = 0;
      int victimId = findwork(chunkSize);
      while (victimId != -1 && !goodSteal) {
	// some work detected, try to steal it
	goodSteal = ss_steal(ss, victimId, chunkSize);
	if (!goodSteal) {
	  if (debug & 128) printf("Thread %d found work on thread %d but could not steal it.\n", GET_THREAD_NUM, victimId);
	  victimId = findwork(chunkSize);
	}
      }
      ss->search_time += time_split(&ss->time);
      if (goodSteal) {
	  if (debug & 128) printf("Thread %d stole %d nodes from thread %d\n", GET_THREAD_NUM, victimId, chunkSize);
	  continue;
	}
    }
	
    /* unable to steal work from shared portion of other stacks -
     * enter quiescent state waiting for termination (done != 0)
     * or cancellation because some thread has made work available
     * (done == 0).
     */
    if (debug & 128) printf("Thread %d couldn't steal any work.  Entering cbarrier.\n", GET_THREAD_NUM);
    done = cbarrier_wait();
    ss->idle_time += time_split(&ss->time);
    ss->idle_sessions++;
  }
  
  /* tree search complete ! */
}


// construct string with all parameter settings 
char * showParametersStr(char *strBuf) {
  int ind = 0;

  if (verbose == 0) {
    ind += sprintf(strBuf+ind, "");
    return strBuf;
  }
	
  // version + execution model
  {
    char * name[5] = {"Sequential C", "C/OpenMP", "UPC", "CRAY_SHMEM", "GPSHMEM"};
    ind += sprintf(strBuf+ind, "UTS 2.0 (%s)\n",name[COMPILER_TYPE]);
  }

  // tree type
  {
    char * name[3] = {"Binomial", "Geometric", "Hybrid"};
    ind += sprintf(strBuf+ind, "Tree type:  %d (%s)\n", type, name[type]);
  }
	
  // tree shape parameters
  ind += sprintf(strBuf+ind, "Tree shape parameters:\n");
  ind += sprintf(strBuf+ind, "  root branching factor b_0 = %.1f, root seed = %d\n", b_0, rootId);
	
  if (type == GEO || type == HYBRID){
    char * name[4] = {"Linear decrease", "Exponential decrease", "Cyclic", 
                      "Fixed branching factor"}; 
    ind += sprintf(strBuf+ind, "  GEO parameters: gen_mx = %d, shape function = %d (%s)\n", 
            gen_mx, shape_fn, name[shape_fn]);
  }
  if (type == BIN || type == HYBRID){
    double q = nonLeafProb;
    int m = nonLeafBF;
    double es  = (1.0 / (1.0 - q * m));
    ind += sprintf(strBuf+ind, "  BIN parameters:  q = %f, m = %d, E(n) = %f, E(s) = %.2f\n",
            q, m, q * m, es);
  }
  if (type == HYBRID){
    ind += sprintf(strBuf+ind, "  HYBRID:  GEO from root to depth %d, then BIN\n", 
            (int) ceil(shiftDepth * gen_mx));
  }
	
  // search method
  ind += sprintf(strBuf+ind, "Execution strategy:  ");
  if (PARALLEL) {
    ind += sprintf(strBuf+ind, "Parallel search using %d threads\n", GET_NUM_THREADS);
    if (doSteal)
      ind += sprintf(strBuf+ind, "   Load balance by work stealing, chunk size = %d nodes\n",chunkSize);
    else
      ind += sprintf(strBuf+ind, "   No load balancing.\n");
  }
  else
    ind += sprintf(strBuf+ind, "Sequential search\n");
      
  // random number generator
  ind += sprintf(strBuf+ind, "Random number generator: ");
  ind = rng_showtype(strBuf, ind);
  ind += sprintf(strBuf+ind, "\n\n");

  return strBuf;
}

// show parameter settings
void showParameters() {
  char strBuf[5000];

  if (verbose > 0) {
    showParametersStr(strBuf);
    printf("%s",strBuf);
  }
}

// display search statistics
void showStats(double elapsedSecs) {
  int i,pe;
  int tnodes = 0, trel = 0, tacq = 0, tsteal = 0, tfail= 0;
  int mdepth = 0, mheight = 0;
  double twork = 0.0;

  StealStack *final_ss = malloc(GET_NUM_THREADS*sizeof(StealStack));
  
  /* Assemble all of the stealstacks so we can gather some stats. */
  memcpy(&final_ss[GET_THREAD_NUM], stealStack, sizeof(StealStack));
  for (i = 1; i < GET_NUM_THREADS; i++) {
	  pe = (GET_THREAD_NUM + i) % GET_NUM_THREADS;
	  SMEMCPY(&final_ss[pe], stealStack, sizeof(StealStack), pe);
  }

  // combine measurements from all threads
  for (i = 0; i < GET_NUM_THREADS; i++) {
    tnodes += final_ss[i].nNodes;
    trel   += final_ss[i].nRelease;
    tacq   += final_ss[i].nAcquire;
    tsteal += final_ss[i].nSteal;
    tfail  += final_ss[i].nFail;
    twork  += final_ss[i].work_time;
    mdepth  = max(mdepth, final_ss[i].maxStackDepth);
    mheight = max(mheight, final_ss[i].maxTreeDepth);
  }
  if (trel != tacq + tsteal) {
    printf("*** error! total released != total acquired + total stolen\n");
  }
    
  // summary execution info for human consumption
  if (verbose > 0) {
    printf("Tree size = %d, tree depth = %d\n", tnodes, mheight); 
    printf("Total chunks released = %d, of which %d reacquired and %d stolen\n",
           trel, tacq, tsteal);
    printf("Failed steals = %d, Max stack size = %d\n", tfail, mdepth);
    printf("Wall clock time = %.3f sec, performance = %.0f nodes/sec (%.0f nodes/sec per thread)\nTotal work time = %.3f sec\n", 
           elapsedSecs, (tnodes / elapsedSecs), 
           (tnodes / elapsedSecs / GET_NUM_THREADS), twork); 
  }

  // summary execution info for machine consumption
  if (verbose == 0) {
    printf("%3d %7.3f %9d %7.0f %7.0f %7.3f %d %d %d %d %.2f %d %d %1d %f %3d %1d %1d\n",
           GET_NUM_THREADS, elapsedSecs, tnodes, 
           tnodes/elapsedSecs, (tnodes/elapsedSecs)/GET_NUM_THREADS, twork,
           chunkSize, tsteal, tfail,  
           type, b_0, rootId, gen_mx, shape_fn, nonLeafProb, nonLeafBF, 
           COMPILER_TYPE, RNG_TYPE);
  }

  // per thread execution info
  if (verbose > 1) {
    for (i = 0; i < GET_NUM_THREADS; i++) {
      printf("** Thread %d\n", i);
      printf("  # nodes explored    = %d\n", final_ss[i].nNodes);
      printf("  # chunks released   = %d\n", final_ss[i].nRelease);
      printf("  # chunks reacquired = %d\n", final_ss[i].nAcquire);
      printf("  # chunks stolen     = %d\n", final_ss[i].nSteal);
      printf("  # failed steals     = %d\n", final_ss[i].nFail);
      printf("  maximum stack depth = %d\n", final_ss[i].maxStackDepth);
      printf("  work time           = %.6f secs\n",final_ss[i].work_time);
      printf("  search time         = %.6f secs\n",final_ss[i].search_time);
      printf("  idle time           = %.6f secs (%d sessions)\n",
             final_ss[i].idle_time, final_ss[i].idle_sessions);
      printf("\n");
    }
  }
}


void parseParameters(int argc, char *argv[]){
  int i = 1; 
  int err = -1;
  while (i < argc && err == -1) {
    if (argv[i][0] != '-' || strlen(argv[i]) != 2 || argc <= i+1) {
      err = i; break;
    }
    switch (argv[i][1]) {
    case 'q':
      nonLeafProb = atof(argv[i+1]); break;
    case 'm':
      nonLeafBF = atoi(argv[i+1]); break;
    case 'r':
      rootId = atoi(argv[i+1]); break;
    case 'c':
      chunkSize = atoi(argv[i+1]); break;
    case 'x':
      debug = atoi(argv[i+1]); break;
    case 'v':
      verbose = atoi(argv[i+1]); break;
    case 's':
     doSteal = atoi(argv[i+1]); 
      if (doSteal != 1 && doSteal != 0) 
	err = i;
      break;
    case 't':
      type = atoi(argv[i+1]); 
      if (type != BIN && type != GEO && type!= HYBRID) 
	err = i;
      break;
    case 'a':
      shape_fn = atoi(argv[i+1]); break;
    case 'b':
      b_0 = atof(argv[i+1]); break;
    case 'd':
      gen_mx = atoi(argv[i+1]); break;
    case 'f':
      shiftDepth = atof(argv[i+1]); break;
    default:
      err = i; break;
    }
    i +=2;
  }
  if (err != -1) {
    printf("unrecognized parameter %s or incorrect value %d\n", argv[i], i);
    printf("usage:  uts (parameter value)*\n");
    printf("parm type  description\n");    
    printf("==== ====  =========================================\n");    
    printf(" -t  int   tree type (0: BIN, 1: GEO, 2: HYBRID)\n");
    printf(" -b  dble  root branching factor\n");
    printf(" -r  int   root seed 0 <= r < 2^31 \n");
    printf(" -a  int   GEO: tree shape function \n");
    printf(" -d  int   GEO: tree depth\n");
    printf(" -q  dble  BIN: probability of non-leaf node\n");
    printf(" -m  int   BIN: number of children for non-leaf node\n");
    printf(" -f  dble  HYBRID: fraction of depth for GEO -> BIN transition\n");
    printf(" -s  int   nonzero to enable work stealing\n");
    printf(" -c  int   chunksize for work sharing and work stealing\n");
    printf(" -u  int   unbalance measure (-1: none; 0: min/size; 1: min/n; 2: max/n)\n");
    printf(" -v  int   nonzero to set verbose output\n");
    printf(" -l  file  output stats as a comma delimited list to \'file\'\n");
    printf(" -x  int   debug level\n");
    exit(4);
  }
}

// NB:  run in parallel under UPC, single thread under openMP
void resolveParameterConflicts() {
}

/*
 *  note on execution model:
 *     under openMP, global vars are shared
 *     under UPC, global vars are private unless explicitly shared
 *
 *  UPC is SPMD starting with main, OpenMP goes SPMD after
 *  parsing parameters
 */
int main(int argc, char *argv[]) {
  Node root;

  gpshmem_init(&argc, &argv);
  stealStack = ALLOC(sizeof(StealStack));

  /* determine benchmark parameters */
  uts_parseParams(argc, argv);

  /* cancellable barrier initialization (single threaded under OMP) */
  cb_init();

  double t1, t2, et;
  StealStack * ss = stealStack;

  /* show parameter settings */
  if (GET_THREAD_NUM == 0)
    uts_printParams(stdout);

  /* initialize stealstacks */
  ss_init(ss, MAXSTACKDEPTH);

  /* initialize root node and push on thread 0 stack */
  if (GET_THREAD_NUM == 0) {
    uts_initRoot(&root, type);
    ss_push(ss, &root);
  }

  // line up for the start
  BARRIER

  /* time parallel search */
  t1 = uts_wctime();
  ss->time = t1;
  parTreeSearch(ss);
  t2 = uts_wctime();
  et = t2 - t1;

  /* display results */
  if (GET_THREAD_NUM == 0) {
    showStats(et);
  } 

  gpshmem_finalize();
}
