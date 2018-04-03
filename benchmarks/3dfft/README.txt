/*
 * Copyright (c) 2006 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 * Copyright (c) 2006 The Technical University of Chemnitz. All
 *                    rights reserved.
 *
 * Authors:
 *    Torsten Hoefler <htor@cs.indiana.edu>
 *
 */
- Compile 3d-fft.cpp with the MPI C++ wrapper compiler (either mpicxx,
  mpic++ or mpiC). 
- Add include path to the FFTW header file (fftw3.h) and LibNBC header
  file (nbc.h). 
- Add FFTW and LibNBC libraries to linker line
