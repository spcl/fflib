/*
 * Copyright (c) 2006 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 * Copyright (c) 2006 The Technical University of Chemnitz. All 
 *                    rights reserved.
 *
 *  Authors:
 *    Peter Gottschling <pgottsch@osl.iu.edu>
 *    Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#include <stdio.h>
#include <assert.h>
#include <mpi.h>
#include <math.h>
#include "nbc.h"

#include <liblsb.h>
#include <fflib.h>


#define BLOCKING 1
#define NONBLOCKING 2
#define OFFLOADED 3

/* ugly global variables */
double gt1, gt2, gt3, gt4;

struct array_3d
{
  double  *array;
  int     x_dim, y_dim, z_dim,
          z_inc;                /* must be x_dim * y_dim */
  /* correspondingly y_inc == x_dim and x_inc == 1 */
};


/* plane within a 3D array */
struct plane
{
  double  *first;               /* pointer to first element in 3D array */
  int     major_dim, minor_dim, /* dimensions of plane */
          major_inc, minor_inc, /* increments in both directions */
          par_inc;              /* increment parallel to the plane */
};


/* Information about Cartesian grid */
struct grid_info
{
  /* neighbors */
  int left, right, top, bottom, front, back;

};


/* Set of buffers to send to or to receive from neighbors */
struct buffer_set
{
  double *start;
  int    left, right, top, bottom, front, back;
  int    d_left, d_right, d_top, d_bottom, d_front, d_back;
};


/* All communication related data */
struct comm_data_t
{
  MPI_Comm           processor_grid;
  struct grid_info   info;
  struct buffer_set  send_buffers, recv_buffers;
  int                comm_type; /* BLOCKING/NONBLOCKING/OFFLOADED */
  NBC_Handle         *handle;
  int                rank;
};

/* =========
   Functions
   ========= 
*/


inline int vector_size(struct array_3d *a)
{ 
  return a->x_dim * a->y_dim * a->z_dim;
}


inline int x_plane_size(struct array_3d *a)
{
  return a->y_dim * a->z_dim;
}


inline int y_plane_size(struct array_3d *a)
{
  return a->x_dim * a->z_dim;
}


inline int z_plane_size(struct array_3d *a)
{
  return a->x_dim * a->y_dim;
}


/* block size of <n>th processor out of num_proc with <total> overall block size 
   n in [0, num_proc-1] */
inline int nth_block_size(int n, int num_proc, int total)
{
  return total / num_proc + (total % num_proc > n);
}


void alloc_buffer_set(struct buffer_set *bs, struct grid_info *gi, struct array_3d *a) 
{
  /* memory must be contiguous in buffers (would be easier otherwise) */
  int     total;
  
  /* all sizes and displacements initialized to 0 */
  bs->left= bs->right= bs->front= bs->back= bs->top= bs->bottom= 0;
  bs->d_left= bs->d_right= bs->d_front= bs->d_back= bs->d_top= bs->d_bottom= 0;

  total= 0;
  if (gi->left != MPI_PROC_NULL) {
    bs->left= x_plane_size(a);
    bs->d_left= total;
    total+= x_plane_size(a);
  }
  if (gi->right != MPI_PROC_NULL) {
    bs->right= x_plane_size(a);
    bs->d_right= total;
    total+= x_plane_size(a);
  }

  if (gi->front != MPI_PROC_NULL) {
    bs->front= y_plane_size(a);
    bs->d_front= total;
    total+= y_plane_size(a);
  }
  if (gi->back != MPI_PROC_NULL) {
    bs->back= y_plane_size(a);
    bs->d_back= total;
    total+= y_plane_size(a);
  }

  if (gi->top != MPI_PROC_NULL) {
    bs->top= z_plane_size(a);
    bs->d_top= total;
    total+= z_plane_size(a);
  }
  if (gi->bottom != MPI_PROC_NULL) {
    bs->bottom= z_plane_size(a);
    bs->d_bottom= total;
    total+= z_plane_size(a);
  }
  
  bs->start= (double*) malloc(total * sizeof(double)); 

/*   int my_rank; */
/*   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); */
/*   printf("rank %i: left nb = %i\n", my_rank, bs->left); */
/*   printf("rank %i: right nb = %i\n", my_rank, bs->right); */
}


