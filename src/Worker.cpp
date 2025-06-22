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
    
#include <limits> // Required for std::numeric_limits
#include <algorithm> // Required for std::max/min

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
    std::cout << "Worker " << rank << " finished compression. Stored " << local_store.size() << " unique symbols for rank " << rank << "." << std::endl;
    if (local_store.empty()) {
         std::cout << "Worker " << rank << " has no local data after compression loop." << std::endl;
    } else {
        for(const auto& pair : local_store) {
            std::cout << "Worker " << rank << " has " << pair.second.size() << " blocks for symbol " << pair.first << std::endl;
        }
    }

    // Phase 3: Query Execution
    Query q;
    MPI_Bcast(&q, sizeof(Query), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    PartialResult my_result;
    my_result.type = q.type;
    my_result.sum = 0.0;
    my_result.count = 0;
    my_result.ohlc.is_set = false;
    my_result.ohlc.low = std::numeric_limits<double>::max();
    my_result.ohlc.high = std::numeric_limits<double>::min(); // Corrected initialization
    my_result.ohlc.open_ts_for_agg = std::numeric_limits<uint64_t>::max();
    my_result.ohlc.close_ts_for_agg = 0;


    if (local_store.count(q.symbol_id)) {
        std::cout << "Worker " << rank << " processing query for symbol " << q.symbol_id << std::endl;
        for (const auto& block : local_store.at(q.symbol_id)) {
            if (block.overlaps_with(q.start_time, q.end_time)) {
                CompressedBlockScanner scanner(block);
                while(scanner.has_next()) {
                    ScannedTickData tick_data = scanner.next();
                    if (tick_data.timestamp >= q.start_time && tick_data.timestamp <= q.end_time) {

                        switch (q.type) {
                            case QueryType::AVG_SPREAD:
                                my_result.sum += (tick_data.ask_price - tick_data.bid_price);
                                my_result.count++;
                                break;
                            case QueryType::VWAP:
                                {
                                    double mid_price = (tick_data.bid_price + tick_data.ask_price) / 2.0;
                                    uint32_t volume = tick_data.bid_size + tick_data.ask_size;
                                    if (volume > 0) {
                                        my_result.sum += mid_price * volume;
                                        my_result.count += volume;
                                    }
                                }
                                break;
                            case QueryType::OHLC:
                                {
                                    double mid_price = (tick_data.bid_price + tick_data.ask_price) / 2.0;
                                    if (!my_result.ohlc.is_set) {
                                        my_result.ohlc.open = mid_price;
                                        my_result.ohlc.high = mid_price;
                                        my_result.ohlc.low = mid_price;
                                        my_result.ohlc.close = mid_price;
                                        my_result.ohlc.is_set = true;
                                        my_result.ohlc.open_ts_for_agg = tick_data.timestamp;
                                        my_result.ohlc.close_ts_for_agg = tick_data.timestamp;
                                    } else {
                                        if (tick_data.timestamp < my_result.ohlc.open_ts_for_agg) {
                                            my_result.ohlc.open_ts_for_agg = tick_data.timestamp;
                                            my_result.ohlc.open = mid_price;
                                        }
                                        if (tick_data.timestamp > my_result.ohlc.close_ts_for_agg) {
                                            my_result.ohlc.close_ts_for_agg = tick_data.timestamp;
                                            my_result.ohlc.close = mid_price;
                                        }
                                        my_result.ohlc.high = std::max(my_result.ohlc.high, mid_price);
                                        my_result.ohlc.low = std::min(my_result.ohlc.low, mid_price);
                                    }
                                }
                                break;
                        }
                    }
                }
            }
        }
    } else {
        std::cout << "Worker " << rank << ": No data for symbol " << q.symbol_id << std::endl;
    }
    
    // Phase 4: Send result back
    MPI_Send(&my_result, sizeof(PartialResult), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    std::cout << "Worker " << rank << " sent partial result for query type " << static_cast<int>(my_result.type) << std::endl;
}
}
