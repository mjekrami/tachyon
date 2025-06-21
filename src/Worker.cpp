#include "Tachyon.h"
#include "Common.h"
#include "CompressedBlock.h"
#include <mpi.h>
#include <vector>
#include <iostream>
#include <map>

namespace tachyon {

void run_worker(int rank) {
    std::cout << "Worker (Rank " << rank << ") started." << std::endl;
    std::map<uint32_t, std::vector<CompressedBlock>> local_store;

    // Phase 2: Compression Loop
    while (true) {
        MPI_Status status;
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if (status.MPI_TAG == 1) { // Termination signal
            MPI_Recv(nullptr, 0, MPI_BYTE, 0, 1, MPI_COMM_WORLD, &status);
            break;
        }

        int count;
        MPI_Get_count(&status, MPI_BYTE, &count);
        std::vector<RawTick> received_ticks(count / sizeof(RawTick));
        MPI_Recv(received_ticks.data(), count, MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);

        if (!received_ticks.empty()) {
            uint32_t symbol = received_ticks[0].symbol_id;
            local_store[symbol].emplace_back();
            local_store[symbol].back().compress(received_ticks);
        }
    }
    std::cout << "Worker " << rank << " finished compression. Stored " << local_store.size() << " unique symbols." << std::endl;
    
    // Phase 3: Query Execution
    Query q;
    MPI_Bcast(&q, sizeof(Query), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    PartialResult my_result;
    if (local_store.count(q.symbol_id)) {
        for (const auto& block : local_store.at(q.symbol_id)) {
            if (block.overlaps_with(q.start_time, q.end_time)) {
                CompressedBlockScanner scanner(block);
                while(scanner.has_next()) {
                    ScannedTickData tick_data = scanner.next();
                    if (tick_data.timestamp >= q.start_time && tick_data.timestamp <= q.end_time) {
                        my_result.sum += (tick_data.ask_price - tick_data.bid_price);
                        my_result.count++;
                    }
                }
            }
        }
    }
    
    // Phase 4: Send result back
    MPI_Send(&my_result, sizeof(PartialResult), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
}
}