inline double av(struct array_3d *a, int x, int y, int z)
{
  return a->array[z * a->z_inc + y * a->x_dim + x];
}

/* same as pointer */
inline double* ap(struct array_3d *a, int x, int y, int z)
{
  return &a->array[z * a->z_inc + y * a->x_dim + x];
}

/* Returns the <x>th plane in x direction */
void x_plane(struct array_3d *a, int x, struct plane *p)
{
  assert(0 <= x && x < a->x_dim);

  p->first= a->array + x;
  p->major_dim= a->z_dim;
  p->minor_dim= a->y_dim;
  p->major_inc= a->z_inc;
  p->minor_inc= a->x_dim;
  p->par_inc= 1;
}


void left_plane(struct array_3d *a, struct plane *p)
{
  x_plane(a, 0, p);
}


void right_plane(struct array_3d *a, struct plane *p)
{
  x_plane(a, a->x_dim - 1, p);
}


/* Returns the <y>th plane in y direction */
void y_plane(struct array_3d *a, int y, struct plane *p)
{
  assert(0 <= y && y < a->y_dim);

  p->first= a->array + y * a->x_dim;
  p->major_dim= a->z_dim;
  p->minor_dim= a->x_dim;
  p->major_inc= a->z_inc;
  p->minor_inc= 1;
  p->par_inc=   a->x_dim;
}


void front_plane(struct array_3d *a, struct plane *p)
{
  y_plane(a, 0, p);
}


void back_plane(struct array_3d *a, struct plane *p)
{
  y_plane(a, a->y_dim - 1, p);
}


/* Returns the <z>th plane in z direction */
void z_plane(struct array_3d *a, int z, struct plane *p)
{
  assert(0 <= z && z < a->z_dim);

  p->first= a->array + z * a->z_inc;
  p->major_dim= a->y_dim;
  p->minor_dim= a->x_dim;
  p->major_inc= a->x_dim;
  p->minor_inc= 1;
  p->par_inc=   a->z_inc;
}


void bottom_plane(struct array_3d *a, struct plane *p)
{
  z_plane(a, 0, p);
}


void top_plane(struct array_3d *a, struct plane *p)
{
  z_plane(a, a->z_dim - 1, p);
}

void print_array_3d(struct array_3d *a)
{
  int i, j, k;

  /* z planes */
  for (i= 0; i < a->z_dim; i++) { 
    for (j= 0; j < a->y_dim; j++) {
      for (k= 0; k < a->x_dim; k++)
	printf("%9f ", av(a, k, j, i));
      printf("\n");
    }
    printf("\n");
  }
}

void parallel_print_array_3d(struct array_3d *a, struct comm_data_t *comm_data)
{
  int         num_proc, id, i, buf;
  MPI_Status  status;

  MPI_Comm_rank(comm_data->processor_grid, &id);
  MPI_Comm_size(comm_data->processor_grid, &num_proc);

  for (i= 0; i < num_proc; i++)
    if (id == i) {
      if (id > 0) 
	MPI_Recv(&buf, 1, MPI_INT, id - 1, 998, comm_data->processor_grid, &status);

      printf ("\nOn processor %i:\n", id);
      print_array_3d(a);

      if (id < num_proc-1)
	MPI_Send(&buf, 1, MPI_INT, id + 1, 998, comm_data->processor_grid);
    }
}


void init_array_3d(struct array_3d *a, int xd, int yd, int zd)
{
  a->x_dim= xd; a->y_dim= yd; a->z_dim= zd; 
  a->z_inc= xd * yd;

  a->array= (double*)malloc(xd * yd * zd * sizeof(double));
}


/* Set array to values 1, 2, ...
   dimensions must be defined, array allocated */
