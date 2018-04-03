#include <string>
#include <iostream>
#include <mpi.h>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

static std::random_device rd;
static std::mt19937 mt(rd());
static double noise = 1.0;

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
    public:
        Fine(MPI_Comm comm, int t, int nu, int smooth) : _time(t), _sg(nu), _gbuff(new double[nu]), _coarse(), _rq(), _st(smooth) {
            MPI_Comm_dup(comm, &_comm);
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
            if(!finalized)
                MPI_Finalize();
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
        inline void global() {
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
        inline void coarseDuty(double t) {
            _coarse = new Coarse(t);
            delete [] _gbuff;
            _gbuff = new double[_sg * size()];
        }
        inline void iterate() {
            work();
            pre();
            communicate();
            global();
            post();
        }
};

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    if(argc < 9 || argc > 10) {
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if(rank == 0)
            std::cout << "Usage: fine wait, coarse wait, noise, Nx, Ny, overlap, transfer, smoothing, Nz" << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Finalize();
        return 1;
    }

    noise = std::stoi(argv[3]);
    int Nx = std::stoi(argv[4]);
    int Ny = std::stoi(argv[5]);
    int Nz = (argc == 10 ? std::stoi(argv[9]) : 1);
    int overlap = std::stoi(argv[6]);

    Fine A(MPI_COMM_WORLD, std::stoi(argv[1]), std::stoi(argv[7]), std::stoi(argv[8]));

    int rankWorld = A.rank();
    int sizeWorld = A.size();

    int xGrid = std::pow(sizeWorld, 1.0 / static_cast<double>(2 + (Nz != 1 ? 1 : 0)) + 1e-8);
    while(sizeWorld % xGrid != 0)
        --xGrid;
    int yGrid = int(std::pow(sizeWorld / xGrid, 1.0 / static_cast<double>(1 + (Nz != 1 ? 1 : 0))));
    double tmp = sizeWorld / xGrid;
    if(Nz > 1)
        while(int(tmp) % yGrid != 0)
            --yGrid;
    int zGrid = Nz == 1 ? 1 : int(tmp / yGrid);
    if(rankWorld == 0)
        std::cout << xGrid << " " << yGrid << " " << zGrid << std::endl;

    int z = rankWorld / (xGrid * yGrid);
    int y = rankWorld / xGrid - yGrid * z;
    int x = rankWorld - xGrid * y - yGrid * xGrid * z;

    int iStart = std::max(x * Nx / xGrid - overlap, 0);
    int iEnd   = std::min((x + 1) * Nx / xGrid + overlap, Nx);
    int jStart = std::max(y * Ny / yGrid - overlap, 0);
    int jEnd   = std::min((y + 1) * Ny / yGrid + overlap, Ny);
    int kStart = std::max(z * Nz / zGrid - overlap, 0);
    int kEnd   = std::min((z + 1) * Nz / zGrid + overlap, Nz);
    int ndof   = (iEnd - iStart) * (jEnd - jStart) * (kEnd - kStart);
    std::vector<std::pair<int, std::vector<int>>> mapping;
    mapping.reserve(Nz == 1 ? 8 : 26);
    if(jStart != 0) { // this subdomain doesn't touch the bottom side of %*\color{DarkGreen}{$\Omega$}*)
        if(iStart != 0) { // this subd. doesn't touch the left side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid - 1, std::vector<int>()); // subd. on the lower left corner is a neighbor
            mapping.back().second.resize(4 * overlap * overlap * (kEnd - kStart));
            if(kStart != 0) {
                mapping.emplace_back(rankWorld - xGrid - xGrid * yGrid - 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
            if(kEnd != Nz) {
                mapping.emplace_back(rankWorld - xGrid + xGrid * yGrid - 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
        }
        mapping.emplace_back(rankWorld - xGrid, std::vector<int>()); // subd. below is a neighbor
        mapping.back().second.resize(2 * overlap * (iEnd - iStart) * (kEnd - kStart));
        if(kStart != 0) {
            mapping.emplace_back(rankWorld - xGrid - xGrid * yGrid, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (iEnd - iStart));
        }
        if(kEnd != Nz) {
            mapping.emplace_back(rankWorld - xGrid + xGrid * yGrid, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (iEnd - iStart));
        }
        if(iEnd != Nx) { // this subd. doesn't touch the right side of %*\color{DarkGreen}{$\Omega$}*)
            mapping.emplace_back(rankWorld - xGrid + 1, std::vector<int>()); // subd. on the lower right corner is a neighbor
            mapping.back().second.resize(4 * overlap * overlap * (kEnd - kStart));
            if(kStart != 0) {
                mapping.emplace_back(rankWorld - xGrid - xGrid * yGrid + 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
            if(kEnd != Nz) {
                mapping.emplace_back(rankWorld - xGrid + xGrid * yGrid + 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
        }
    }
    if(kStart != 0) {
        mapping.emplace_back(rankWorld - xGrid * yGrid, std::vector<int>());
        mapping.back().second.resize(2 * overlap * (jEnd - jStart) * (iEnd - iStart));
    }
    if(kEnd != Nz) {
        mapping.emplace_back(rankWorld + xGrid * yGrid, std::vector<int>());
        mapping.back().second.resize(2 * overlap * (jEnd - jStart) * (iEnd - iStart));
    }
    if(iStart != 0) {
        mapping.emplace_back(rankWorld - 1, std::vector<int>());
        mapping.back().second.resize(2 * overlap * (jEnd - jStart) * (kEnd - kStart));
        if(kStart != 0) {
            mapping.emplace_back(rankWorld - xGrid * yGrid - 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (jEnd - jStart));
        }
        if(kEnd != Nz) {
            mapping.emplace_back(rankWorld + xGrid * yGrid - 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (jEnd - jStart));
        }
    }
    if(iEnd != Nx) {
        mapping.emplace_back(rankWorld + 1, std::vector<int>());
        mapping.back().second.resize(2 * overlap * (jEnd - jStart) * (kEnd - kStart));
        if(kStart != 0) {
            mapping.emplace_back(rankWorld - xGrid * yGrid + 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (jEnd - jStart));
        }
        if(kEnd != Nz) {
            mapping.emplace_back(rankWorld + xGrid * yGrid + 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (jEnd - jStart));
        }
    }
    if(jEnd != Ny) {
        if(iStart != 0) {
            mapping.emplace_back(rankWorld + xGrid - 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (kEnd - kStart));
            if(kStart != 0) {
                mapping.emplace_back(rankWorld + xGrid - xGrid * yGrid - 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
            if(kEnd != Nz) {
                mapping.emplace_back(rankWorld + xGrid + xGrid * yGrid - 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
        }
        mapping.emplace_back(rankWorld + xGrid, std::vector<int>());
        mapping.back().second.resize(2 * overlap * (iEnd - iStart) * (kEnd - kStart));
        if(kStart != 0) {
            mapping.emplace_back(rankWorld + xGrid - xGrid * yGrid, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (iEnd - iStart));
        }
        if(kEnd != Nz) {
            mapping.emplace_back(rankWorld + xGrid + xGrid * yGrid, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (iEnd - iStart));
        }
        if(iEnd != Nx) {
            mapping.emplace_back(rankWorld + xGrid + 1, std::vector<int>());
            mapping.back().second.resize(4 * overlap * overlap * (kEnd - kStart));
            if(kStart != 0) {
                mapping.emplace_back(rankWorld + xGrid - xGrid * yGrid + 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
            if(kEnd != Nz) {
                mapping.emplace_back(rankWorld + xGrid + xGrid * yGrid + 1, std::vector<int>());
                mapping.back().second.resize(8 * overlap * overlap * overlap);
            }
        }
    }
    /*
    for(int i = 0; i < sizeWorld; ++i) {
        MPI_Barrier(MPI_COMM_WORLD);
        if(i == rankWorld) {
            std::cout << x << " " << y << " " << z << std::endl;
            std::cout << mapping.size() << "\t ";
            for(int j = 0; j < mapping.size(); ++j)
                std::cout << mapping[j].first << " ";
            std::cout << std::endl;
            std::cout << "[" << iStart << "; " << iEnd << "] x [" << jStart << "; " << jEnd << "] x [" << kStart << "; " << kEnd << "]" << std::endl;
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
    */
    //
    A.neighbors(mapping);

    if(A.rank() == 0)
        A.coarseDuty(std::stoi(argv[2]));

    int iter = 100;

    if(A.rank() == 0)
        std::cout << "Starting MPI test" << std::endl;
    MPI_Barrier(MPI_COMM_WORLD);
    auto start_time = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < iter; ++i)
        A.iterate();
    auto end_time = std::chrono::high_resolution_clock::now();

    for(int i = 0; i < A.size(); ++i) {
        MPI_Barrier(MPI_COMM_WORLD);
        if(i == A.rank())
            std::cout << std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() << std::endl;
        MPI_Barrier(MPI_COMM_WORLD);
    }
    return 0;
}
