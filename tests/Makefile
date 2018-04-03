CC    = /home/digirols/openmpi-1.10.1/build/bin/mpicc
#CC=mpicc
CFLAGS= -std=c99 -D_POSIX_C_SOURCE=199309L -g3 -O0 -I../include
LDFLAGS=  -L ../lib/ -Wl,-rpath,../lib/ -lff


all: broadcast sbroadcast scatter alltoallv

%: %.c
	$(CC) $(CFLAGS) $*.c -o $* $(OBJECTS) $(LDFLAGS)

