#!/bin/bash

np=$1
shift
/home/digirols/openmpi-1.8.5-normal/build/bin/mpirun -n $np --mca pml ob1 --mca btl ^portals4 --mca mtl ^portals4 --mca osc ^portals4 --mca coll ^tuned,portals4 $@
