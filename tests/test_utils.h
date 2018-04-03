#ifndef __TEST_UTILS_H__
#define __TEST_UTILS_H__

void fill(char * data, int size, int seed){
    srand(seed);
    for (int i=0; i<size-1; i++) {
        data[i] = 'a' + (rand() % 21);
        //printf("fill: %i\n", data[i]);
    }
    data[size-1] = '\0';
}


int check(char * data, int size, int seed){
    srand(seed);
    int correct=1;
    for (int i=0; i<size-1; i++){
        char tmp = 'a' + (rand() % 21);
        if (data[i] != tmp) correct=0;
        //printf("check: data: %i; rand: %i\n", data[i], tmp);
    }
    return correct;
}

MPI_File createFilePerProcess(char* dir, char *prefixName) {
        MPI_File filehandle;
        char line[256];
        int ier;
        char fn[200];
        char timestr[25];
        //init output file

        time_t mytime;
        struct tm* tm_info;
        mytime = time(NULL);
        tm_info = localtime(&mytime);
        strftime(timestr, 25, "%Y-%m-%d_%H-%M", tm_info);
        snprintf(&fn[0], 100, "%s%s-%s.pid-%d.out\0",dir, prefixName, timestr, getpid());
        ier = MPI_File_open(MPI_COMM_SELF, &fn[0], MPI_MODE_EXCL + MPI_MODE_CREATE + MPI_MODE_WRONLY, MPI_INFO_NULL, &filehandle);

        if (ier != MPI_SUCCESS) {
                fprintf(stderr,"Error at open file %s for writing the timing values. The file probably already exists.\nWill now abort.\n", fn);
                MPI_Abort(MPI_COMM_WORLD, 1);
        }
        return filehandle;
}

#endif //__TEST_UTILS_H__
