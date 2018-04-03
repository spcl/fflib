/* Copyright (c) 2014 ETH - Eidgenössische Technische Hochschule Zürich
 * Scalable and Parallel Computing Lab - (http://spcl.inf.ethz.ch/)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer listed
 *    in this license in the documentation and/or other materials
 *     provided with the distribution.
 * 
 *  - Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * The copyright holders provide no reassurances that the source code
 * provided does not infringe any patent, copyright, or any other
 * intellectual property rights of third parties.  The copyright holders
 * disclaim any liability to any recipient for claims brought against
 * recipient by any third party for infringement of that parties
 * intellectual property rights.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors
 * ========
 * Salvatore Di Girolamo <digirols@inf.ethz.ch>
 */


#ifndef __FFCOLL_PORTALS_IMPL_H__
#define __FFCOLL_PORTALS_IMPL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portals4.h>
#include "ff.h"


#define FF_OP_LENGTH 2048

#define CTRL_MSG_MASK   0x0FFFFFFFFFFFFFFFULL
#define CTRL_MSG_MATCH  0x1000000000000000ULL
#define CTRL_RTS_MSG    0x1100000000000000ULL
#define CTRL_RTR_MSG    0x1200000000000000ULL
#define DATA_MSG        0X2000000000000000ULL
#define COMP_MSG        0X2000000000000000ULL
#define SHADOW_TAG      0x0010000000000000ULL

#ifdef DEBUG
#define PDEBUG(X) X
#else
#define PDEBUG(X)
#endif


typedef struct _ffcoll_op ff_op;
typedef struct _ffcoll_schedule ff_schedule;
typedef struct _ffcoll_buffer ff_buffer;
typedef struct _ffcoll_container ff_container;

ptl_handle_ni_t ni_logical;
ptl_handle_eq_t eventqueue;
ptl_pt_index_t logical_pt_index;
ptl_process_t myself;


typedef unsigned int ff_handle;

struct _ffcoll_op{
    ptl_handle_any_t handle;

    enum {SEND=0, RECEIVE, COMPUTATION, NOP} type;
    //Associated counter
    ptl_handle_ct_t ct;
    ptl_handle_ct_t depct;

    int options;
    //Dependencies
    ff_op * dependency[FF_MAX_DEPENDECIES];
    int depcount;

    int locked;

    ff_peer peer;
    ff_tag tag;

    void * buff;
    ff_size_t length;

    //ff_op * nextdep;
    //Used for handlers
    ff_op * next;
    unsigned int id;

    //schedule
    ff_schedule_h sched;

    //operator and datatype
    ff_operator_t _operator;
    ff_datatype_t datatype;

    //This is needed since the PtlTriggeredMEAppend requires
    //that the ptl_me_t is kept alive at least until the append is triggered
    //This is ugly. The information are redundant. Should be solved.
    ptl_me_t data_me;

    //ptl_md_t __md;

    ptl_handle_any_t md;
    ptl_handle_ct_t _depct;
    ptl_handle_me_t me;


#ifdef FF_ONE_CT
    int outdegree;
#endif

#ifdef FF_EXPERIMENTAL
    ptl_handle_ct_t rndvct;
#endif

#ifdef FF_MEM_REUSE
    ff_op * trash_next;
    ff_handle ffhandle;
#endif

};



struct _ffcoll_container{
    //ff_schedule schedules[FF_MAX_ASYNCH_DEGREE];
    int adegree;
    ff_schedule * head;
    ff_schedule * tail;
    ff_schedule * current;
    int started;
    

    //TODO: this should be a set of key/value pairs.
    //Currently is the union of the parameters of the implemented
    //collective operations. They are used by the clone function.
    ff_sched_info info;
/*    int count;
    int root;
    ff_size_t unitsize;
    ff_tag tag;
*/
    //This user-provided function clones the current schedule.
    //It must create a scheudle with the same functions but different buffers.
    ff_schedule_h (*create_schedule)(ff_sched_info);
};




struct _ffcoll_schedule{
    unsigned int options;
    unsigned int length;
    ff_op * ops[FF_MAX_OP_PER_SCHED];

    ff_schedule_h handle;

    void * rcvbuffer;
    void * sndbuffer;


    int is_user_dep;

    void * tmpbuff;
/*
    ff_peer root;
    int count;
    ff_size_t unitsize;
    ff_tag tag;

*/
    int id; //for debug. to remove. look also at ff_create_scheduel

    //The schedule is started when startct reaches 1
    //(i.e. the "indipendent" operations depend on startct)
    ff_op_h startop; //it's a fake op: it's used only for the counter
    ff_op_h in_use; //the counter is 1 if it is in use
    ff_op_h user_dep; //all the actions that require user intervention (e.g. send buffer ready) depends on this.

    ff_schedule * next;

#ifdef FF_ONE_CT
    ff_op_h waitop;
#endif

#ifdef FF_MEM_REUSE
    ff_schedule * trash_next;
#endif

#ifdef FF_STAT
    /* Count the additional counters (i.e., the one introduces for multi-deps). 
       The total number of used counters is sched->length + sched->_ct_count. */
    unsigned int _ct_count;
#endif
};



struct hentry *allocated;// = NULL;
int ids;// = 1;

#ifdef FF_MEM_REUSE
ff_op * ops;
ff_schedule * schedules;
void * assigned_mem;
#endif

int ff_post_receive(ff_op * op, ptl_handle_ct_t depct, int threshold);
int ff_repost_receive(ff_op * op);

int ff_post_send(ff_op * op, ptl_handle_ct_t depct, int threshold);
int ff_repost_send(ff_op * op);

int ff_post_computation(ff_op * comp, ptl_handle_ct_t depct, int threshold);
void ff_complete_receive(ptl_event_t event);

int _ff_op_reset(ff_op * op);
int _ff_op_repost(ff_op * op);
int _ff_op_post(ff_op * op);
int _ff_op_wait(ff_op * op);
int _ff_op_test(ff_op * op);
int _ff_op_unlock(ff_op * op);
int _ff_op_try_lock(ff_op * op);
int _ff_op_free(ff_op * op);
int _ff_op_hb(ff_op * before, ff_op * after);
int ff_op_try_lock(ff_op_h oph);
int ff_op_unlock(ff_op_h oph);


int _ff_schedule_wait(ff_schedule * sched);
int _ff_schedule_test(ff_schedule * sched);
int _ff_schedule_trylock(ff_schedule * sched);
int _ff_schedule_unlock(ff_schedule * sched);
int _ff_schedule_free(ff_schedule * sched);

void ff_op_init();
ff_op_h ff_op_create(ff_op ** op);


int append_overflow(int i);
//int append_overflow();


void ff_print_events();


#endif //__FFCOLL_PORTALS_IMPL_H__
