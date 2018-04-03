#!/usr/bin/python
if __name__ == '__main__':
  import sys
  import os
  sys.path.insert(0, os.path.abspath('config'))
  import configure
  configure_options = [
    '--prefix=/home/digirols/fflib/benchmarks/petsc-3.6.3/build',
    '--with-mpi-dir=/home/digirols/openmpi-1.10.1/build/',
    'PETSC_ARCH=arch-linux2-c-debug',
  ]
  configure.petsc_configure(configure_options)
