#include <string>
#include <iostream>
#include <mpi.h>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

#define ASYNCH_DEGREE 10
#define SEED 12345
#define ITER 200


extern "C"
{
#include "ff.h"
#include "ff_collectives.h"
}

static std::random_device rd;
//static std::mt19937 mt(rd());
static std::mt19937 mt(SEED);
static double noise = 1.0;


//static int tag=1;
//static int tags=1015;


/* The tag counter must be different for each interleaved solo. 
   They can advance with a different speed. */
static int tag_scatter=1000;
static int tag_gather=2000;


ff_schedule_h create_solo_gather(ff_sched_info info){
    //double * sndbuff = ((double **) info.userdata)[0];
    //double * recbuff = ((double **) info.userdata)[1];
    //printf("[Rank %i] creating gather: tag: %i\n", ff_get_rank(), tag);
    void * sndbuff = ff_get_rank()==0 ? NULL : info.userdata;
    void * rcvbuff = ff_get_rank()==0 ? info.userdata:  NULL; 
    return ff_solo_gather(sndbuff, rcvbuff, info.count, info.unitsize, info.root, tag_gather++);
}

ff_schedule_h create_solo_scatter(ff_sched_info info){
    //double * sndbuff = ((double **) info.userdata)[0];
    //double * recbuff = ((double **) info.userdata)[1];
    //printf("creating scatter\n");
    void * sndbuff = ff_get_rank()==0 ? info.userdata : NULL;
    void * rcvbuff = ff_get_rank()==0 ?  NULL : info.userdata;
    return ff_scatter(sndbuff, rcvbuff, info.count, info.unitsize, info.root, tag_scatter++);
}

class Coarse {
    private:
        int _time;
    public:
        Coarse(int t) : _time(t) { }
        inline void work() {
            std::uniform_int_distribution<int> dist(0, _time / noise);
            //printf("[Rank %i] COARSE sleeping for %i\n", ff_get_rank(), _time + dist(mt));
            std::this_thread::sleep_for(std::chrono::milliseconds(_time + dist(mt)));
        }
};

class Fine {
    private:
        /* MPI communicator. */
        MPI_Comm _comm;
        /* Local timer for simulating work. */
        int      _time;
        /* Structure for sparse communications. */
        std::vector<std::pair<int, std::vector<int>>> _sp;
        /* Send buffer for neighborhood communications. */
        std::vector<double*>    _rbuff;
        /* Receive buffer for neighborhood communications. */
        std::vector<double*>    _sbuff;
        /* Size of global communications. */
        int           _sg;
        /* Buffer for global communications. */
        double*    _gbuff;
        /* Structure for coarsened work. */
        Coarse*   _coarse;
        MPI_Request* _rq;
        /* Time for applying the smoother. */
        int          _st;
        //offloading
        ff_sched_info collinfo;
        //int adegree;
        ff_schedule_h gather, scatter;
        MPI_Request mgather, mscatter;
    public:
        ff_container_h scatterc;
        ff_container_h gatherc;


        Fine(MPI_Comm comm, int t, int nu, int smooth) : _time(t), _sg(nu), _gbuff(new double[nu]), _coarse(), _rq(), _st(smooth) {
            MPI_Comm_dup(comm, &_comm);


            
            collinfo.count=_sg;
            collinfo.unitsize=sizeof(double);
            collinfo.tag = 1; //meaningless
            collinfo.root=0;
            //adegree=ASYNCH_DEGREE;

            if (rank()!=0){
                collinfo.userdata = (void *) _gbuff;

                scatterc = ff_container_create(collinfo, create_solo_scatter);
                gatherc = ff_container_create(collinfo, create_solo_gather);
                /*for (int i=0; i<adegree; i++) {
                    ff_container_increase_async(scatterc);
                    ff_container_increase_async(gatherc);
                }
                ff_container_start(scatterc);
                ff_container_start(gatherc);*/
            }
        }
        ~Fine() {

            //printf("finalizing!\n");
            MPI_Comm_free(&_comm);

            ff_finalize();
            //printf("freeing memory\n");
            delete [] _gbuff;
            delete [] _rq;
            if(!_rbuff.empty())
                delete [] _rbuff[0];
            delete _coarse;
            int finalized=0;
            //MPI_Finalized(&finalized);
            //if(!finalized){
                //MPI_Finalize();
              //  ff_finalize();
            //}
        }

        int getSG() { return _sg; }

