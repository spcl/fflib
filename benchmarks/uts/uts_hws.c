#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#ifdef sgi
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <mpi.h>

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))


/***********************************************************
 *                                                         *
 *  splitable random number generator to use:              *
 *     (default)  sha1 hash                                *
 *     (UTS_ALFG) additive lagged fibonacci generator      *
 *                                                         *
 ***********************************************************/

#if defined(UTS_ALFG)
#  include "rng/alfg.h"
#  define RNG_TYPE 1
#elif defined(BRG_RNG)
#  include "rng/brg_sha1.h"
#  define RNG_TYPE 0
#elif defined(DEVINE_RNG)
#  include "rng/devine_sha1.h"
#  define RNG_TYPE 0
#endif

/** Pthreads **/
#include <pthread.h>

#define PARALLEL         1
#define COMPILER_TYPE    0
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAXTHREADS       128
#define LOCK_T           pthread_mutex_t
#define GET_NUM_THREADS  pthread_num_threads
#define GET_NUM_STACKS   (pthread_num_threads+1)
#define GET_THREAD_NUM   *(int*)pthread_getspecific(pthread_thread_num)
#define SET_LOCK(zlk)    pthread_mutex_lock(zlk)
#define UNSET_LOCK(zlk)  pthread_mutex_unlock(zlk)
#define INIT_LOCK(zlk)   zlk = pthread_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk)  zlk = pthread_global_lock_alloc()
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define BARRIER           

int pthread_num_threads = 1;              // Command line parameter - default to 1
pthread_key_t pthread_thread_num;         // Key to store each thread's ID

/* helper function to match UPC lock allocation semantics */
LOCK_T * pthread_global_lock_alloc() {    
    LOCK_T *lock = (LOCK_T *) malloc(sizeof(LOCK_T));
    pthread_mutex_init(lock, NULL);
    return lock;
}

/** Debugging Macros **/
#define DBG_GEN    1
#define DBG_CHUNK  2
#define DBG_TOKEN  4
#define DBG_MSGCNT 8

//#define DEBUG_LEVEL (DBG_GEN | DBG_CHUNK)
#define DEBUG_LEVEL 0

#define DEBUG(dbg_class, command) if (DEBUG_LEVEL & (dbg_class)) { command; }


/**** Hierarchical Load Balancing State ****/
static volatile int start_search = 0;

/**** MPI Work Stealing State ****/
enum uts_tags { MPIWS_WORKREQUEST = 1, MPIWS_WORKRESPONSE, MPIWS_TDTOKEN, MPIWS_STATS };
enum colors { BLACK = 0, WHITE, PINK, RED };
char *color_names[] = { "BLACK", "WHITE", "PINK", "RED" };

typedef struct {
	enum colors color;
	long send_count;
	long recv_count;
} td_token_t;

/** Global State **/
static int comm_size, comm_rank;
static enum colors my_color;     // Ring-based termination detection 
static int         last_steal;   // Rank of last thread stolen from
static long        chunks_recvd; // Total messages received
static long        chunks_sent;  // Total messages sent
static long        ctrl_recvd;   // Total messages received
static long        ctrl_sent;    // Total messages sent

/** Global Communication handles **/
static MPI_Request  wrin_request;  // Incoming steal request
static MPI_Request  wrout_request; // Outbound steal request
static MPI_Request  iw_request;    // Outbound steal request
static MPI_Request  td_request;    // Term. Detection listener

/** Global Communication Buffers **/
static long        wrin_buff;       // Buffer for accepting incoming work requests
static long        wrout_buff;      // Buffer to send outgoing work requests
static void       *iw_buff;         // Buffer to receive incoming work
static td_token_t  td_token;        // Dijkstra's token


/***********************************************************
 *                                                         *
 *  Tree node descriptor and statistics                    *
 *                                                         *
 ***********************************************************/
#define MAXSTACKDEPTH   50000  
#define MAXNUMCHILDREN    100  // cap on children (BIN root is exempt)
#define MAXHISTSIZE      2000  // max tree depth in histogram

struct node_t{
  int type;          // distribution governing number of children
  int height;        // depth of this node in the tree
  int numChildren;   // number of children, -1 => not yet determined
  
  /* for statistics (if configured via UTS_STAT) */
#ifdef UTS_STAT
  struct node_t *pp;          // parent pointer
  int sizeChildren;           // sum of children sizes
  int maxSizeChildren;        // max of children sizes
  int ind;
  int size[MAXNUMCHILDREN];   // children sizes
  double unb[MAXNUMCHILDREN]; // imbalance of each child 0 <= unb_i <= 1
#endif
  
  /* for RNG state associated with this node */
  struct state_t state;
};

typedef struct node_t Node;


/***********************************************************
 *                                                         *
 *  tree generation and search parameters                  *
 *                                                         *
 *  Tree generation strategy is controlled via various     *
 *  parameters set from the command line.  The parameters  *
 *  and their default values are given below.              *
 ***********************************************************/

/* Tree type
 *   Trees are generated using a Galton-Watson process, in 
 *   which the branching factor of each node is a random 
 *   variable.
 *   
 *   The random variable can follow a binomial distribution
 *   or a geometric distribution.  Hybrid tree are
 *   generated with geometric distributions near the
 *   root and binomial distributions towards the leaves.
 */
#define BIN 0
#define GEO 1
#define HYBRID 2
int type   = GEO;   // default tree type
double b_0 = 4.0;   // default branching factor at the root
int rootId = 0;     // default seed for RNG state at root

/*  Tree type BIN (BINOMIAL)
 *  The branching factor at the root is specified by b_0.
 *  The branching factor below the root follows an 
 *     identical binomial distribution at all nodes.
 *  A node has m children with prob q, or no children with 
 *     prob (1-q).  The expected branching factor is q * m.
 *
 *  Default parameter values 
 */
int    nonLeafBF   = 4;            // m
double nonLeafProb = 15.0 / 64.0;  // q

/*  Tree type GEO (GEOMETRIC)
 *  The branching factor follows a geometric distribution with
 *     expected value b.
 *  The probability that a node has 0 <= n children is p(1-p)^n for
 *     0 < p <= 1. The distribution is truncated at MAXNUMCHILDREN.
 *  The expected number of children b = (1-p)/p.  Given b (the
 *     target branching factor) we can solve for p.
 *
 *  A shape function computes a target branching factor b_i
 *     for nodes at depth i as a function of the root branching
 *     factor b_0 and a maximum depth gen_mx.
 *   
 *  Default parameter values
 */
int gen_mx    = 6;     // default depth of tree
int shape_fn  = 0;     // default shape function (b_i decr linearly)

/*
 *  In type HYBRID trees, each node is either type BIN or type
 *  GEO, with the generation strategy changing from GEO to BIN 
 *  at a fixed depth, expressed as a fraction of gen_mx
 */
double shiftDepth = 0.5;         

/*
 * compute granularity - number of rng evaluations per tree node
 */
int computeGranularity = 1;

/* (parallel) execution parameters 
 */
int doSteal   = 1;     // 1 => use work stealing
int chunkSize = 20;    // number of nodes to move to/from shared area
int stealNchunks = 1;  // Number of chunks to steal (internode)


/* display parameters 
 */
int debug    = 0;
int verbose  = 1;
int release_interval    = 1;
int pollint  = 100;

/*
 * Tree statistics (if selected via UTS_STAT)
 *   compute overall size and imbalance metrics
 *   and histogram size and imbalance per level
 */
#ifdef UTS_STAT
int stats    = 1;
int unbType  = 1;
int hist[MAXHISTSIZE+1][2];         // average # nodes per level
double unbhist[MAXHISTSIZE+1][3];   // average imbalance per level
int maxHeight = 0;         // maximum depth of tree
double maxImb = 0;         // maximum imbalance
double minImb = 1;
double treeImb=-1;         // Overall imbalance, undefined

