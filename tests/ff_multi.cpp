#include <string>
#include <iostream>
#include <mpi.h>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

extern "C"
{
#include "ff.h"
#include "ff_collectives.h"
}

static std::random_device rd;
static std::mt19937 mt(rd());
static double noise = 1.0;

int ccount=500;
static int tagg=15;
static int tags=1015;

ff_schedule_h create_singleactivated_gather(ff_sched_info info){
    //double * sndbuff = ((double **) info.userdata)[0];
    //double * recbuff = ((double **) info.userdata)[1];
    //printf("creating gather\n");
    return ff_oneside_gather(info.root, info.userdata, info.userdata, info.unitsize, info.count, 5); //ccount++);
}

ff_schedule_h create_singleactivated_scatter(ff_sched_info info){
    //double * sndbuff = ((double **) info.userdata)[0];
    //double * recbuff = ((double **) info.userdata)[1];
    //printf("creating scatter\n");
    return ff_scatter(info.userdata, info.userdata, info.count, info.unitsize, info.root, 10); //ccount++);
}

class Coarse {
    private:
        int _time;
    public:
        Coarse(int t) : _time(t) { }
        inline void work() {
            std::uniform_int_distribution<int> dist(-_time / noise, _time / noise);
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
        ff_container_h scatterc;
        ff_container_h gatherc;
        ff_sched_info collinfo;
        int adegree;

    public:
        Fine(MPI_Comm comm, int t, int nu, int smooth) : _time(t), _sg(nu), _gbuff(new double[nu]), _coarse(), _rq(), _st(smooth) {
            MPI_Comm_dup(comm, &_comm);


            collinfo.count=_sg;
            collinfo.unitsize=sizeof(double);
            collinfo.tag = 1; //meaningless
            collinfo.root=0;
            adegree=5;

            if (rank()!=0){
                collinfo.userdata = (void *) _gbuff;

                scatterc = ff_container_create(collinfo, create_singleactivated_scatter);
                gatherc = ff_container_create(collinfo, create_singleactivated_gather);
                for (int i=0; i<adegree; i++) {
                    ff_container_increase_async(scatterc);
                    ff_container_increase_async(gatherc);
                }
                ff_container_start(scatterc);
                ff_container_start(gatherc);
            }
        }
        ~Fine() {
            delete [] _gbuff;
            delete [] _rq;
            if(!_rbuff.empty())
                delete [] _rbuff[0];
            delete _coarse;
            MPI_Comm_free(&_comm);
            int finalized;
            MPI_Finalized(&finalized);
            if(!finalized){
                MPI_Finalize();
                ff_finalize();
            }
        }
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
            std::uniform_int_distribution<int> dist(-_time / noise, _time / noise);
            std::this_thread::sleep_for(std::chrono::milliseconds(_time + dist(mt)));
        }
        inline void communicate() {
            for(unsigned short i = 0; i < _sp.size(); ++i) {
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
            std::uniform_int_distribution<int> dist(-_st / noise, _st / noise);
            std::this_thread::sleep_for(std::chrono::milliseconds(_st + dist(mt)));
        }
        inline void post() {
            std::uniform_int_distribution<int> dist(-_st / noise, _st / noise);
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

        inline void global() {
            ff_schedule_h gather, scatter;
            if(_coarse) {
                //MPI_Gather(MPI_IN_PLACE, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
                printf("start gather: %i %i\n", tagg, tags);

                //gather = ff_gather3(0, NULL, _gbuff, sizeof(double), _sg, tagg++);
                gather = ff_gather3(NULL, _gbuff, _sg, sizeof(double), 0, tagg++);
                ff_schedule_post(gather, 1);
                ff_schedule_wait(gather);
                printf("end gather\n");
                
                _coarse->Coarse::work();
                //MPI_Scatter(_gbuff, _sg, MPI_DOUBLE, MPI_IN_PLACE, 0, MPI_DOUBLE, 0, _comm);
                //scatter = ff_scatter(_gbuff, NULL, _sg, sizeof(double), 0, tags++);
                scatter = ff_scatter3(_gbuff, NULL, _sg, sizeof(double), 0, tags++);
                ff_schedule_post(scatter, 1);
                ff_schedule_wait(scatter);
            }
            else {
                //printf("sg: %i\n", _sg);
                //MPI_Gather(_gbuff, _sg, MPI_DOUBLE, NULL, 0, MPI_DOUBLE, 0, _comm);
                //gather = ff_gather2(0, _gbuff, NULL, sizeof(double), _sg, tagg++);
                gather = ff_gather3(_gbuff, NULL, _sg, sizeof(double), 0, tagg++);
                ff_schedule_post(gather, 1);
                ff_schedule_wait(gather);
                
                //MPI_Scatter(NULL, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
                scatter = ff_scatter3(NULL, _gbuff,  _sg, sizeof(double), 0, tags++);
                ff_schedule_post(scatter, 1);
                ff_schedule_wait(scatter);
            }
            ff_schedule_free(scatter);
            ff_schedule_free(gather);
        }

        /*
        inline void offloadGlobal() {

            if(_coarse) {
                MPI_Gather(MPI_IN_PLACE, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
                _coarse->Coarse::work();
                \:MPI_Scatter(_gbuff, _sg, MPI_DOUBLE, MPI_IN_PLACE, 0, MPI_DOUBLE, 0, _comm);
            }
            else {
                MPI_Gather(_gbuff, _sg, MPI_DOUBLE, NULL, 0, MPI_DOUBLE, 0, _comm);
                MPI_Scatter(NULL, 0, MPI_DOUBLE, _gbuff, _sg, MPI_DOUBLE, 0, _comm);
            }

        }*/

        inline void offloadGlobal() {

            ff_schedule_h sched;

            if(_coarse) {
			    sched = ff_container_get_next(gatherc);
                ff_schedule_satisfy_user_dep(sched);
                //printf("waiting scatter\n");
                ff_schedule_wait(sched);

                

                _coarse->Coarse::work();

                sched = ff_container_get_next(scatterc);
                ff_schedule_satisfy_user_dep(sched);

                //printf("waiting gather\n");
                //do we need this?
                ff_schedule_wait(sched);

            }
            else {
                //we don't have to do nothing here
            }

            //re-fill the container
            int fs = ff_container_flush(scatterc);
            for (int j=0; j<fs; j++) ff_container_increase_async(scatterc);
            
            int fg = ff_container_flush(gatherc);
            for (int j=0; j<fg; j++) ff_container_increase_async(gatherc);
            
            //printf("flush: scatter: %i; gather: %i\n", fs, fg);

        }
        inline void coarseDuty(double t) {
            _coarse = new Coarse(t);
            delete [] _gbuff;
            _gbuff = new double[_sg * size()];
            
            collinfo.userdata = (void *) _gbuff;

            scatterc = ff_container_create(collinfo, create_singleactivated_scatter);
            gatherc = ff_container_create(collinfo, create_singleactivated_gather);
            for (int i=0; i<adegree; i++) {
                ff_container_increase_async(scatterc);
                ff_container_increase_async(gatherc);
            }
            ff_container_start(scatterc);
            ff_container_start(gatherc);
        }
        inline void iterate() {
            work();
            pre();
            communicate();
            global();
            post();
        }

        inline void iterateMPI() {
            work();
            pre();
            communicate();
            globalMPI();
            post();
        }

        inline void offloadIterate() {
            work();
            communicate();
            pre();
            offloadGlobal();
            post();
        }
};

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    ff_init();
    if(argc < 9 || argc > 10) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(rank == 0)
            std::cout << "Usage: fine wait, coarse wait, noise, Nx, Ny, overlap, transfer, smoothing, Nz" << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
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
        if(iStart != 0) { // this subd. doesn't touch the left side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid - 1, std::vector<int>()); // subd. on the lower left corner is a neighbor
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(i - iStart + (iEnd - iStart) * j);
        }
        mapping.emplace_back(rankWorld - xGrid, std::vector<int>()); // subd. below is a neighbor
        mapping.back().second.reserve(2 * overlap * (iEnd - iStart));
        for(int j = 0; j < 2 * overlap; ++j)
            for(int i = iStart; i < iEnd; ++i)
                mapping.back().second.emplace_back(i - iStart + (iEnd - iStart) * j);
        if(iEnd != Nx) { // this subd. doesn't touch the right side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid + 1, std::vector<int>()); // subd. on the lower right corner is a neighbor
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int i = 0; i < 2 * overlap; ++i)
                for(int j = 0; j < 2 * overlap; ++j)
                    mapping.back().second.emplace_back((iEnd - iStart) * (i + 1) - 2 * overlap + j);
        }
    }
    if(iStart != 0) {
        mapping.emplace_back(rankWorld - 1, std::vector<int>());
        mapping.back().second.reserve(2 * overlap * (jEnd - jStart));
        for(int i = jStart; i < jEnd; ++i)
            for(int j = 0; j < 2 * overlap; ++j)
                mapping.back().second.emplace_back(j + (i - jStart) * (iEnd - iStart));
    }
    if(iEnd != Nx) {
        mapping.emplace_back(rankWorld + 1, std::vector<int>());
        mapping.back().second.reserve(2 * overlap * (jEnd - jStart));
        for(int i = jStart; i < jEnd; ++i)
            for(int j = 0; j < 2 * overlap; ++j)
                mapping.back().second.emplace_back((iEnd - iStart) * (i + 1 - jStart) - 2 * overlap + j);
    }
    if(jEnd != Ny) {
        if(iStart != 0) {
            mapping.emplace_back(rankWorld + xGrid - 1, std::vector<int>());
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j);
        }
        mapping.emplace_back(rankWorld + xGrid, std::vector<int>());
        mapping.back().second.reserve(2 * overlap * (iEnd - iStart));
        for(int j = 0; j < 2 * overlap; ++j)
            for(int i = iStart; i < iEnd; ++i)
                mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j);
        if(iEnd != Nx) {
            mapping.emplace_back(rankWorld + xGrid + 1, std::vector<int>());
            mapping.back().second.reserve(4 * overlap * overlap);
            for(int j = 0; j < 2 * overlap; ++j)
                for(int i = iStart; i < iStart + 2 * overlap; ++i)
                    mapping.back().second.emplace_back(ndof - 2 * overlap * (iEnd - iStart) + i - iStart + (iEnd - iStart) * j + (iEnd - iStart - 2 * overlap));
        }
    }
    A.neighbors(mapping);

    if(A.rank() == 0)
        A.coarseDuty(std::stoi(argv[2]));

    int iter = 100;

    /*** FFLIB: synch ***/
    if(A.rank() == 0)
        std::cout << "Starting synch test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    auto start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        if(rankWorld == 0) std::cout << "Iteration " << i << std::endl;
        A.iterate();
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto t1 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();



    /*** FFLIB: async ***/
    if(A.rank() == 0)
        std::cout << "Starting asynch test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        //if (rankWorld == 0 || 1==1) std::cout << "Offload iteration " << i << std::endl;
        A.offloadIterate();
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();


    /*** MPI ***/
    if(A.rank() == 0)
        std::cout << "Starting MPI test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i) {
        //if (rankWorld == 0 || 1==1) std::cout << "Offload iteration " << i << std::endl;
        A.iterateMPI();
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto t3 = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();


    ff_barrier();

    for(int i = 0; i < A.size(); ++i) {
        MPI_Barrier(MPI_COMM_WORLD);
        if(i == A.rank())
            std::cout << t1 << " \t" << t2 << " \t" << t3 << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
    }
    return 0;
}