        inline void neighbors(std::vector<std::pair<int, std::vector<int>>>& m) {
            _sp = std::move(m);
            _rq = new MPI_Request[2 * _sp.size()];
            _rbuff.reserve(_sp.size());
            _sbuff.reserve(_sp.size());
            int size = 0;
            for(const auto& i : _sp)
                size += i.second.size();
            double* rbuff = new double[2 * size];
            double* sbuff = rbuff + size;
            size = 0;
            for(const auto& i : _sp) {
                _rbuff.emplace_back(rbuff + size);
                _sbuff.emplace_back(sbuff + size);
                size += i.second.size();
            }
        }
        inline int size() {
            int size;
            MPI_Comm_size(_comm, &size);
            return size;
        }
        inline int rank() {
            int rank;
            MPI_Comm_rank(_comm, &rank);
            return rank;
        }
        inline void work() {
            std::uniform_int_distribution<int> dist(0, _time / noise);
            //printf("[Rank %i] sleeping for: %i\n", ff_get_rank(), _time + dist(mt));
            std::this_thread::sleep_for(std::chrono::milliseconds(_time + dist(mt)));
        }
        inline void communicate() {
            //printf("[Rank %i] neigh: %i\n", ff_get_rank(), _sp.size());
            
            for(unsigned short i = 0; i < _sp.size(); ++i) {
                //printf("[Rank %i] comm with %i\n", ff_get_rank(), _sp[i].first);
           
                MPI_Irecv(_rbuff[i], _sp[i].second.size(), MPI_DOUBLE, _sp[i].first, 0, _comm, _rq + i);
                MPI_Isend(_sbuff[i], _sp[i].second.size(), MPI_DOUBLE, _sp[i].first, 0, _comm, _rq + _sp.size() + i);
            }
            for(unsigned short i = 0; i < _sp.size(); ++i) {
                int index;
                MPI_Waitany(_sp.size(), _rq, &index, MPI_STATUS_IGNORE);
            }
            MPI_Waitall(_sp.size(), _rq + _sp.size(), MPI_STATUSES_IGNORE);

        }
        inline void pre() {
            std::uniform_int_distribution<int> dist(0, _st / noise);
            std::this_thread::sleep_for(std::chrono::milliseconds(_st + dist(mt)));
        }
        inline void post() {
            std::uniform_int_distribution<int> dist(0, _st / noise);
            std::this_thread::sleep_for(std::chrono::milliseconds(_st + dist(mt)));
        }

