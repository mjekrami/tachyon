#include <mpi.h>
#include <iostream>
#include "Tachyon.h"

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_size < 2) {
        if (world_rank == 0) {
            std::cerr << "Tachyon-Tick requires at least 2 processes (1 Coordinator, 1+ Workers)." << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    if (world_rank == 0) {
        tachyon::run_coordinator(world_size);
    } else {
        tachyon::run_worker(world_rank);
    }

    MPI_Finalize();
    return 0;
}
