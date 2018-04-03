/*
 * Copyright (c) 2006 The Trustees of Indiana University and Indiana
 *                    University Research and Technology
 *                    Corporation.  All rights reserved.
 * Copyright (c) 2006 The Technical University of Chemnitz. All
 *                    rights reserved.
 *
 * Authors:
 *    Peter Gottschling <pgottsch@osl.iu.edu>
 *    Torsten Hoefler <htor@cs.indiana.edu>
 *
 */

#ifndef NBC_COMMUNICATION_PATTERNS_INCLUDE
#define NBC_COMMUNICATION_PATTERNS_INCLUDE

#include <vector>

#define DO_TEST

namespace nbc {

   template <typename Computation, typename Communication, typename Buffer>
   void normal(int myblocks, Computation& computation, Communication& communication, Buffer& buffer)
   {
     for(int i=0; i<myblocks; i++) 
       computation(i, buffer);
     communication(buffer);
     communication.wait(0, buffer);
   }


   template <typename Computation, typename Communication, typename BufferVector>
   void pipeline(int myblocks, Computation& computation, Communication& communication, BufferVector& buffers)
   {
     if (buffers.size() < 2) throw "not enough buffers!\n";
     for(int i=0; i<myblocks; i++) {
       computation(i, buffers[i%2]);
       if(i) communication.wait(i-1, buffers[(i-1)%2]);
       communication(buffers[i%2]);
     }
     communication.wait(myblocks-1, buffers[(myblocks-1)%2]);
   }


   template <typename Computation, typename Communication, typename BufferVector>
   void pipeline_tiled(int myblocks, Computation& computation, Communication& communication, BufferVector& buffers)
   {
     if (buffers.size() < 2) throw "not enough buffers!\n";
     int tiling= buffers[0]->tile_size(), blocks= myblocks/tiling;
     for(int i=0; i < blocks; i++) {
       for(int j=0; j<tiling;j++) {
         computation(i*tiling+j, buffers[i%2]);
#ifdef DO_TEST
         if(i) communication.test(buffers[(i-1)%2]);
#endif
       }
       
       if(i) communication.wait((i-1)*tiling, buffers[(i-1)%2]);
       communication(buffers[i%2]);
     }
     communication.wait((blocks-1)*tiling, buffers[(blocks-1)%2]);
     
     // remaining blocks if myblocks is not divisible by blocks
     for(int i=blocks*tiling; i < myblocks; i++) 
       computation(i, buffers[0]);
     communication(buffers[0]); 
     communication.wait(blocks*tiling, buffers[0]);     
   }


   template <typename Computation, typename Communication, typename BufferVector>
   void pipeline_window(int myblocks, Computation& computation, Communication& communication, BufferVector& buffers)
   {
     if (buffers.size() < 2) throw "not enough buffers!\n";
     int wp= buffers.size(), w= wp - 1; // w is window size, wp w+1

     for(int i=0; i<myblocks; i++) {
       computation(i, buffers[i%wp]);
       if (i>=w) communication.wait(i-w, buffers[(i-w)%wp]);
#ifdef DO_TEST
       if(i) communication.test(buffers[(i-1)%wp]);
#endif
       communication(buffers[i%wp]);
     }

     for (int i= std::max(myblocks - w, 0); i < myblocks; i++) 
       communication.wait(i, buffers[i%wp]);
   }


   template <typename Computation, typename Communication, typename BufferVector>
   void pipeline_tiled_window(int myblocks, Computation& computation, Communication& communication, BufferVector& buffers)
   {
     if (buffers.size() < 2) throw "not enough buffers!\n";
     int tiling= buffers[0]->tile_size(), blocks= myblocks/tiling;
     int wp= buffers.size(), w= wp - 1; // w is window size, wp w+1
     
     for(int i=0; i < blocks; i++) {
       for(int j=0; j<tiling;j++) {
         computation(i*tiling+j, buffers[i%wp]);
#ifdef DO_TEST
         if(i) communication.test(buffers[(i-1)%wp]);
#endif
       }
       if (i>=w) communication.wait((i-w)*tiling, buffers[(i-w)%wp]);
       communication(buffers[i%wp]);
     }

     // Waiting for the outstanding block communications
     for (int i= std::max(blocks - w, 0); i < blocks; i++) 
       communication.wait(i*tiling, buffers[i%wp]);

     // remaining blocks if myblocks is not divisible by blocks
     for(int i=blocks*tiling; i < myblocks; i++) 
       computation(i, buffers[0]);
     communication(buffers[0]); 
     communication.wait(blocks*tiling, buffers[0]);     
   }


      

} // namespace nbc

#endif // NBC_COMMUNICATION_PATTERNS_INCLUDE