void iota_array_3d(struct array_3d *a)
{
  int array_size, n;
  
  array_size= a->x_dim * a->y_dim * a->z_dim;
  for (n= 0; n < array_size; n++) 
    a->array[n]= (double) n;
}


/* Set array to <value>
   dimensions must be defined, array allocated */
void set_array_3d(struct array_3d *a, double value)
{
  int array_size, n;
  
  array_size= a->x_dim * a->y_dim * a->z_dim;
  for (n= 0; n < array_size; n++) 
    a->array[n]= value;
}


void set_plane(struct plane *p, double value)
{
  int i, j, major_index, minor_index;

  for (major_index= i= 0; i < p->major_dim; i++, major_index+= p->major_inc)
    for (minor_index= major_index, j= 0; j < p->minor_dim; j++, minor_index+= p->minor_inc)
      p->first[minor_index]= value;
}
 

inline check_same_dimensions(struct array_3d *v1, struct array_3d *v2)
{
  assert(v1->x_dim == v2->x_dim && v1->y_dim == v2->y_dim && v1->z_dim == v2->z_dim);
}

/* Same semantic as volume_mult_simple but the vast majority of grid points
   has no branches */
void volume_mult(struct array_3d *v_in, struct array_3d *v_out, struct comm_data_t *comm_data)
{
  int i, j, k, test_modulo;
  double tmp;
  
  test_modulo= v_in->z_dim / 100;
  if (test_modulo == 0) test_modulo= 1;

  check_same_dimensions(v_in, v_out);

  /* First plane if exist */
  if (v_in->z_dim > 0) {
    i= 0;
    for (j= 0; j < v_in->y_dim; j++) 
      for (k= 0; k < v_in->x_dim; k++) {
	tmp= 6.0 * av(v_in, k, j, i);	

	if (v_in->z_dim > 1) tmp-= av(v_in, k, j, i+1);
	
	if (j > 0) tmp-= av(v_in, k, j-1, i);
	if (j < v_in->y_dim-1) tmp-= av(v_in, k, j+1, i);

	if (k > 0) tmp-= av(v_in, k-1, j, i);
	if (k < v_in->x_dim-1) tmp-= av(v_in, k+1, j, i);

	*ap(v_out, k, j, i)= tmp;
      }
  }

  /* Second to second-last plane */
  for (i= 1; i < v_in->z_dim-1; i++) {
    if ((comm_data->non_blocking) && (i % test_modulo == 0)) 
      NBC_Test(comm_data->handle);
    /* First line */
    if (v_in->y_dim > 0) {
      j= 0;
      for (k= 0; k < v_in->x_dim; k++) {
	tmp= 6.0 * av(v_in, k, j, i) - av(v_in, k, j, i-1) - av(v_in, k, j, i+1);	

	if (j < v_in->y_dim-1) tmp-= av(v_in, k, j+1, i);

	if (k > 0) tmp-= av(v_in, k-1, j, i);
	if (k < v_in->x_dim-1) tmp-= av(v_in, k+1, j, i);

	*ap(v_out, k, j, i)= tmp;
      }
    }

    /* Second to second-last line */
    for (j= 1; j < v_in->y_dim-1; j++) {
      /* First point if exist */      
      if (v_in->x_dim > 0) {
	*ap(v_out, 0, j, i)= 6.0 * av(v_in, 0, j, i) 
             - av(v_in, 0, j, i-1) - av(v_in, 0, j, i+1)	
  	     - av(v_in, 0, j-1, i) - av(v_in, 0, j+1, i);
	if (0 < v_in->x_dim-1) *ap(v_out, 0, j, i)-= av(v_in, 1, j, i);
      }

      /* Second to second-last point */
      for (k= 1; k < v_in->x_dim-1; k++) {
	*ap(v_out, k, j, i)= 6.0 * av(v_in, k, j, i) 
	     - av(v_in, k, j, i-1) - av(v_in, k, j, i+1)	
  	     - av(v_in, k, j-1, i) - av(v_in, k, j+1, i)
             - av(v_in, k-1, j, i) - av(v_in, k+1, j, i);
      }

      /* Last point if exist, implies k-1 is valid */
      if (v_in->x_dim > 1) {
	k= v_in->x_dim - 1;
	*ap(v_out, k, j, i)= 6.0 * av(v_in, k, j, i) 
	     - av(v_in, k, j, i-1) - av(v_in, k, j, i+1)	
  	     - av(v_in, k, j-1, i) - av(v_in, k, j+1, i)
             - av(v_in, k-1, j, i);
      }
    }

    /* Last line if exist, implies j-1 is valid */
    if (v_in->y_dim > 1) {
      j= v_in->y_dim - 1;
      for (k= 0; k < v_in->x_dim; k++) {
	tmp= 6.0 * av(v_in, k, j, i) - av(v_in, k, j, i-1) - av(v_in, k, j, i+1)
  	     - av(v_in, k, j-1, i);
	
	if (k > 0) tmp-= av(v_in, k-1, j, i);
	if (k < v_in->x_dim-1) tmp-= av(v_in, k+1, j, i);

	*ap(v_out, k, j, i)= tmp;
      }
    }
  }

  /* Last plane if there is more then one, implies that i-1 is valid */
  if (v_in->z_dim > 1) {
    i= v_in->z_dim - 1;
    for (j= 0; j < v_in->y_dim; j++) 
      for (k= 0; k < v_in->x_dim; k++) {
	tmp= 6.0 * av(v_in, k, j, i) - av(v_in, k, j, i-1);	
	
	if (j > 0) tmp-= av(v_in, k, j-1, i);
	if (j < v_in->y_dim-1) tmp-= av(v_in, k, j+1, i);

	if (k > 0) tmp-= av(v_in, k-1, j, i);
	if (k < v_in->x_dim-1) tmp-= av(v_in, k+1, j, i);

	*ap(v_out, k, j, i)= tmp;
      }
  }
}