int *rootSize;             // size of the root's children 
double *rootUnb;           // imbalance of root's children

/* Tseng statistics */
int totalNodes=0;
double imb_max = 0;         //% of work in largest child (ranges from 100/n to 100%)
double imb_avg = 0;
double imb_devmaxavg = 0;   //( % of work in largest child ) - ( avg work )
double imb_normdevmaxavg = 0;  // ( % of work in largest child ) - ( avg work ) / ( 100% - avg work )
#else
int stats = 0;
int unbType = -1;
#endif
/* CSV filename */
char csv_filename[256] = "";


/***********************************************************
 *                                                         *
 *  per-thread state                                       *
 *                                                         *
 ***********************************************************/

#define SS_WORK    0
#define SS_SEARCH  1
#define SS_IDLE    2
#define SS_OVH     3
#define SS_CBOVH   4
#define SS_NSTATES 5

/* session record for session visualization */
struct sessionRecord_t
{
  double startTime, endTime;
};
typedef struct sessionRecord_t SessionRecord;

/* steal record for steal visualization */
struct stealRecord_t
{
  long int nodeCount;           /* count nodes generated during the session */
  int victimThread;             /* thread from which we stole the work  */
};
typedef struct stealRecord_t StealRecord;

/* Store debugging and trace data */
struct metaData_t
{
  SessionRecord sessionRecords[SS_NSTATES][20000];   /* session time records */
  StealRecord stealRecords[20000]; /* steal records */
};
typedef struct metaData_t MetaData;

/* holds text string for debugging info */
char debug_str[1000];

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
  int wakeups, falseWakeups, nNodes_last;
  double time[SS_NSTATES], timeLast;         /* perf measurements */
  int entries[SS_NSTATES], curState; 
  LOCK_T * stackLock; /* lock for manipulation of shared portion */
  Node * stack;       /* addr of actual stack of nodes in local addr space */
  SHARED_INDEF Node * stack_g; /* addr of same stack in global addr space */
#ifdef TRACE
  MetaData * md;        /* meta data used for debugging and tracing */
#endif
};
typedef struct stealStack_t StealStack;

typedef SHARED StealStack * SharedStealStackPtr;


/***********************************************************
 *                                                         *
 *  shared state                                           *
 *                                                         *
 ***********************************************************/

// root Node 
Node *rootNode;

// shared access to each thread's stealStack
SHARED SharedStealStackPtr stealStack[MAXTHREADS];

// termination detection 
VOLATILE SHARED int cb_cancel;
VOLATILE SHARED int cb_count;
VOLATILE SHARED int cb_done;
VOLATILE SHARED int cb_nowork;
LOCK_T * cb_lock;

SHARED double startTime[MAXTHREADS];


/***********************************************************
 *                                                         *
 *  FUNCTIONS                                              *
 *                                                         *
 ***********************************************************/

/*
 * wall clock time
 *   for detailed accounting of work, this needs
 *   high resolution
 */
#ifdef sgi  
double wctime(){
  timespec_t tv;
  double time;
  clock_gettime(CLOCK_SGI_CYCLE,&tv);
  time = ((double) tv.tv_sec) + ((double)tv.tv_nsec / 1e9);
  return time;
}
#else
double wctime(){
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec + 1E-6 * tv.tv_usec);
}
#endif


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
  SET_LOCK(s->stackLock);
  s->sharedStart = 0;
  s->local  = 0;
  s->top    = 0;
  s->workAvail = 0;
  UNSET_LOCK(s->stackLock);
}

/* fatal error */
void ss_error(char *str) {
  printf("*** [Thread %i] %s\n",GET_THREAD_NUM, str);
  MPI_Abort(MPI_COMM_WORLD, 10);
}

/* initialize the stack */
void ss_init(StealStack *s, int nelts) {
  int nbytes = nelts * sizeof(Node);

  if (debug & 1)
    printf("Thread %d intializing stealStack %p, sizeof(Node) = %X\n", 
           GET_THREAD_NUM, s, (int)(sizeof(Node)));

  // allocate stack in shared addr space with affinity to calling thread
  // and record local addr for efficient access in sequel
  s->stack_g = (SHARED_INDEF Node *) ALLOC (nbytes);
  s->stack = (Node *) s->stack_g;
#ifdef TRACE
  s->md = (MetaData *) ALLOC (sizeof(MetaData));
  if (s->md == NULL)
    ss_error("ss_init: out of memory");
#endif
  if (s->stack == NULL) {
    printf("Request for %d bytes for stealStack on thread %d failed\n",
           nbytes, GET_THREAD_NUM);
    ss_error("ss_init: unable to allocate space for stealstack");
  }
  INIT_LOCK(s->stackLock);
  if (debug & 1)
    printf("Thread %d init stackLock %p\n", GET_THREAD_NUM, (void *) s->stackLock);
  s->stackSize = nelts;
  s->nNodes = 0;
  s->maxStackDepth = 0;
  s->maxTreeDepth = 0;
  s->nAcquire = 0;
  s->nRelease = 0;
  s->nSteal = 0;
  s->nFail = 0;
  s->wakeups = 0;
  s->falseWakeups = 0;
  s->nNodes_last = 0;
  ss_mkEmpty(s);
}


/* local push */
void ss_push(StealStack *s, Node *c) {
  if (s->top >= s->stackSize)
    ss_error("ss_push: overflow");
  if (debug & 1)
    printf("ss_push: Thread %d, posn %d: node %s [%d]\n",
           GET_THREAD_NUM, s->top, rng_showstate(c->state.state, debug_str), c->height);
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
           GET_THREAD_NUM, s->top - 1, rng_showstate(r->state.state, debug_str),
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
           GET_THREAD_NUM, s->top, rng_showstate(r->state.state, debug_str), 
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
  SET_LOCK(s->stackLock);
  if (s->top - s->local >= k) {
    s->local += k;
    s->workAvail += k;
    s->nRelease++;
  }
  else
    ss_error("ss_release:  do not have k vals to release");
  UNSET_LOCK(s->stackLock);
}

/* move k values from top of shared stack into local stack
 * return false if k vals are not avail on shared stack
 */
int ss_acquire(StealStack *s, int k) {
  int avail;
  SET_LOCK(s->stackLock);
  avail = s->local - s->sharedStart;
  if (avail >= k) {
    s->local -= k;
    s->workAvail -= k;
    s->nAcquire++;
  }
  UNSET_LOCK(s->stackLock);
  return (avail >= k);
}

/* steal k values from shared portion of victim thread's stealStack
 * onto local portion of current thread's stealStack.
 * return false if k vals are not avail in victim thread
 */
