#include "Tachyon.h"
#include "Common.h"
#include "TickGenerator.h"
#include <mpi.h>
#include <vector>
#include <iostream>
#include <map>
#include <string>

namespace tachyon {

void run_coordinator(int world_size) {
    std::cout << "Coordinator (Rank 0) started." << std::endl;
    int num_workers = world_size - 1;

    // Phase 1: Setup
    std::map<std::string, uint32_t> symbol_map = {{"AAPL", 0}, {"GOOG", 1}};

    // Phase 2: Data Ingestion & Distribution
    TickGenerator generator(symbol_map);
    const int BATCHES_TO_SEND = 10;
    const int TICKS_PER_BATCH = 4096;

    for (int i = 0; i < BATCHES_TO_SEND; ++i) {
        int target_worker = (i % num_workers) + 1;
        std::vector<RawTick> batch = generator.generate_batch(TICKS_PER_BATCH);
        
        MPI_Send(batch.data(), batch.size() * sizeof(RawTick), MPI_BYTE, 
                 target_worker, 0, MPI_COMM_WORLD);
    }

    // Signal workers that data sending is done
    for (int i = 1; i < world_size; ++i) {
        MPI_Send(nullptr, 0, MPI_BYTE, i, 1, MPI_COMM_WORLD); // Use tag 1 for termination signal
    }
    std::cout << "Coordinator: All data sent. Starting query phase." << std::endl;

    // Phase 3: Querying
    Query q = {QueryType::AVG_SPREAD, 1000, 5000000000ULL, 0 /*AAPL_ID*/};
    MPI_Bcast(&q, sizeof(Query), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    // Phase 4: Aggregation
    PartialResult final_result;
    for (int i = 1; i < world_size; ++i) {
        PartialResult partial;
        MPI_Recv(&partial, sizeof(PartialResult), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        final_result.sum += partial.sum;
        final_result.count += partial.count;
    }

    if (final_result.count > 0) {
        double avg_spread = final_result.sum / final_result.count;
        std::cout << "--------------------------------\n";
        std::cout << "Final Query Result:\n";
        std::cout << "Average Spread: " << avg_spread << "\n";
        std::cout << "Based on " << final_result.count << " ticks.\n";
        std::cout << "--------------------------------\n";
    } else {
        std::cout << "No data found for the given query." << std::endl;
    }
}
}