void volume_mult_simple(struct array_3d *v_in, struct array_3d *v_out)
{
  int i, j, k;
  double tmp;

  check_same_dimensions(v_in, v_out);

  for (i= 0; i < v_in->z_dim; i++) 
    for (j= 0; j < v_in->y_dim; j++) 
      for (k= 0; k < v_in->x_dim; k++) {
	tmp= 6.0 * av(v_in, k, j, i);	

	if (i > 0) tmp-= av(v_in, k, j, i-1);
	if (i < v_in->z_dim-1) tmp-= av(v_in, k, j, i+1);
	
	if (j > 0) tmp-= av(v_in, k, j-1, i);
	if (j < v_in->y_dim-1) tmp-= av(v_in, k, j+1, i);

	if (k > 0) tmp-= av(v_in, k-1, j, i);
	if (k < v_in->x_dim-1) tmp-= av(v_in, k+1, j, i);

	*ap(v_out, k, j, i)= tmp;
      }
}


void plane_buffer_mult(struct plane *p, double* buffer)
{
  
  int i, j, major_index, minor_index;

  for (major_index= i= 0; i < p->major_dim; i++, major_index+= p->major_inc)
    for (minor_index= major_index, j= 0; j < p->minor_dim; j++, minor_index+= p->minor_inc)
      p->first[minor_index]-= *buffer++;
}


void plane_buffer_copy(struct plane *p, double* buffer)
{
  
  int i, j, major_index, minor_index;

  for (major_index= i= 0; i < p->major_dim; i++, major_index+= p->major_inc)
    for (minor_index= major_index, j= 0; j < p->minor_dim; j++, minor_index+= p->minor_inc)
      *buffer++= p->first[minor_index];
}


inline copy_dims(int d_in[3], int d_out[3])
{
  d_out[0]= d_in[0]; d_out[1]= d_in[1]; d_out[2]= d_in[2]; 
}


