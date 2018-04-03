#ifndef __FFCOLL_PORTALS_H__
#define __FFCOLL_PORTALS_H__
//#include <infiniband/verbs.h>
//Portals
//#include <portals4.h>

#define FF_CHECK_RES
#define FF_ALLOCATOR


#ifdef FF_CHECK_RES
#define FF_CHECK_NULL(X) { if ((X)==NULL) {printf("FFCOLL (%s:%d): error in " #X " (NULL)\n", __FILE__, __LINE__); return FF_FAIL;} }
#define FF_PCALL(X) {int rval; if ((rval=X)!=PTL_OK) { printf("FFCOLL: error %i (%s:%d) on %s\n", rval, __FILE__, __LINE__, #X); exit(-1); return FF_FAIL;} }
#define FF_CHECK_FAIL(X) { if ((X)==FF_FAIL) {printf("FFCOLL: error in " #X " (FAIL)\n"); return FF_FAIL;} }
#else
#define FF_CHECK_NULL(X) X
#define FF_PCALL(X) X
#define FF_CHECK_FAIL(X) X
#endif
//#define DEBUG 1




#define FF_QUICK(X) { ff_schedule_h _sched = X; ff_schedule_post(_sched, 1); ff_schedule_wait(_sched); ff_schedule_free(_sched); }

//#define FF_ONE_CT
#define FF_BUSY_WAITING
#define FF_EXPERIMENTAL
#define FF_MEM_REUSE

#define FF_STAT

#define FF_RNDVZ_THRESHOLD 65536
#define FF_UQ_LENGTH 131072
//#define FF_UQ_LENGTH FF_RNDVZ_THRESHOLD*2
#define FF_UQ_SIZE 100
#define EQ_COUNT 1024


#ifdef FF_MEM_REUSE
#define FF_STORAGE_PRE_OP 1024
#endif


#define FF_SUCCESS 0
#define FF_FAIL -1
#define FF_EMPTY -2


//Max indeg of a node in a CDAG. TODO: transform in a linked list!!!
#define FF_MAX_DEPENDECIES 5
#define FF_MAX_OP_PER_SCHED 50

#define FF_SCHED_CACHED 2

#define FF_OP_NONE 0

//ff_op options
#define FF_DEP_OR       1
#define FF_DONT_WAIT    2
#define FF_SHADOW_TAG   4
#define FF_BUF_DEP      8
#define FF_REUSE        16

/*== operators and types are defined as in portals4.h ==*/
#define FF_MIN       PTL_MIN
#define FF_MAX       PTL_MAX
#define FF_SUM       PTL_SUM
#define FF_PROD      PTL_PROD

#define FF_INT8_T   PTL_INT8_T
#define FF_UINT8_T  PTL_UINT8_T
#define FF_INT16_T  PTL_INT16_T
#define FF_UINT16_T PTL_UINT16_T
#define FF_INT32_T  PTL_INT32_T
#define FF_UINT32_T PTL_UINT32_T
#define FF_UINT64_T PTL_UINT64_T
#define FF_FLOAT    PTL_FLOAT
#define FF_DOUBLE   PTL_DOUBLE

#define FF_BUFFER_REUSE 1


typedef uint64_t ff_peer;
//typedef char * ff_buff;
typedef ptl_rank_t ff_rank_t;
typedef unsigned int ff_size_t;
typedef unsigned int ff_operator_t;
typedef unsigned int ff_datatype_t;

/*typedef struct __ffcoll_ctr{
    ptl_handle_ct_t * ctr;
}ff_ctr;*/


typedef unsigned int ff_tag;

typedef unsigned int ff_op_h;
typedef unsigned int ff_schedule_h;
typedef unsigned int ff_container_h;

typedef struct _ff_sched_info{
    //TODO: this should be a set of key/value pairs.
    //Currently is the union of the parameters of the implemented
    //collective operations. They are used by the clone function.
    int count;
    int root;
    ff_size_t unitsize;
    ff_tag tag;
    void * userdata;
    ff_operator_t _operator;
    ff_datatype_t datatype;
}ff_sched_info;

int ff_init(int *argc, char *** argv);
int ff_finalize();

ff_rank_t ff_get_rank();
ff_size_t ff_get_size();

ff_op_h ff_op_create_send(void * buff, ff_size_t length, ff_peer peer, ff_tag tag);
ff_op_h ff_op_create_recv(void * buff, ff_size_t length, ff_peer peer, ff_tag tag);

ff_op_h ff_op_create_computation(void * buff1, ff_size_t length1, void * buff2, ff_size_t length2, ff_operator_t _operator, ff_datatype_t datatype, ff_tag tag);

int ff_op_post(ff_op_h op);
int ff_op_repost(ff_op_h op);
int ff_op_reset(ff_op_h op);
int ff_op_wait(ff_op_h op);
int ff_op_hb(ff_op_h after, ff_op_h before);
int ff_op_free(ff_op_h op);
int ff_op_is_executed(ff_op_h oph);
int ff_op_setopt(ff_op_h oph, int options);
int ff_op_test(ff_op_h oph);

ff_op_h ff_op_nop_create(int options);

ff_schedule_h ff_schedule_create(unsigned int options, void * sendbuff, void * rcvbuff);
int ff_schedule_add(ff_schedule_h schedh, ff_op_h oph);
int ff_schedule_post(ff_schedule_h sched, int start);
int ff_schedule_repost(ff_schedule_h sched);
int ff_schedule_wait(ff_schedule_h schedh);
int ff_schedule_test(ff_schedule_h schedh);
int ff_schedule_free(ff_schedule_h schedh);
int ff_schedule_set_indep(ff_schedule_h schedh, ff_op_h oph);
int ff_schedule_set_user_dep(ff_schedule_h schedh, ff_op_h oph);
int ff_schedule_satisfy_user_dep(ff_schedule_h);
int ff_schedule_get_buffers(ff_schedule_h schedh, void ** sndbuff, void ** rcvbuff);
int ff_schedule_start(ff_schedule_h schedh);
int ff_schedule_trylock(ff_schedule_h schedh);
int ff_schedule_unlock(ff_schedule_h schedh);
int ff_schedule_settmpbuff(ff_schedule_h schedh, void * buff);

//ff_container_h ff_container_create(int root, int count, ff_size_t unitsize, ff_tag tag, ff_schedule_h (*create_schedule)(ff_sched_info));
ff_container_h ff_container_create(ff_sched_info info, ff_schedule_h (*create_schedule)(ff_sched_info));
int ff_container_increase_async(ff_container_h containerh);
ff_schedule_h ff_container_wait(ff_container_h containerh);
ff_schedule_h ff_container_get_next(ff_container_h containerh);
int ff_container_start(ff_container_h containerh);
int ff_container_trylock(ff_container_h containerh);
int ff_container_unlock(ff_container_h containerh);
int ff_container_flush(ff_container_h containerh);
ff_schedule_h ff_container_get_head(ff_container_h containerh);

#ifdef FF_STAT
int ff_schedule_stat(ff_schedule_h schedh, int * ops, int * counters);
#endif

//used only for test purposes
int ff_init_barrier();

//struct ibv_pd * ff_get_pd();


//int ffcoll_pos


/* Only for debug purposes is here. It should be in ff_impl.h. */
void ff_eq_poll();



void * ff_malloc(size_t size);
void ff_free(void * ptr);

#endif //__FFCOLL_PORTALS_H__