int ss_steal(StealStack *s, int victim, int k) {
  int victimLocal, victimShared, victimWorkAvail;
  int ok;
  
  if (s->sharedStart != s->top)
    ss_error("ss_steal: thief attempts to steal onto non-empty stack");

  if (s->top + k >= s->stackSize)
    ss_error("ss_steal: steal will overflow thief's stack");
  
  /* lock victim stack and try to reserve k elts */
  if (debug & 32)
    printf("Thread %d wants    SS %d\n", GET_THREAD_NUM, victim);
  
  SET_LOCK(stealStack[victim]->stackLock);
  
  if (debug & 32)
    printf("Thread %d acquires SS %d\n", GET_THREAD_NUM, victim);
  
  victimLocal = stealStack[victim]->local;
  victimShared = stealStack[victim]->sharedStart;
  victimWorkAvail = stealStack[victim]->workAvail;
  
  if (victimLocal - victimShared != victimWorkAvail)
    ss_error("ss_steal: stealStack invariant violated");
  
  ok = victimWorkAvail >= k;
  if (ok) {
    /* reserve a chunk */
    stealStack[victim]->sharedStart =  victimShared + k;
    stealStack[victim]->workAvail = victimWorkAvail - k;
  }
  UNSET_LOCK(stealStack[victim]->stackLock);
  
  if (debug & 32)
    printf("Thread %d releases SS %d\n", GET_THREAD_NUM, victim);
	
  /* if k elts reserved, move them to local portion of our stack */
  if (ok) {
    SHARED_INDEF Node * victimStackBase = stealStack[victim]->stack_g;
    SHARED_INDEF Node * victimSharedStart = victimStackBase + victimShared;

    SMEMCPY(&(s->stack[s->top]), victimSharedStart, k * sizeof(Node));

    s->nSteal++;
    if (debug & 4) {
      int i;
      for (i = 0; i < k; i ++) {
        Node * r = &(s->stack[s->top + i]);
        printf("ss_steal:  Thread %2d posn %d (steal #%d) receives %s [%d] from thread %d posn %d (%p)\n",
               GET_THREAD_NUM, s->top + i, s->nSteal,
               rng_showstate(r->state.state, debug_str),
               r->height, victim, victimShared + i, 
               (void *) victimSharedStart);
      }
    }
    s->top += k;

   #ifdef TRACE
      /* update session record of theif */
      s->md->stealRecords[s->entries[SS_WORK]].victimThread = victim;
   #endif
  }
  else {
    s->nFail++;
    if (debug & 4) {
      printf("Thread %d failed to steal %d nodes from thread %d, ActAv = %d, sh = %d, loc =%d\n",
	     GET_THREAD_NUM, k, victim, victimWorkAvail, victimShared, victimLocal);
    }
  }
  return (ok);
} 

/* search other threads for work to steal */
int findwork(int k) {
  int i,v;
  for (i = 1; i < GET_NUM_STACKS; i++) {
    v = (GET_THREAD_NUM + i) % GET_NUM_STACKS;
    if (stealStack[v]->workAvail >= k)
      return v;
  }
  return -1;
}

void ss_initState(StealStack *s) {
  int i;
  s->timeLast = wctime();
  for (i = 0; i < SS_NSTATES; i++) {
    s->time[i] = 0.0;
    s->entries[i] = 0;
  }
  s->curState = SS_IDLE;
  if (debug & 8)
    printf("Thread %d start state %d (t = %f)\n", 
           GET_THREAD_NUM, s->curState, s->timeLast);
}

void ss_setState(StealStack *s, int state){
  double time;
  if (state < 0 || state >= SS_NSTATES)
    ss_error("ss_setState: thread state out of range");
  if (state == s->curState)
    return;
  time = wctime();
  s->time[s->curState] +=  time - s->timeLast;

  #ifdef TRACE  
    /* close out last session record */
    s->md->sessionRecords[s->curState][s->entries[s->curState] - 1].endTime = time;
    if (s->curState == SS_WORK)
    {
       s->md->stealRecords[s->entries[SS_WORK] - 1].nodeCount = s->nNodes
           - s->md->stealRecords[s->entries[SS_WORK] - 1].nodeCount;
    }

    /* initialize new session record */
    s->md->sessionRecords[state][s->entries[state]].startTime = time;
    if (state == SS_WORK)
    {
       s->md->stealRecords[s->entries[SS_WORK]].nodeCount = s->nNodes;
    }
  #endif

  s->entries[state]++;
  s->timeLast = time;
  s->curState = state;

  if(debug & 8)
    printf("Thread %d enter state %d [#%d] (t = %f)\n",
           GET_THREAD_NUM, state, s->entries[state], time);
}

#ifdef UTS_STAT
/*
 * Statistics, 
 * : number of nodes per level
 * : imbalanceness of nodes per level
 *
 */
void initHist()
{
  int i;
  for (i=0; i<MAXHISTSIZE; i++){
    hist[i][0]=0;
    hist[i][1]=0;
    unbhist[i][1]=1;
    unbhist[i][2]=0;
  }
}

void updateHist(Node* c, double unb)
{
  if (c->height<MAXHISTSIZE){
    hist[c->height][1]++;
    hist[c->height][0]+=c->numChildren;

    unbhist[c->height][0]+=unb;
    if (unbhist[c->height][1]>unb)
      unbhist[c->height][1]=unb;
    if (unbhist[c->height][2]<unb)
      unbhist[c->height][2]=unb;
		
  }
  else {
    hist[MAXHISTSIZE][1]++;
    hist[MAXHISTSIZE][0]+=c->numChildren;
  }
}

void showHist(FILE *fp)
{
  int i;	
  fprintf(fp, "depth\tavgNumChildren\t\tnumChildren\t imb\t maxImb\t minImb\t\n");
  for (i=0; i<MAXHISTSIZE; i++){
    if ((hist[i][0]!=0)&&(hist[i][1]!=0))
      fprintf(fp, "%d\t%f\t%d\t %lf\t%lf\t%lf\n", i, (double)hist[i][0]/hist[i][1], 
              hist[i][0], unbhist[i][0]/hist[i][1], unbhist[i][1], unbhist[i][2]);	
  }
}

double getImb(Node *c)
{
  int i=0;
  double avg=.0, tmp=.0;
  double unb=0.0;
  
  avg=(double)c->sizeChildren/c->numChildren;

  for (i=0; i<c->numChildren; i++){		
    if ((type==BIN)&&(c->pp==NULL))
      {
        if (unbType<2)
          tmp=min((double)rootSize[i]/avg, avg/(double)rootSize[i]);
        else 
          tmp=max((double)rootSize[i]/avg, avg/(double)rootSize[i]);
        
        if (unbType>0)
          unb+=tmp*rootUnb[i];
        else 
          unb+=tmp*rootUnb[i]*rootSize[i];
      }	
    else{
      if (unbType<2)
        tmp=min((double)c->size[i]/avg, avg/(double)c->size[i]);
      else 
        tmp=max((double)c->size[i]/avg, avg/(double)c->size[i]);
      
      if (unbType>0)
        unb+=tmp*c->unb[i];
      else 
        unb+=tmp*c->unb[i]*c->size[i];
    }
  }
	
  if (unbType>0){
    if (c->numChildren>0) 
      unb=unb/c->numChildren;
    else unb=1.0;
  }
  else {
    if (c->sizeChildren>1) 
      unb=unb/c->sizeChildren;
    else unb=1.0;
  }
  if ((debug & 1) && unb>1) printf("unb>1%lf\t%d\n", unb, c->numChildren);
	
  return unb;
}

void getImb_Tseng(Node *c)
{
  double t_max, t_avg, t_devmaxavg, t_normdevmaxavg;

  if (c->numChildren==0)
    {
      t_avg =0;
      t_max =0;
    }
  else 
    {
      t_max = (double)c->maxSizeChildren/(c->sizeChildren-1);
      t_avg = (double)1/c->numChildren;
    }

  t_devmaxavg = t_max-t_avg;
	
  if (debug & 1)
    printf("max\t%lf, %lf, %d, %d, %d\n", t_max, t_avg, 
           c->maxSizeChildren, c->sizeChildren, c->numChildren);
	
  if (1-t_avg==0)
    t_normdevmaxavg = 1;
  else
    t_normdevmaxavg = (t_max-t_avg)/(1-t_avg);

  imb_max += t_max;
  imb_avg += t_avg;
  imb_devmaxavg += t_devmaxavg;
  imb_normdevmaxavg +=t_normdevmaxavg;
}