inline int
nb_rank(MPI_Comm grid, int rank, int dim, int dir, int dims[3], int my_coords[3])
{
  int       nb_coords[3], my_nb;

  /* check for outer boundaries: first lower end then upper */
  if (dir == -1 && my_coords[dim] == 0
      || dir == 1 && my_coords[dim] == dims[dim]-1)
    return MPI_PROC_NULL;

  copy_dims(my_coords, nb_coords);
  nb_coords[dim]+= dir;
  MPI_Cart_rank(grid, nb_coords, &my_nb); 
 
  return my_nb;
}

void init_processor_grid(MPI_Comm *processor_grid, struct grid_info *gi)
{
  int num_proc, my_rank, dims[]= {0, 0, 0}, periods[]= {0, 0, 0}, my_coords[3],
      nb_coords[3];

  MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
  MPI_Dims_create(num_proc, 3, dims);
  MPI_Cart_create(MPI_COMM_WORLD, 3, dims, periods, 0, processor_grid);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
/*   if (!my_rank)  */
/*     printf("dims = {%i, %i, %i}\n", dims[0], dims[1], dims[2]); */
  
  MPI_Cart_coords(*processor_grid, my_rank, 3, my_coords);
/*      printf("rank %i: my_coords = {%i, %i, %i}\n", my_rank, my_coords[0], my_coords[1], my_coords[2]);  */

  gi->left=   nb_rank(*processor_grid, my_rank, 0, -1, dims, my_coords);
  gi->right=  nb_rank(*processor_grid, my_rank, 0,  1, dims, my_coords);
  gi->front=  nb_rank(*processor_grid, my_rank, 1, -1, dims, my_coords);
  gi->back=   nb_rank(*processor_grid, my_rank, 1,  1, dims, my_coords);
  gi->bottom= nb_rank(*processor_grid, my_rank, 2, -1, dims, my_coords);
  gi->top=    nb_rank(*processor_grid, my_rank, 2,  1, dims, my_coords);

/*   printf("rank %i: left nb = %i\n", my_rank, gi->left); */
/*   printf("rank %i: right nb = %i\n", my_rank, gi->right); */

}


/* fill send buffer with boundary values of v */
void fill_buffers(struct array_3d *v, struct buffer_set *send_buffers)
{
  struct plane    v_left_plane, v_right_plane, v_front_plane, v_back_plane,
                  v_bottom_plane, v_top_plane;
  
  left_plane(v, &v_left_plane);
  right_plane(v, &v_right_plane);
  front_plane(v, &v_front_plane);
  back_plane(v, &v_back_plane);
  top_plane(v, &v_top_plane);
  bottom_plane(v, &v_bottom_plane);

  /* single buffer sizes are set to 0 if there is no neighbor, we use this as test */
  if (send_buffers->left) 
    plane_buffer_copy(&v_left_plane, send_buffers->start + send_buffers->d_left);
  if (send_buffers->right) 
    plane_buffer_copy(&v_right_plane, send_buffers->start + send_buffers->d_right);
  if (send_buffers->front) 
    plane_buffer_copy(&v_front_plane, send_buffers->start + send_buffers->d_front);
  if (send_buffers->back) 
    plane_buffer_copy(&v_back_plane, send_buffers->start + send_buffers->d_back);
  if (send_buffers->top) 
    plane_buffer_copy(&v_top_plane, send_buffers->start + send_buffers->d_top);
  if (send_buffers->bottom) 
    plane_buffer_copy(&v_bottom_plane, send_buffers->start + send_buffers->d_bottom);
}


