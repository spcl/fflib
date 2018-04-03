ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

lib_LTLIBRARIES = libff.la
libff_la_SOURCES = src/ff.c src/ff_computation.c src/ff_container.c src/ff_op.c src/ff_recv.c src/ff_schedule.c src/ff_send.c src/ff_storage.c src/colls/ff_activation.c src/colls/ff_allgather.c src/colls/ff_allreduce.c src/colls/ff_alltoall.c src/colls/ff_barrier.c src/colls/ff_bcast.c src/colls/ff_gather.c src/colls/ff_reduce.c src/colls/ff_scatter.c src/colls/ff_solo_allreduce.c
libff_la_CFLAGS = "-std=c11"

library_includedir = ${includedir}
library_include_HEADERS = src/ff.h src/colls/ff_collectives.h