void updateParStat(Node *c)
{
  double unb;

  totalNodes++;
  if (maxHeight<c->height) 
    maxHeight=c->height;
	
  unb=getImb(c);
  maxImb=max(unb, maxImb);
  minImb=min(unb, minImb);
  updateHist(c, unb);
  
  getImb_Tseng(c);
	
  if (c->pp!=NULL){
    if ((c->type==BIN)&&(c->pp->pp==NULL)){
      rootSize[c->pp->ind]=c->sizeChildren;
      rootUnb[c->pp->ind]=unb;
    }
    else{
      c->pp->size[c->pp->ind]=c->sizeChildren;
      c->pp->unb[c->pp->ind]=unb;
    }
    /* update statistics per node*/
    c->pp->ind++;
    c->pp->sizeChildren+=c->sizeChildren;
    if (c->pp->maxSizeChildren<c->sizeChildren)
      c->pp->maxSizeChildren=c->sizeChildren;		
  }
  else 
    treeImb = unb;
}
#endif

/*
 *	Tree Implementation      
 *
 */

// Interpret 32 bit positive integer as value on [0,1)
double toProb(int n) {
  if (n < 0) {
    printf("*** toProb: rand n = %d out of range\n",n);
  }
  return ((n<0)? 0.0 : ((double) n)/2147483648.0);
}


void initNode(Node * child)
{
  child->type = -1;
  child->height = -1;
  child->numChildren = -1;    // not yet determined

#ifdef UTS_STAT
  if (stats){	
    int i;
    child->ind = 0;
    child->sizeChildren = 1;
    child->maxSizeChildren = 0;
    child->pp = NULL;
    for (i = 0; i < MAXNUMCHILDREN; i++){
      child->size[i] = 0;
      child->unb[i]  = 0.0;
    }
  }
#endif
}


void initRootNode(Node * root, int type)
{
  root->type = type;
  root->height = 0;
  root->numChildren = -1;      // means not yet determined
  rng_init(root->state.state, rootId);

  if (debug & 1)
    printf("root node of type %d at %p\n",type, root);

  #ifdef TRACE
    stealStack[0]->md->stealRecords[0].victimThread = 0;  // first session is own "parent session"
  #endif

#ifdef UTS_STAT
  if (stats){
    int i;
    root->ind = 0;
    root->sizeChildren = 1;
    root->maxSizeChildren = 1;
    root->pp = NULL;
    
    if (type != BIN){
      for (i=0; i<MAXNUMCHILDREN; i++){
        root->size[i] = 0;
        root->unb[i]  =.0; 
      }
    }
    else {
      int rbf = (int) ceil(b_0);
      rootSize = malloc(rbf*sizeof(int));
      rootUnb = malloc(rbf*sizeof(double));
      for (i = 0; i < rbf; i++) {
        rootSize[i] = 0;
        rootUnb[i] = 0.0;
      }
    }
  }
#endif
}


int numBINchildren(Node * parent){
  // distribution is identical everywhere below root

  int v = rng_rand(parent->state.state);	
  double d = toProb(v);
  int numChildren = ( d < nonLeafProb) ? nonLeafBF : 0;

  return numChildren;
}


int numGEOchildren(Node * parent){
  double b_i = b_0;
  int depth = parent->height;
  int numChildren, h;
  double p, u;
  
  // use shape function to compute target b_i
  if (depth > 0){
    switch (shape_fn){
      
      // expected size polynomial in depth
    case 1:
      b_i = b_0 * pow((double) depth, -log(b_0)/log((double) gen_mx));
      break;
      
      // cyclic tree size
    case 2:
      if (depth > 5 * gen_mx){
        b_i = 0.0;
        break;
      } 
      b_i = pow(b_0, 
                sin(2.0*3.141592653589793*(double) depth / (double) gen_mx));
      break;

      // identical distribution at all nodes up to max depth
    case 3:
      b_i = (depth < gen_mx)? b_0 : 0;
      break;
      
      // linear decrease in b_i
    case 0:
    default:
      b_i =  b_0 * (1.0 - (double)depth / (double) gen_mx);
      break;
    }
  }

  // given target b_i, find prob p so expected value of 
  // geometric distribution is b_i.
  p = 1.0 / (1.0 + b_i);

  // get uniform random number on [0,1)
  h = rng_rand(parent->state.state);
  u = toProb(h);

  // max number of children at this cumulative probability
  // (from inverse geometric cumulative density function)
  numChildren = floor(log(1 - u) / log(1 - p)); 

  return numChildren;
}

// forward decl
void releaseNodes(StealStack *ss);

/* 
 * Generate all children of the parent
 *
 * details depend on tree type, node type and shape function
 *
 */