void start_send_boundaries(struct comm_data_t *comm_data)
{
  /* send and receive displacements and sizes are equal b/c we always send whole planes */
  int *sendcounts, *sdispls, *recvcounts, *rdispls, num_proc, i;
  struct buffer_set        send_buffers, recv_buffers;
  struct grid_info         gi;
  MPI_Comm                 processor_grid;

  send_buffers= comm_data->send_buffers;
  recv_buffers= comm_data->recv_buffers;
  gi= comm_data->info;
  processor_grid= comm_data->processor_grid;

  MPI_Comm_size(processor_grid, &num_proc);
  sendcounts= (int*) malloc(num_proc * sizeof(int));
  sdispls=    (int*) malloc(num_proc * sizeof(int));
  recvcounts= (int*) malloc(num_proc * sizeof(int));
  rdispls=    (int*) malloc(num_proc * sizeof(int));

  for (i= 0; i < num_proc; i++) {
    sendcounts[i]= 0; sdispls[i]= 0; recvcounts[i]= 0; rdispls[0]= 0;
  }

  /* replace for the six neighbors */
  if (send_buffers.left) {
    sendcounts[gi.left]= recvcounts[gi.left]= send_buffers.left;
    sdispls[gi.left]= rdispls[gi.left]= send_buffers.d_left;
  }
  if (send_buffers.right) { 
    sendcounts[gi.right]= recvcounts[gi.right]= send_buffers.right;
    sdispls[gi.right]= rdispls[gi.right]= send_buffers.d_right;
  }

  if (send_buffers.front) {
    sendcounts[gi.front]= recvcounts[gi.front]= send_buffers.front;
    sdispls[gi.front]= rdispls[gi.front]= send_buffers.d_front;
  }
  if (send_buffers.back) {
    sendcounts[gi.back]= recvcounts[gi.back]= send_buffers.back;
    sdispls[gi.back]= rdispls[gi.back]= send_buffers.d_back;
  }

  if (send_buffers.top) {
    sendcounts[gi.top]= recvcounts[gi.top]= send_buffers.top;
    sdispls[gi.top]= rdispls[gi.top]= send_buffers.d_top;
  }
  if (send_buffers.bottom) {
    sendcounts[gi.bottom]= recvcounts[gi.bottom]= send_buffers.bottom;
    sdispls[gi.bottom]= rdispls[gi.bottom]= send_buffers.d_bottom;
  }

  gt1 = MPI_Wtime();
  if (comm_data->comm_type==NONBLOCKING)
        NBC_Ialltoallv(send_buffers.start, sendcounts, sdispls, MPI_DOUBLE, 
		   recv_buffers.start, recvcounts, rdispls, MPI_DOUBLE, processor_grid, comm_data->handle);
    else if (comm_data->comm_type==OFFLOADED){  
        ff_schedule_t sched = ff_alltoallv(send_buffers.start, sendcounts, sdispls, recv_buffers.start, recvcounts, rdispls, sizeof(double));     
    }else {
        MPI_Alltoallv(send_buffers.start, sendcounts, sdispls, MPI_DOUBLE, 
		  recv_buffers.start, recvcounts, rdispls, MPI_DOUBLE, processor_grid);
        /*NBC_Ialltoallv(send_buffers.start, sendcounts, sdispls, MPI_DOUBLE, 
		   recv_buffers.start, recvcounts, rdispls, MPI_DOUBLE, processor_grid, comm_data->handle);
        NBC_Wait(comm_data->handle);*/
  }
  gt4 = MPI_Wtime();
  
  free(sendcounts); free(sdispls); free(recvcounts); free(rdispls);
}

void finish_send_boundaries(struct comm_data_t *comm_data)
{
   if (comm_data->non_blocking)
     NBC_Wait(comm_data->handle);
   gt2 = MPI_Wtime();
}

/* Multiply value of send_buffer with -1 and add this to corresponding values in outer planes */
void mult_boundaries(struct array_3d *v, struct buffer_set *recv_buffers)
{
  struct plane    v_left_plane, v_right_plane, v_front_plane, v_back_plane,
                  v_bottom_plane, v_top_plane;
  
  left_plane(v, &v_left_plane);
  right_plane(v, &v_right_plane);
  front_plane(v, &v_front_plane);
  back_plane(v, &v_back_plane);
  top_plane(v, &v_top_plane);
  bottom_plane(v, &v_bottom_plane);

  /* single buffer sizes are set to 0 if there is no neighbor, we use this as test */
  if (recv_buffers->left) 
    plane_buffer_mult(&v_left_plane, recv_buffers->start + recv_buffers->d_left);
  if (recv_buffers->right) 
    plane_buffer_mult(&v_right_plane, recv_buffers->start + recv_buffers->d_right);
  if (recv_buffers->front) 
    plane_buffer_mult(&v_front_plane, recv_buffers->start + recv_buffers->d_front);
  if (recv_buffers->back) 
    plane_buffer_mult(&v_back_plane, recv_buffers->start + recv_buffers->d_back);
  if (recv_buffers->top) 
    plane_buffer_mult(&v_top_plane, recv_buffers->start + recv_buffers->d_top);
  if (recv_buffers->bottom) 
    plane_buffer_mult(&v_bottom_plane, recv_buffers->start + recv_buffers->d_bottom);
}   


