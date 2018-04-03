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

#include <string.h> //memcpy
#include <math.h>
#include <portals4.h>

#include "../ff.h"
#include "ff_collectives.h"

#define RANK2VRANK(rank, vrank, root) \
{ \
  vrank = rank; \
  if (rank == 0) vrank = root; \
  if (rank == root) vrank = 0; \
}
#define VRANK2RANK(rank, vrank, root) \
{ \
  rank = vrank; \
  if (vrank == 0) rank = root; \
  if (vrank == root) rank = 0; \
}


/* from ptl_atomic.c */
static int ff_atom_type_size[] = {
    [PTL_INT8_T] = 1,
    [PTL_UINT8_T] = 1,
    [PTL_INT16_T] = 2,
    [PTL_UINT16_T] = 2,
    [PTL_INT32_T] = 4,
    [PTL_UINT32_T] = 4,
    [PTL_INT64_T] = 8,
    [PTL_UINT64_T] = 8,
    [PTL_FLOAT] = 4,
    [PTL_FLOAT_COMPLEX] = 8,
    [PTL_DOUBLE] = 8,
    [PTL_DOUBLE_COMPLEX] = 16,

    /* these are system dependant */
    [PTL_LONG_DOUBLE] = sizeof(long double),
};
/* end ptl_atomic.c */

int ff_activation_bintree(ff_schedule_h sched, ff_op_h * hook, ff_peer root, ff_tag tag);
int ff_activation_tree(ff_schedule_h sched, ff_op_h * hook, ff_tag tag);