void genChildren(Node * parent, Node * child, StealStack * ss) {
  int parentHeight = parent->height;
  int numChildren, childType;

  // switch on tree type
  switch (type) {
    
  case BIN:
    childType = BIN;
    if (parentHeight == 0)
      numChildren = (int) floor(b_0);
    else {
      numChildren = numBINchildren(parent);
    }
    break;
  
  case GEO:
    childType = GEO;
    numChildren = numGEOchildren(parent);
    break;
    
  case HYBRID:
    if (parentHeight < shiftDepth * gen_mx) {
      childType = GEO;
      numChildren = numGEOchildren(parent);
    }
    else {
      childType = BIN;
      numChildren = numBINchildren(parent);
    break;

    }
  }
  
  // limit number of children
  // only a BIN root can have more than MAXNUMCHILDREN
  if (parentHeight == 0 && parent->type == BIN) {
    int rootBF = (int) ceil(b_0);
    if (numChildren > rootBF) {
      printf("*** Number of children of root truncated from %d to %d\n",
             numChildren, rootBF);
      numChildren = rootBF;
    }
  }
  else {
    if (numChildren > MAXNUMCHILDREN) {
      printf("*** Number of children truncated from %d to %d\n", 
             numChildren, MAXNUMCHILDREN);
      numChildren = MAXNUMCHILDREN;
    }
  }

  // record number of children in parent
  parent->numChildren = numChildren;
  if (debug & 2) {
    printf("Gen:  Thread %d, posn %2d: node %s [%d] has %2d children\n",
           GET_THREAD_NUM, ss_topPosn(ss),
           rng_showstate(parent->state.state, debug_str), 
           parentHeight, numChildren);
  }
  
  // construct children and push onto stack
  if (numChildren > 0) {
    int i, j;
    child->type = childType;
    child->height = parentHeight + 1;

#ifdef UTS_STAT
    if (stats)
      child->pp = parent;  // pointer to parent
#endif

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


    
/*
 *  Parallel tree traversal
 *
 */

// cancellable barrier

// initialize lock:  single thread under omp, all threads under upc
void cb_init(){
  INIT_SINGLE_LOCK(cb_lock);
  if (debug & 4)
    printf("Thread %d, cb lock at %p\n", GET_THREAD_NUM, (void *) cb_lock);

  // fixme: no need for all upc threads to repeat this
  SET_LOCK(cb_lock);
  cb_count = 0;
  cb_cancel = 0;
  cb_done = 0;
  UNSET_LOCK(cb_lock);
}

//  delay this thread until all threads arrive at barrier
//     or until barrier is cancelled
int cbarrier_wait() {
  int l_count, l_done, l_cancel;
  int pe = GET_THREAD_NUM;

  SET_LOCK(cb_lock);
  cb_count++;
  if (cb_count == GET_NUM_THREADS && !cb_cancel) {
		if (!stealStack[GET_NUM_STACKS-1]->workAvail == 0)
			printf("cbarrier_wait(): detected termination but there is work in the master's stack!");
    cb_nowork = 1;
  }
  l_count = cb_count;
  l_done = cb_done;
  /* Track false wakeups */
  if (stealStack[pe]->nNodes_last == stealStack[pe]->nNodes) {
    ++stealStack[pe]->falseWakeups;
  }
  stealStack[GET_THREAD_NUM]->nNodes_last = stealStack[pe]->nNodes;
  UNSET_LOCK(cb_lock);

  if (debug & 16)
    printf("Thread %d enter spin-wait, count = %d, done = %d\n",
           GET_THREAD_NUM, l_count, l_done);

  // spin
  do {
    l_count = cb_count;
    l_cancel = cb_cancel;
    l_done = cb_done;
  }
  while (!l_cancel && !l_done);

  if (debug & 16)
    printf("Thread %d exit  spin-wait, count = %d, done = %d, cancel = %d\n",
           GET_THREAD_NUM, l_count, l_done, l_cancel);


  SET_LOCK(cb_lock);
  cb_count--;
  l_count = cb_count;
  //FIXME: This will only wake up 1 or more threads
  cb_cancel = 0;
  cb_nowork = 0;
  l_done = cb_done;
  ++stealStack[GET_THREAD_NUM]->wakeups;
  UNSET_LOCK(cb_lock);

  if (debug & 16)
    printf("Thread %d exit idle state, count = %d, done = %d\n",
           GET_THREAD_NUM, l_count, cb_done);

  return cb_done;
}

// causes one or more threads waiting at barrier, if any,
//  to be released
void cbarrier_cancel() {
  cb_cancel = 1;
}

void releaseNodes(StealStack *ss){
  if (doSteal) {
    if (ss_localDepth(ss) > 2 * chunkSize && ss->nNodes % release_interval == 0) {
      // Attribute this time to runtime overhead
      ss_setState(ss, SS_OVH);
      ss_release(ss, chunkSize);
      // This has significant overhead on clusters!
      //if (ss->nNodes % release_interval == 0) {
        ss_setState(ss, SS_CBOVH);
        cbarrier_cancel();
      //}

      ss_setState(ss, SS_WORK);
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

      ss_setState(ss, SS_WORK);

      /* examine node at stack top */
      parent = ss_top(ss);
      if (parent->numChildren < 0){
	// first time visited, construct children and place on stack
	genChildren(parent,&child,ss);

      }
      else {
	// second time visit, process accumulated statistics and pop
#ifdef UTS_STAT
        if (stats)
          updateParStat(parent);
#endif
        ss_pop(ss);
      }
      
      // release some nodes for stealing, if enough are available
      // and wake up quiescent threads
      releaseNodes(ss);
    }
		
    /* local work exhausted on this stack - resume tree search if able
     * to re-acquire work from shared portion of this thread's stack
     */
    if (ss_acquire(ss, chunkSize))
      continue;

    /* no work left in this thread's stack */
    /* try to steal work from another thread's stack */
    if (doSteal) {
      int goodSteal = 0;
      int victimId;
      
      ss_setState(ss, SS_SEARCH);
      victimId = findwork(chunkSize);
      while (victimId != -1 && !goodSteal) {
	// some work detected, try to steal it
	goodSteal = ss_steal(ss, victimId, chunkSize);
	if (!goodSteal)
	  victimId = findwork(chunkSize);
      }
      if (goodSteal)
	  continue;
    }
	
    /* unable to steal work from shared portion of other stacks -
     * enter quiescent state waiting for termination (done != 0)
     * or cancellation because some thread has made work available
     * (done == 0).
     */
    ss_setState(ss, SS_IDLE);
    done = cbarrier_wait();
  }
  
  /* tree search complete ! */
}

/* Pthreads ParTreeSearch Arguments */
struct pthread_args {
	StealStack *ss;
	int         id;
};

/* Pthreads ParTreeSearch Wrapper */
void * pthread_spawn_search(void *arg)
{
	pthread_setspecific(pthread_thread_num, &((struct pthread_args*)arg)->id);

	// Wait for the searc to start
	while (!start_search);

	parTreeSearch(((struct pthread_args*)arg)->ss);
	return NULL;
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
    char * name[4] = {"Sequential C", "C/OpenMP", "UPC", "SHMEM"};
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
    ind += sprintf(strBuf+ind, "Parallel search using %d processes, %d threads per process\n", comm_size, GET_NUM_THREADS);
    if (doSteal)
      ind += sprintf(strBuf+ind, "   Load balance by work stealing, chunk size = %d nodes, internode steal size = %d chunks\n",chunkSize, stealNchunks);
    else
      ind += sprintf(strBuf+ind, "   No load balancing.\n");
  }
  else
    ind += sprintf(strBuf+ind, "Sequential search\n");
      
  // random number generator
  ind += sprintf(strBuf+ind, "Random number generator: ");
  ind = rng_showtype(strBuf, ind);
  ind += sprintf(strBuf+ind, "\nCompute granularity: %d\n", computeGranularity);
  ind += sprintf(strBuf+ind, "   Release Interval: %d\n", release_interval);
  ind += sprintf(strBuf+ind, "   Polling Interval: %d us\n\n", pollint);

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


#ifdef TRACE
// print session records for each thread (used when trace is enabled)
void printSessionRecords()
{
  int i, j, k;
  double offset;

  for (i = 0; i < GET_NUM_THREADS; i++) {
    offset = startTime[i] - startTime[0];

    for (j = 0; j < SS_NSTATES; j++)
       for (k = 0; k < stealStack[i]->entries[j]; k++) {
          printf ("%d %d %f %f", i, j,
            stealStack[i]->md->sessionRecords[j][k].startTime - offset,
            stealStack[i]->md->sessionRecords[j][k].endTime - offset);
          if (j == SS_WORK)
            printf (" %d %ld",
              stealStack[i]->md->stealRecords[k].victimThread,
              stealStack[i]->md->stealRecords[k].nodeCount);
            printf ("\n");
     }
  }
}
#endif

// display search statistics
void showStats(double elapsedSecs) {
  int i, num_workers;
  int tnodes = 0, trel = 0, tacq = 0, tsteal = 0, tfail= 0;
  int mdepth = 0, mheight = 0;
  double twork = 0.0, tsearch = 0.0, tidle = 0.0, tovh = 0.0, tcbovh = 0.0;
  MPI_Status status;
  StealStack *mpi_ss;

  mpi_ss = (StealStack*)malloc(sizeof(StealStack)*comm_size);
  if (!mpi_ss)
	  ss_error("showStats(): out of memory\n");

  ss_init(&mpi_ss[comm_rank], MAXSTACKDEPTH);

  // Gather thread SS onto a single MPI SS
  for (i = 0; i < GET_NUM_THREADS; i++) {
    mpi_ss[comm_rank].nNodes   += stealStack[i]->nNodes;
    mpi_ss[comm_rank].nRelease += stealStack[i]->nRelease;
    mpi_ss[comm_rank].nAcquire += stealStack[i]->nAcquire;
    mpi_ss[comm_rank].nSteal   += stealStack[i]->nSteal;
    mpi_ss[comm_rank].nFail    += stealStack[i]->nFail;
    mpi_ss[comm_rank].time[SS_WORK]   += 0;
    mpi_ss[comm_rank].time[SS_SEARCH] += 0;
    mpi_ss[comm_rank].time[SS_IDLE]   += 0;
    mpi_ss[comm_rank].time[SS_OVH]    += 0;
    mpi_ss[comm_rank].time[SS_CBOVH]  += 0;
//    mpi_ss[comm_rank].twork   += stealStack[i]->time[SS_WORK];
//    mpi_ss[comm_rank].tsearch += stealStack[i]->time[SS_SEARCH];
//    mpi_ss[comm_rank].tidle   += stealStack[i]->time[SS_IDLE];
//    mpi_ss[comm_rank].tovh    += stealStack[i]->time[SS_OVH];
//    mpi_ss[comm_rank].tcbovh  += stealStack[i]->time[SS_CBOVH];
    mpi_ss[comm_rank].maxStackDepth = max(mpi_ss[comm_rank].maxStackDepth, stealStack[i]->maxStackDepth);
    mpi_ss[comm_rank].maxTreeDepth  = max(mpi_ss[comm_rank].maxTreeDepth, stealStack[i]->maxTreeDepth);
  }

  num_workers = comm_size;

  /* Gather the stats and return if I'm not the one that has them */
  if (comm_rank > 0) {
    MPI_Send(&mpi_ss[comm_rank], sizeof(StealStack), MPI_BYTE, 0, MPIWS_STATS, MPI_COMM_WORLD);
    return;
  } else {
    for(i = 1; i < num_workers; i++) {
      MPI_Recv(&(mpi_ss[i]), sizeof(StealStack), MPI_BYTE, i, MPIWS_STATS, MPI_COMM_WORLD, &status);
    }
  }

  // combine measurements from all threads
  for (i = 0; i < num_workers; i++) {
    tnodes  += mpi_ss[i].nNodes;
    trel    += mpi_ss[i].nRelease;
    tacq    += mpi_ss[i].nAcquire;
    tsteal  += mpi_ss[i].nSteal;
    tfail   += mpi_ss[i].nFail;
    twork   += mpi_ss[i].time[SS_WORK];
    tsearch += mpi_ss[i].time[SS_SEARCH];
    tidle   += mpi_ss[i].time[SS_IDLE];
    tovh    += mpi_ss[i].time[SS_OVH];
    tcbovh  += mpi_ss[i].time[SS_CBOVH];
    mdepth   = max(mdepth, mpi_ss[i].maxStackDepth);
    mheight  = max(mheight, mpi_ss[i].maxTreeDepth);
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
    printf("Wall clock time = %.3f sec, performance = %.0f nodes/sec (%.0f nodes/sec per process)\n", 
           elapsedSecs, (tnodes / elapsedSecs), (tnodes / elapsedSecs / num_workers));
/*    printf("Wall clock time = %.3f sec, performance = %.0f nodes/sec (%.0f nodes/sec per thread)\nTotal work time = %.3f sec\n", 
           elapsedSecs, (tnodes / elapsedSecs), 
           (tnodes / elapsedSecs / num_workers), twork);
    printf("Avg time per thread: Work = %.6f, Overhead = %6f, CB_Overhead = %6f, Search = %.6f, Idle = %.6f.\n\n", (twork / num_workers),
           (tovh / num_workers), (tcbovh/num_workers), (tsearch / num_workers), (tidle / num_workers));
*/  }

  // summary execution info for machine consumption
  if (verbose == 0) {
    printf("%3d %7.3f %9d %7.0f %7.0f %7.3f %d %d %d %d %.2f %d %d %1d %f %3d %1d %1d\n",
           num_workers, elapsedSecs, tnodes, 
           tnodes/elapsedSecs, (tnodes/elapsedSecs)/num_workers, twork,
           chunkSize, tsteal, tfail,  
           type, b_0, rootId, gen_mx, shape_fn, nonLeafProb, nonLeafBF, 
           COMPILER_TYPE, RNG_TYPE);
  }

  // per thread execution info
  if (verbose > 1) {
    for (i = 0; i < num_workers; i++) {
      printf("** Process %d (%d threads)\n", i, GET_NUM_THREADS);
      printf("  # nodes explored    = %d\n", mpi_ss[i].nNodes);
      printf("  # chunks released   = %d\n", mpi_ss[i].nRelease);
      printf("  # chunks reacquired = %d\n", mpi_ss[i].nAcquire);
      printf("  # chunks stolen     = %d\n", mpi_ss[i].nSteal);
      printf("  # failed steals     = %d\n", mpi_ss[i].nFail);
      printf("  maximum stack depth = %d\n", mpi_ss[i].maxStackDepth);
/*      printf("  work time           = %.6f secs (%d sessions)\n",
             mpi_ss[i].time[SS_WORK], mpi_ss[i].entries[SS_WORK]);
      printf("  overhead time       = %.6f secs (%d sessions)\n",
             mpi_ss[i].time[SS_OVH], mpi_ss[i].entries[SS_OVH]);
      printf("  search time         = %.6f secs (%d sessions)\n",
             mpi_ss[i].time[SS_SEARCH], mpi_ss[i].entries[SS_SEARCH]);
      printf("  idle time           = %.6f secs (%d sessions)\n",
             mpi_ss[i].time[SS_IDLE], mpi_ss[i].entries[SS_IDLE]);
*/      printf("  wakeups             = %d, false wakeups = %d (%.2f%%)",
             mpi_ss[i].wakeups, mpi_ss[i].falseWakeups,
             (mpi_ss[i].wakeups == 0) ? 0.00 : ((((double)mpi_ss[i].falseWakeups)/mpi_ss[i].wakeups)*100.0));
      printf("\n");
    }
  }

  // Dump to CSV - Comma separated list.  Can be opened in most spreadsheet apps.
  if (csv_filename[0] != '\0') {
	  FILE *csvfile = fopen(&csv_filename[0], "w");
	  if (! csvfile) perror("Error opening csv file");
	  else {
  		// The first row is a header.  Print that first.
  		// If you change anything below, be sure to update the header
  		fprintf(csvfile, "Total PEs, Chunk Size, Tree Size, Max Tree Depth, Chunks Released, Chunks Reacquired, Chunks Stolen, Failed Steals, Max Stack Size, Wallclock Time, Avg. Work Time, Avg. Overhead Time, Avg. CB Overhead Time, Avg. Search Time, Avg. Idle Time\n");
      fprintf(csvfile, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %0f, %0f, %0f, %0f, %0f, %0f\n",
   		  num_workers, chunkSize, tnodes, mheight, trel, tacq, tsteal, tfail, mdepth, elapsedSecs,
		  twork/num_workers, tovh/num_workers, tcbovh/num_workers, tsearch/num_workers, tidle/num_workers);

      if (fclose(csvfile))
        perror("Closing the csv file failed");
	  }
  }

  #ifdef TRACE
    printSessionRecords();
  #endif

  // tree statistics output to stat.txt, if requested
#ifdef UTS_STAT
  if (stats) {
    FILE *fp;
    char * tmpstr;
    char strBuf[5000];
    
    fp = fopen("stat.txt", "a+w");
    fprintf(fp, "\n------------------------------------------------------------------------------------------------------\n");
    showParametersStr(strBuf);
    fprintf(fp, "%s\n", strBuf);
    
    fprintf(fp, "\nTotal nodes = %d\n", totalNodes); 
    fprintf(fp, "Max depth   = %d\n\n", maxHeight); 
    fprintf(fp, "Tseng ImbMeasure(overall)\n max:\t\t%lf \n avg:\t\t%lf \n devMaxAvg:\t %lf\n normDevMaxAvg: %lf\t\t\n\n", 
            imb_max/totalNodes, imb_avg/totalNodes, imb_devmaxavg/totalNodes, 
            imb_normdevmaxavg/totalNodes);
    
    switch (unbType){
    case 0: tmpstr = "(min imb weighted by size)"; break;
    case 1: tmpstr = "(min imb not weighted by size)"; break;
    case 2: tmpstr = "(max imb not weighted by size)"; break;
    default: tmpstr = "(?unknown measure)"; break;
    }
    fprintf(fp, "ImbMeasure:\t%s\n Overall:\t %lf\n Max:\t\t%lf\n Min:\t\t%lf\n\n", 
            tmpstr, treeImb, minImb, maxImb);
    showHist(fp);
    fprintf(fp, "\n------------------------------------------------------------------------------------------------------\n\n\n");
    fclose(fp);
  }
#endif
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
    case 'C':
      stealNchunks = atoi(argv[i+1]); break;
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
    case 'u':
      unbType = atoi(argv[i+1]); 
      if (unbType == -1)
        stats = 0;
      break;
    case 'a':
      shape_fn = atoi(argv[i+1]); break;
    case 'b':
      b_0 = atof(argv[i+1]); break;
    case 'd':
      gen_mx = atoi(argv[i+1]); break;
    case 'f':
      shiftDepth = atof(argv[i+1]); break;
    case 'g':
      computeGranularity = max(1,atoi(argv[i+1])); break;
    case 'l':
      strncpy(&csv_filename[0], argv[i+1], 256); break;
    case 'i':
      release_interval = atoi(argv[i+1]); break;
    case 'I':
      pollint = atoi(argv[i+1]); break;
    case 'T':
      pthread_num_threads = atoi(argv[i+1]);
      if (pthread_num_threads > MAXTHREADS) {
        printf("Warning: Requested threads > MAXTHREADS.  Truncated to %d threads\n", MAXTHREADS);
        pthread_num_threads = MAXTHREADS;
      }
      break;
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
    printf(" -g  int   compute granularity: number of rng_spawns per node\n");
    printf(" -u  int   unbalance measure (-1: none; 0: min/size; 1: min/n; 2: max/n)\n");
    printf(" -i  int   set intranode release interval\n");
    printf(" -I  int   set master polling interval\n");
    printf(" -T  int   set number of intranode threads\n");
    printf(" -C  int   set number of chunks stolen in an internode steal\n");
    printf(" -v  int   nonzero to set verbose output\n");
    printf(" -l  file  output stats as a comma delimited list to \'file\'\n");
    printf(" -x  int   debug level\n");
    exit(4);
  }
}

// NB:  run in parallel under UPC, single thread under openMP
void resolveParameterConflicts() {
  
  // no work stealing if running sequentially
  if (PARALLEL == 0)
    doSteal = 0;

  // no imbalance statistics if running in parallel
  if (PARALLEL){
    unbType = -1;
    stats = 0;
  }
}

/************************************************************************/
/* Hierarchical MPI code                                                */
/************************************************************************/


/** Make progress on any outstanding WORKREQUESTs or WORKRESPONSEs */
void ws_make_progress()
{
	MPI_Status  status;
	int         flag, index, victimId, goodSteal;
	int         steal_size = chunkSize * stealNchunks;
	int         i;
	void       *work;
	StealStack  ss;
	
	// Test for incoming work_requests
	MPI_Test(&wrin_request, &flag, &status);

	if (flag) {
		// Got a work request
		++ctrl_recvd;
		
		/* Repost that work request listener */
		MPI_Irecv(&wrin_buff, 1, MPI_LONG, MPI_ANY_SOURCE, MPIWS_WORKREQUEST, MPI_COMM_WORLD,
			       	&wrin_request);

		index = status.MPI_SOURCE;

		/* Check if we have any surplus work */
		//DEBUG(DBG_CHUNK, printf(" -Thread %d: Processing steal request from process %d\n", comm_rank, index));

		// Create a dummy stealstack
		// FIXME: We can make this static
		ss.stack_g = (SHARED_INDEF Node *) ALLOC (2*steal_size*sizeof(Node));
		ss.stack = (Node *) ss.stack_g;
		ss.stackSize = 2*steal_size;
		ss.nNodes = 0;
		ss.sharedStart = 0;
		ss.local  = 0;
		ss.top    = 0;
		ss.workAvail = 0;

		goodSteal = 0;
		victimId  = -1;
		for (i = 0; i < GET_NUM_STACKS; i++) {
			if (stealStack[i]->workAvail >= steal_size) {
				victimId = i;
				break;
			}
		}
		if (victimId != -1) {
			// some work detected, try to steal it
			goodSteal = ss_steal(&ss, victimId, steal_size);
		}
		if (goodSteal) {
			work = ss.stack;
			DEBUG(DBG_CHUNK, printf(" -Thread %d: Releasing a chunk to thread %d\n", comm_rank, index));
			++chunks_sent;
			MPI_Send(work, steal_size*sizeof(Node), MPI_BYTE, index,
				MPIWS_WORKRESPONSE, MPI_COMM_WORLD);

			/* If we a node to our left steals from us, our color becomes black */
			if (index < comm_rank) my_color = BLACK;
		} else {
			// Send a "no work" response
			++ctrl_sent;
			MPI_Send(NULL, 0, MPI_BYTE, index, MPIWS_WORKRESPONSE, MPI_COMM_WORLD);
		}

		free(ss.stack_g);
	}
	
	return;
}

/**
 * ensure local work exists, find it if it doesnt
 *  returns process id where work is stolen from if no can be found locally
 *  returns -1 if no local work exists and none could be stolen
 **/
int ensureLocalWork()
{
	MPI_Status  status;
	int         flag, i;
	int         steal_size = chunkSize * stealNchunks;

	/* If no more work */
	while (cb_nowork) {
		//if (my_color == PINK)
		//	ss_setState(s, SS_IDLE);
		//else
		//	ss_setState(s, SS_SEARCH);
		
		/* Check if we should post another steal request */
		if (wrout_request == MPI_REQUEST_NULL && my_color != PINK) {
			if (iw_buff == NULL) {
				iw_buff = malloc(steal_size*sizeof(Node));
				if (!iw_buff)
					ss_error("ensureLocalWork(): Out of memory\n");
			}
		
			/* Send the request and wait for a work response */
			last_steal = (last_steal + 1) % comm_size;
			if (last_steal == comm_rank) last_steal = (last_steal + 1) % comm_size;

			DEBUG(DBG_CHUNK, printf("Thread %d: Asking thread %d for work\n", comm_rank, last_steal));

			++ctrl_sent;

			MPI_Isend(&wrout_buff, 1, MPI_LONG, last_steal, MPIWS_WORKREQUEST, MPI_COMM_WORLD, &wrout_request);
			MPI_Irecv(iw_buff, steal_size*sizeof(Node), MPI_BYTE, last_steal, MPIWS_WORKRESPONSE,
					MPI_COMM_WORLD, &iw_request);
		}

		// Call into the stealing progress engine and update our color
		ws_make_progress();
	
		// Test for incoming work
		MPI_Test(&iw_request, &flag, &status);

		if (flag && wrout_request != MPI_REQUEST_NULL) {
			int work_rcv;
			int victim;

			MPI_Get_count(&status, MPI_BYTE, &work_rcv);
		
			if (work_rcv > 0) {
				++chunks_recvd;
				DEBUG(DBG_CHUNK, printf(" -Thread %d: Incoming Work received, %d bytes\n", comm_rank, work_rcv));
	
				if (work_rcv != steal_size*sizeof(Node)) {
					ss_error("ws_make_progress(): Work received size does not equal chunk size");
				}
	
				//ctrk_get(comm_rank, node->work);
	
				/* Push stolen work onto the back of the queue */
				victim = GET_NUM_STACKS-1;
				SET_LOCK(stealStack[victim]->stackLock);

				memcpy(&(stealStack[victim]->stack[stealStack[victim]->sharedStart + stealStack[victim]->workAvail]),
					iw_buff, steal_size*sizeof(Node));

				// Update start of "local" region
				stealStack[victim]->local += steal_size;
				// Update end of "shared" region
				stealStack[victim]->workAvail += steal_size;

				UNSET_LOCK(stealStack[victim]->stackLock);

				// FIXME: Maybe not lock here?
				SET_LOCK(cb_lock);
				cb_nowork = 0;
				UNSET_LOCK(cb_lock);
				cbarrier_cancel();
				//deq_pushBack(localQueue, node);
#ifdef TRACE
				/* Successful Steal */
				ss_markSteal(s, status.MPI_SOURCE);
#endif
			}
			else {
				// Received "No Work" message
				++ctrl_recvd;
			}
	
			// Clear on the outgoing work_request
			MPI_Wait(&wrout_request, &status);
		}
	
		/* Test if we have the token */
		MPI_Test(&td_request, &flag, &status);

		if (flag) {
			enum colors next_token;
			int         forward_token = 1;
		
			DEBUG(DBG_TOKEN, printf("ensureLocalWork(): Thread %d received %s token\n", comm_rank, color_names[td_token.color]));
			switch (td_token.color) {
				case WHITE:
					if (cb_nowork) {
						int cb_nowork_l;
						SET_LOCK(cb_lock);
						cb_nowork_l = cb_nowork;
						UNSET_LOCK(cb_lock);

						// Double Check after locking
						if (!cb_nowork_l) {
							printf("False abort");
							forward_token = 0;
						} else {

						if (comm_rank == 0 && my_color == WHITE) {
							if (td_token.recv_count != td_token.send_count) {
							// There are outstanding messages, try again
								DEBUG(DBG_MSGCNT, printf(" TD_RING: In-flight work, recirculating token\n"));
								my_color = WHITE;
								next_token = WHITE;
							} else {
							// Termination detected, pass RED token
								my_color = PINK;
								next_token = PINK;
							}
						} else if (my_color == WHITE) {
							next_token = WHITE;
						} else {
							// Every time we forward the token, we change
							// our color back to white
							my_color = WHITE;
							next_token = BLACK;
						}

						// forward message
						forward_token = 1;
					}}
					else {
						forward_token = 0;
					}
					break;
				case PINK:
					if (comm_rank == 0) {
						if (td_token.recv_count != td_token.send_count) {
							// There are outstanding messages, try again
							DEBUG(DBG_MSGCNT, printf(" TD_RING: ReCirculating pink token nr=%ld ns=%ld\n", td_token.recv_count, td_token.send_count));
							my_color = PINK;
							next_token = PINK;
						} else {
							// Termination detected, pass RED token
							my_color = RED;
							next_token = RED;
						}
					} else {
						my_color = PINK;
						next_token = PINK;
					}

					forward_token = 1;
					break;
				case BLACK:
					// Non-Termination: Token must be recirculated
					if (comm_rank == 0)
						next_token = WHITE;
					else {
						my_color = WHITE;
						next_token = BLACK;
					}
					forward_token = 1;
					break;
				case RED:
					// Termination: Set our state to RED and circulate term message
					my_color = RED;
					next_token = RED;

					if (comm_rank == comm_size - 1)
						forward_token = 0;
					else
						forward_token = 1;
					break;
			}

			/* Forward the token to the next node in the ring */
			if (forward_token) {
				td_token.color = next_token;

				/* Update token counters */
				if (comm_rank == 0) {
					if (td_token.color == PINK) {
						td_token.send_count = ctrl_sent;
						td_token.recv_count = ctrl_recvd;
					} else {
						td_token.send_count = chunks_sent;
						td_token.recv_count = chunks_recvd;
					}
				} else {
					if (td_token.color == PINK) {
						td_token.send_count += ctrl_sent;
						td_token.recv_count += ctrl_recvd;
					} else {
						td_token.send_count += chunks_sent;
						td_token.recv_count += chunks_recvd;
					}
				}

				DEBUG(DBG_TOKEN, printf("ensureLocalWork(): Thread %d forwarding %s token\n", comm_rank, color_names[td_token.color]));
				MPI_Send(&td_token, sizeof(td_token_t), MPI_BYTE, (comm_rank+1)%comm_size,
					MPIWS_TDTOKEN, MPI_COMM_WORLD);

				if (my_color != RED) {
					/* re-Post termination detection listener */
					int j = (comm_rank == 0) ? comm_size - 1 : comm_rank - 1; // Receive the token from the processor to your left
					MPI_Irecv(&td_token, sizeof(td_token_t), MPI_BYTE, j, MPIWS_TDTOKEN, MPI_COMM_WORLD, &td_request);
				}
			}

			if (my_color == RED) {
				// Clean up outstanding requests.
				// This is safe now that the pink token has mopped up all outstanding messages.
				MPI_Cancel(&wrin_request);
				if (iw_request != MPI_REQUEST_NULL)
					MPI_Cancel(&iw_request);
				// Terminate
				cb_done = 1;
				return -1;
			}
		}
		
	
	}

	return 0;  // Local work exists
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

  MPI_Init(&argc, &argv);

  MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
  MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);

  if (comm_rank == 0)
    printf("HWS: MPI Initialized with %d PEs\n", comm_size);
   
/* determine benchmark parameters */
  parseParameters(argc,argv);
  resolveParameterConflicts();

#ifdef UTS_STAT
  if (stats)
    initHist();
#endif  

  // Start searching for work at the next thread to our right
  wrout_request = MPI_REQUEST_NULL;
  wrout_buff    = chunkSize;
  last_steal    = comm_rank;
  iw_request    = MPI_REQUEST_NULL;
  iw_buff       = NULL; // Allocated on demand
  chunks_sent   = 0;
  chunks_recvd  = 0;
  ctrl_sent     = 0;
  ctrl_recvd    = 0;

  // Termination detection
  my_color       = WHITE;
  td_token.color = BLACK;

  // Setup non-blocking recieve for recieving shared work requests
  MPI_Irecv(&wrin_buff, 1, MPI_LONG, MPI_ANY_SOURCE, MPIWS_WORKREQUEST, MPI_COMM_WORLD, &wrin_request);

  /* Set up the termination detection receives */	
  if (comm_rank == 0) {
    // Thread 0 initially has a black token
    td_request = MPI_REQUEST_NULL;
  } else {
    /* Post termination detection listener */
    int j = (comm_rank == 0) ? comm_size - 1 : comm_rank - 1; // Receive the token from the processor to your left

    MPI_Irecv(&td_token, sizeof(td_token_t), MPI_BYTE, j, MPIWS_TDTOKEN, MPI_COMM_WORLD,
      &td_request);
  }

  /* cancellable barrier initialization (single threaded under OMP) */
  cb_init();


/********** PTHEADS Parallel Region **********/
  {
    double t1, t2, et;
    int i, err;
    void *rval;
    struct pthread_args *args;
    pthread_t *thread_ids;

    if (comm_rank == 0)
      showParameters();

    /* allocate stealstacks */
    for (i = 0; i < GET_NUM_STACKS; i++) {
      stealStack[i] = ALLOC (sizeof(StealStack));
      ss_init(stealStack[i], MAXSTACKDEPTH);
    }

    if (comm_rank == 0) {
      /* initialize root node and push on thread 0 stack */
      initRootNode(&root, type);
      ss_push(stealStack[0], &root);
    }

    thread_ids = malloc(sizeof(pthread_t)*GET_NUM_THREADS);
    args = malloc(sizeof(struct pthread_args)*GET_NUM_THREADS);
    pthread_key_create(&pthread_thread_num, NULL);

    start_search = 0;

    for (i = 0; i < GET_NUM_THREADS; i++) {
      ss_initState(stealStack[i]);
      args[i].ss = stealStack[i];
      args[i].id = i;
      //printf("Master: Creating thread %d\n", i);
      err = pthread_create(&thread_ids[i], NULL, pthread_spawn_search, (void*)&args[i]);
      if (err != 0) {
        printf("FATAL: Error spawning thread %d\n", err);
        exit(1);
      }
    }

    /***** BEGIN MASTER THREAD *****/
    MPI_Barrier(MPI_COMM_WORLD);
    start_search = 1;
    t1 = wctime();

    while (!cb_done) {
      ws_make_progress();
      if (cb_count > 0) {
        // Generate steal request
        ensureLocalWork();
      }
      // FIXME: Call usleep here instead.  Polling interval should be a parameter...
      //pthread_yield();
      usleep(pollint);
    }

    t2 = wctime();
    et = t2 - t1;
    //printf("MASTER: Detected Termination\n");
    //fflush(NULL);
    /*****  END MASTER THREAD  *****/

    for (i = 0; i < GET_NUM_THREADS; i++) {
      //printf("Master: Waiting for thread %d\n", i);
      pthread_join(thread_ids[i], &rval);
    }

//#ifdef TRACE
//      startTime[GET_THREAD_NUM] = t1;
//      ss->md->sessionRecords[SS_IDLE][ss->entries[SS_IDLE] - 1].endTime = t2;
//#endif

    showStats(et);
  }

/********** End Parallel Region **********/

  MPI_Finalize();
  return 0;
}