        inline void globalMPI() {
            if(_coarse) {
                MPI_Gather(MPI_IN_PLACE, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
                _coarse->Coarse::work();
                MPI_Scatter(_gbuff, _sg, MPI_DOUBLE, MPI_IN_PLACE, 0, MPI_DOUBLE, 0, _comm);
            }
            else {
                MPI_Gather(_gbuff, _sg, MPI_DOUBLE, NULL, 0, MPI_DOUBLE, 0, _comm);
                MPI_Scatter(NULL, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
            }
        }

        inline void IglobalMPI() {
            MPI_Status stat;
            if(_coarse) {
                MPI_Igather(MPI_IN_PLACE, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm, &mgather);
                MPI_Wait(&mgather, &stat);
                _coarse->Coarse::work();
                MPI_Iscatter(_gbuff, _sg, MPI_DOUBLE, MPI_IN_PLACE, 0, MPI_DOUBLE, 0, _comm, &mscatter);
                MPI_Wait(&mscatter, &stat);
            }
            else {
                MPI_Igather(_gbuff, _sg, MPI_DOUBLE, NULL, 0, MPI_DOUBLE, 0, _comm, &mgather);
                MPI_Iscatter(NULL, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm, &mscatter);
            }
        }

        inline void IglobalMPI_wait(){
            MPI_Status stat;
            if (!_coarse){
                //printf("[Rank %i] waiting gather\n", ff_get_rank());
                MPI_Wait(&mgather, &stat);
                //printf("[Rank %i] waiting scatter\n", ff_get_rank());
                MPI_Wait(&mscatter, &stat);
                //printf("[Rank %i] ok\n");
            }
        }

        inline void global() {
            //ff_schedule_h gather, scatter;
            if(_coarse) {
                //_coarse->Coarse::work();
            
                gather = ff_gather(NULL, _gbuff, _sg, sizeof(double), 0, tag_gather++);
                ff_schedule_post(gather, 1);
                ff_schedule_wait(gather);
                
                _coarse->Coarse::work();

                scatter = ff_scatter(_gbuff, NULL, _sg, sizeof(double), 0, tag_scatter++);
                ff_schedule_post(scatter, 1);
                ff_schedule_wait(scatter);
            }
            else {
                gather = ff_gather(_gbuff, NULL, _sg, sizeof(double), 0, tag_gather++);
                ff_schedule_post(gather, 1);
                ff_schedule_wait(gather);
                
                scatter = ff_scatter(NULL, _gbuff,  _sg, sizeof(double), 0, tag_scatter++);
                ff_schedule_post(scatter, 1);
                ff_schedule_wait(scatter);
            }
            ff_schedule_free(scatter);
            ff_schedule_free(gather);
        }


        inline void Iglobal() {
            //ff_schedule_h gather, scatter;
            if(_coarse) {
                //_coarse->Coarse::work();
            
                gather = ff_gather(NULL, _gbuff, _sg, sizeof(double), 0, tag_gather++);
                ff_schedule_post(gather, 1);
                ff_schedule_wait(gather);
                
                _coarse->Coarse::work();

                scatter = ff_scatter(_gbuff, NULL, _sg, sizeof(double), 0, tag_scatter++);
                ff_schedule_post(scatter, 1);
                ff_schedule_wait(scatter);
            }
            else {
                gather = ff_gather(_gbuff, NULL, _sg, sizeof(double), 0, tag_gather++);
                ff_schedule_post(gather, 1);
                //ff_schedule_wait(gather);
                
                scatter = ff_scatter(NULL, _gbuff,  _sg, sizeof(double), 0, tag_scatter++);
                ff_schedule_post(scatter, 1);
                //ff_schedule_wait(scatter);
            }
            //ff_schedule_free(scatter);
            //ff_schedule_free(gather);
        }

        inline void Iglobal_wait(){
            if (!_coarse){
                ff_schedule_wait(scatter);
                ff_schedule_wait(gather);
            }
            ff_schedule_free(scatter);
            ff_schedule_free(gather);
        }
    

        inline void offloadGlobal() {

            ff_schedule_h sched;

            if(_coarse) {
			    sched = ff_container_get_next(gatherc);
                if (sched==FF_FAIL) printf("ERROR!!!\n");
                ff_schedule_satisfy_user_dep(sched);
                //printf("waiting gather\n");
                ff_container_wait(gatherc);
                //printf("gather ok\n");
        
                ff_schedule_free(sched);                

                ff_container_increase_async(gatherc);

                _coarse->Coarse::work();
                
                sched = ff_container_get_next(scatterc);
                //printf("sched: %i\n", sched);
                ff_schedule_satisfy_user_dep(sched);

                //printf("waiting gather\n");
                //do we need this?
                ff_container_wait(scatterc);
                ff_schedule_free(sched);
                ff_container_increase_async(scatterc);
            }
            else {
                //we don't have to do nothing here
            
                //re-fill the container
                int fs = ff_container_flush(scatterc);
                for (int j=0; j<fs; j++) ff_container_increase_async(scatterc);
            
                int fg = ff_container_flush(gatherc);
                for (int j=0; j<fg; j++) ff_container_increase_async(gatherc);
            
                //if (fs>0 || fg>0) printf("[Rank %i] flush: scatter: %i; gather: %i\n", ff_get_rank(), fs, fg);


            }
        }
        inline void coarseDuty(double t) {
            _coarse = new Coarse(t);
            delete [] _gbuff;
            _gbuff = new double[_sg * size()];
            collinfo.userdata = (void *) _gbuff;

            scatterc = ff_container_create(collinfo, create_solo_scatter);
            gatherc = ff_container_create(collinfo, create_solo_gather);
            /*
            ff_container_increase_async(scatterc);
            ff_container_increase_async(gatherc);
          
            ff_container_start(scatterc);
            ff_container_start(gatherc);
            */
        }
        inline void iterate() {
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            global();
            post();
        }


        inline void Iiterate() {
            //printf("[Rank %i] start it\n", ff_get_rank());
            Iglobal();
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            //printf("[Rank %i] wait it\n", ff_get_rank());
            Iglobal_wait();
            post();
        }

        inline void iterateMPI() {
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            globalMPI();
            post();
        }


        inline void IiterateMPI() {
            IglobalMPI();
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            IglobalMPI_wait();
            post();
        }

        inline void offloadIterate() {
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            offloadGlobal();
            post();
        }

        inline void emptyIterate() {
            if (!_coarse || size() == 1) work();
            pre();
            if (!_coarse) communicate();
            post();
        }


};


int main(int argc, char** argv) {
    ///MPI_Init(&argc, &argv);
    ff_init(&argc, &argv);
    if(argc < 9 || argc > 10) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(rank == 0)
            std::cout << "Usage: fine wait, coarse wait, noise, Nx, Ny, overlap, transfer, smoothing, Nz" << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
        //MPI_Finalize();
        ff_finalize();
        return 1;
    }




  
    noise = std::stoi(argv[3]);
    int Nx = std::stoi(argv[4]);
    int Ny = std::stoi(argv[5]);
    int Nz = (argc == 10 ? std::stoi(argv[9]) : 0);
    int overlap = std::stoi(argv[6]);

    Fine A(MPI_COMM_WORLD, std::stoi(argv[1]), std::stoi(argv[7]), std::stoi(argv[8]));

    int rankWorld = A.rank();
    int sizeWorld = A.size();


    mt = std::mt19937(SEED*A.rank());
        /*
    for (int i=0; i<20; i++){
        std::uniform_int_distribution<int> dist(-10, +10);
        printf("[Rank %i] i: %i; time:%i\n", ff_get_rank(), i, dist(mt));
        if (i==10) mt = std::mt19937(SEED*A.rank());
    }*/



    int xGrid = int(sqrt(sizeWorld));
    while(sizeWorld % xGrid != 0)
        --xGrid;
    int yGrid = sizeWorld / xGrid;

    int y = rankWorld / xGrid;
    int x = rankWorld - xGrid * y;

    int iStart = std::max(x * Nx / xGrid - overlap, 0);
    int iEnd   = std::min((x + 1) * Nx / xGrid + overlap, Nx);
    int jStart = std::max(y * Ny / yGrid - overlap, 0);
    int jEnd   = std::min((y + 1) * Ny / yGrid + overlap, Ny);
    int ndof   = (iEnd - iStart) * (jEnd - jStart);
    std::vector<std::pair<int, std::vector<int>>> mapping;
    mapping.reserve(8);
    if(jStart != 0) { // this subdomain doesn't touch the bottom side of %*\color{DarkGreen}{$\Omega$}*)
        if(iStart != 0 && rankWorld - xGrid - 1 != 0) { // this subd. doesn't touch the left side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid - 1, std::vector<int>()); // subd. on the lower left corner is a neighbor
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(i - iStart + (iEnd - iStart) * j);
        }

        if (rankWorld - xGrid != 0){
            mapping.emplace_back(rankWorld - xGrid, std::vector<int>()); // subd. below is a neighbor
            mapping.back().second.reserve(2 * overlap * (iEnd - iStart));
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iEnd; ++i)
                    mapping.back().second.emplace_back(i - iStart + (iEnd - iStart) * j);
        }
        if(iEnd != Nx && rankWorld - xGrid + 1 != 0) { // this subd. doesn't touch the right side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid + 1, std::vector<int>()); // subd. on the lower right corner is a neighbor
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int i = 0; i < 2 * overlap; ++i)
                for(int j = 0; j < 2 * overlap; ++j)
                    mapping.back().second.emplace_back((iEnd - iStart) * (i + 1) - 2 * overlap + j);
        }
    }
    if(iStart != 0 && rankWorld - 1 != 0) {
        mapping.emplace_back(rankWorld - 1, std::vector<int>());
        mapping.back().second.reserve(2 * overlap * (jEnd - jStart));
        for(int i = jStart; i < jEnd; ++i)
            for(int j = 0; j < 2 * overlap; ++j)
                mapping.back().second.emplace_back(j + (i - jStart) * (iEnd - iStart));
    }
    if(iEnd != Nx && rankWorld + 1 != 0) {
        mapping.emplace_back(rankWorld + 1, std::vector<int>());
        mapping.back().second.reserve(2 * overlap * (jEnd - jStart));
        for(int i = jStart; i < jEnd; ++i)
            for(int j = 0; j < 2 * overlap; ++j)
                mapping.back().second.emplace_back((iEnd - iStart) * (i + 1 - jStart) - 2 * overlap + j);
    }
    if(jEnd != Ny) {
        if(iStart != 0 && rankWorld + xGrid - 1 != 0) {
            mapping.emplace_back(rankWorld + xGrid - 1, std::vector<int>());
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j);
        }
        if (rankWorld + xGrid != 0){
            mapping.emplace_back(rankWorld + xGrid, std::vector<int>());
            mapping.back().second.reserve(2 * overlap * (iEnd - iStart));
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iEnd; ++i)
                    mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j);
        }
        if(iEnd != Nx && rankWorld + xGrid + 1 != 0) {
            mapping.emplace_back(rankWorld + xGrid + 1, std::vector<int>());
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j + (iEnd - iStart - 2 * overlap));
        }
    }
    A.neighbors(mapping);

    printf("[Rank %i] neighbors: %i\n", ff_get_rank(), mapping.size());

    if(A.rank() == 0)
        A.coarseDuty(std::stoi(argv[2]));

    int iter = ITER;
    int warmup = 20;

    /* WARMUP */
    for(int i = 0; i < warmup; ++i) {
        A.Iiterate();
    }
    MPI_Barrier(MPI_COMM_WORLD);

    mt = std::mt19937(SEED*A.rank());

    /*** FFLIB: synch ***/
    if(A.rank() == 0)
        std::cout << "Starting synch test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    auto start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        //if(rankWorld == 0) std::cout << "Iteration " << i << std::endl;
        A.Iiterate();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto t1 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    MPI_Barrier(MPI_COMM_WORLD);

    /*** FFLIB: async ***/
    int initfill = ASYNCH_DEGREE;
    if(A.rank() == 0){
        initfill=1;
        std::cout << "Starting asynch test" << std::endl;
    }
    MPI_Barrier(MPI_COMM_WORLD);

    //printf("[Rank %i] starting async\n", ff_get_rank());
    for (int i=0; i<initfill; i++) {
        ff_container_increase_async(A.scatterc);
        ff_container_increase_async(A.gatherc);
    }

    ff_container_start(A.scatterc);
    ff_container_start(A.gatherc);

    
    //printf("[Rank %i] warmup start; initfill: %i\n", ff_get_rank(), initfill);
    int term=1;
    /* WARM UP */
    for (int i=0; i<ASYNCH_DEGREE; i++){
        A.offloadIterate();
    }

    for (int i=0; i<initfill; i++) {
        ff_container_increase_async(A.scatterc);
        ff_container_increase_async(A.gatherc);
    }
    //MPI_Barrier(MPI_COMM_WORLD);
    ff_schedule_h barrier = ff_broadcast(&term, 1, sizeof(int), 0, 0);
    ff_schedule_post(barrier, 1);
    ff_schedule_wait(barrier);
    ff_schedule_free(barrier);
    /* END WARM UP */
    
    mt = std::mt19937(SEED*A.rank());

    //printf("[Rank %i] warmup complete\n", ff_get_rank());
    start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        //printf("[Rank %i] iteration: %i\n", ff_get_rank(), i);
        //if (rankWorld == 0 || 1==1) std::cout << "Offload iteration " << i << std::endl;
        A.offloadIterate();
    }
    //printf("[Rank %i] entering barrier\n", ff_get_rank());

    //MPI_Barrier(MPI_COMM_WORLD);

    term=tag_scatter;
    barrier = ff_broadcast(&term, 1, sizeof(int), 0, 0);
    ff_schedule_post(barrier, 1);

    //printf("[Rank %i] sink phase\n", ff_get_rank());
    while (ff_schedule_test(barrier)!=FF_SUCCESS){
        int fs = ff_container_flush(A.scatterc);
        //if (fs>0) printf("term fs: %i\n", fs);
        for (int j=0; j<fs; j++) ff_container_increase_async(A.scatterc);
            
        int fg = ff_container_flush(A.gatherc);
        //if (fg>0) printf("term fg: %i\n", fg);
        for (int j=0; j<fg; j++) ff_container_increase_async(A.gatherc);
        
    }

    //printf("[Rank %i] term: %i; tag_scatter: %i\n", ff_get_rank(), term, tag_scatter);
    end_time = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    //printf("[Rank %i] Complete\n", ff_get_rank());

    MPI_Barrier(MPI_COMM_WORLD);
    /*** MPI ***/
    mt = std::mt19937(SEED*A.rank());

    if(A.rank() == 0)
        std::cout << "Starting MPI test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        
        A.IiterateMPI();
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    


    /* empty iterate */

    MPI_Barrier(MPI_COMM_WORLD);
    /*** MPI ***/
    mt = std::mt19937(SEED*A.rank());

    if(A.rank() == 0)
        std::cout << "Starting Empty test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        
        A.emptyIterate();
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto t4 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();


    //ff_barrier();

    //int t3=0;
    for(int i = 0; i < A.size(); ++i) {
        MPI_Barrier(MPI_COMM_WORLD);
        if(i == A.rank()){
            std::cout << A.size() << "\t" << A.getSG() << "\t" << A.rank() << "\t" << "SYNC" << "\t" << t1 << std::endl;
            std::cout << A.size() << "\t" << A.getSG() << "\t" << A.rank() << "\t" << "SOLO" << "\t" << t2 << std::endl;
            std::cout << A.size() << "\t" << A.getSG() << "\t" << A.rank() << "\t" << "MPI" << "\t" << t3 << std::endl;
            std::cout << A.size() << "\t" << A.getSG() << "\t" << A.rank() << "\t" << "EMPTY" << "\t" << t4 << std::endl;
            //std::cout << t1 << " \t" << t2 << " \t" << t3 << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    
    return 0;
}