void matrix_vector_mult(struct array_3d *v_in, struct array_3d *v_out,
			struct comm_data_t *comm_data)
{
  fill_buffers(v_in, &comm_data->send_buffers);
  start_send_boundaries(comm_data);
  volume_mult(v_in, v_out, comm_data);
/*   printf("--vol in mvm\n"); */
/*   print_array_3d(v_out); */

  finish_send_boundaries(comm_data);
  mult_boundaries(v_out, &comm_data->recv_buffers);
}


/* Allocates send and receive buffers, needs 3D array for size of buffers */
void allocate_buffers(struct comm_data_t *comm_data, struct array_3d *x)
{
  alloc_buffer_set(&comm_data->send_buffers, &comm_data->info, x);
  alloc_buffer_set(&comm_data->recv_buffers, &comm_data->info, x);
  comm_data->handle = malloc(sizeof(NBC_Handle));
}


void allocate_vectors(struct array_3d *x, struct array_3d *b, int argc, char** argv, 
		      struct comm_data_t *comm_data)
{
  int total_x, total_y, total_z, local_x, local_y, local_z, dims[3], periods[3], my_coords[3];

  if (argc < 5) {
    printf("Usage: cg_poisson_3d <X_Dimension> <Y_Dimension> <Z_Dimension> <rel_error>\n");
    printf("                Dimensions of the whole domain\n");
    printf("                CG stops when r_k/r_0 < rel_error\n");
    exit(1); }

  total_x= atoi(argv[1]); total_y= atoi(argv[2]); total_z= atoi(argv[3]); 
 
  MPI_Cart_get(comm_data->processor_grid, 3, dims, periods, my_coords);
 
  local_x= nth_block_size(my_coords[0], dims[0], total_x);
  local_y= nth_block_size(my_coords[1], dims[1], total_y);
  local_z= nth_block_size(my_coords[2], dims[2], total_z);
  
  init_array_3d(x, local_x, local_y, local_z);
  init_array_3d(b, local_x, local_y, local_z);
}


/* Sets b such that x=1 is the solution, x initialized to 0 
   x= 1; b= Ax; x= 0; */
void init_vectors(struct array_3d *x, struct array_3d *b, struct comm_data_t *comm_data)
{
  set_array_3d(x, 1.0);
  matrix_vector_mult(x, b, comm_data);
  set_array_3d(x, 0.0);
}


/* v+= w */
void vector_assign_add(struct array_3d *v, struct array_3d *w)
{
  int size, i;

  size= vector_size(v);
  for (i= 0; i < size; i++)
    v->array[i] += w->array[i];
}


double parallel_dot(struct array_3d *v, struct array_3d *w, struct comm_data_t *comm_data)
{
  int         size, size_b, i;
  double      local_dot_0, local_dot_1, local_dot_2, local_dot_3, global_dot;

  local_dot_0= local_dot_1= local_dot_2= local_dot_3= 0.0;

  size= vector_size(v);
  size_b= (size / 4) * 4;
  for (i= 0; i < size_b; i+= 4) {
    local_dot_0+= v->array[i] * w->array[i];
    local_dot_1+= v->array[i+1] * w->array[i+1];
    local_dot_2+= v->array[i+2] * w->array[i+2];
    local_dot_3+= v->array[i+3] * w->array[i+3];
  }

  for (i= size_b; i < size; i++) 
    local_dot_0+= v->array[i] * w->array[i];

  local_dot_0+= local_dot_1 + local_dot_2 + local_dot_3;

  MPI_Allreduce(&local_dot_0, &global_dot, 1, MPI_DOUBLE, MPI_SUM, comm_data->processor_grid);
  return global_dot;
}
  

int conjugate_gradient(struct array_3d *b, struct array_3d *x, double rel_error, struct comm_data_t *comm_data)
{
  int                   size, i, iter;
  struct array_3d       v, q, r;
  double                res_0, gamma, gamma_old, beta, delta, alpha;
  double                t0,t1,t2,t3;

  init_array_3d(&v, x->x_dim, x->y_dim, x->z_dim);
  init_array_3d(&q, x->x_dim, x->y_dim, x->z_dim);
  init_array_3d(&r, x->x_dim, x->y_dim, x->z_dim);

  size= x->x_dim * x->y_dim * x->z_dim;

  /* since x == 0, q and r are b */
  for (i= 0; i < size; i++)
    q.array[i]= r.array[i]= b->array[i];
  
  res_0= gamma= parallel_dot(&r, &r, comm_data);
  rel_error= rel_error * rel_error; /* b/c we compare dot products not norms */

  for(iter= 0; fabs(gamma/res_0) >= rel_error && iter < 10000; iter++) {
    if (iter > 0) {
      beta= gamma / gamma_old;
      for (i= 0; i < size; i++)
	      q.array[i]= r.array[i] + beta * q.array[i];
    }

    t1= MPI_Wtime();
    matrix_vector_mult(&q, &v, comm_data);
    t2= MPI_Wtime();
    delta= parallel_dot(&q, &v, comm_data);
    alpha= gamma / delta;

    for (i= 0; i < size; i++) {
      x->array[i]+= alpha * q.array[i];
      r.array[i]-= alpha * v.array[i];
    }

    gamma_old= gamma;
    gamma= parallel_dot(&r, &r, comm_data);
    t3= MPI_Wtime();
    if((iter == 2) && (comm_data->rank == 0)) {
      printf("matrix_vector_mult: %.4lf, rest: %.4lf\n", t2-t1, t3-t2);
      printf("gt2-gt1: %lf\n", gt2-gt1);
      printf("gt4-gt1: %lf\n", gt4-gt1);
    }
  }
  return iter;
}




int main(int argc, char** argv) 
{
  struct array_3d       x, b;
  struct comm_data_t    comm_data;
  double                rel_error;
  double                t1, t2, t3;
  int                   rank, iter_block, iter_non_block;

  MPI_Init(&argc, &argv);
  
  comm_data.non_blocking= 0;
  init_processor_grid(&comm_data.processor_grid, &comm_data.info);
  allocate_vectors(&x, &b, argc, argv, &comm_data);
  allocate_buffers(&comm_data, &x);
  init_vectors(&x, &b, &comm_data);
  MPI_Comm_rank(comm_data.processor_grid, &comm_data.rank);

/*   matrix_vector_mult(b, x, comm_data); */
/*   print_array_3d(x); */

  rel_error= atof(argv[4]);
  
  t1= MPI_Wtime();
  iter_block= conjugate_gradient(&b, &x, rel_error, &comm_data);
  t2= MPI_Wtime();
  if (comm_data.rank == 0) printf("blocking run done ... \n");

  comm_data.non_blocking= 1;
  iter_non_block= conjugate_gradient(&b, &x, rel_error, &comm_data);
  t3= MPI_Wtime();
  if (comm_data.rank == 0) printf("non blocking run done ... \n");

  if (comm_data.rank == 0)  {
    printf("Blocking operation %.2lf, non-blocking operation %.2lf (%.2lf)\n", t2-t1, t3-t2, ((t2-t1)-(t3-t2))/(t2-t1)*100);
    printf("Time resolution: %lf\n", MPI_Wtick());
    printf("Iterations blocking: %i, non-blocking: %i\n", iter_block, iter_non_block);
  }
/*   parallel_print_array_3d(x, comm_data); */

  MPI_Finalize();
  return 0;
}

