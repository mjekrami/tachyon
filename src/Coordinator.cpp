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
#include <limits> // Required for std::numeric_limits

namespace tachyon {

// Helper struct for OHLC aggregation in Coordinator
struct CoordinatorOHLCAggregator {
    double open_price = 0.0;
    double high_price = std::numeric_limits<double>::min();
    double low_price = std::numeric_limits<double>::max();
    double close_price = 0.0;
    uint64_t first_tick_ts = std::numeric_limits<uint64_t>::max();
    uint64_t last_tick_ts = 0;
    bool is_set = false;

    void aggregate(const PartialResult& partial) {
        if (!partial.ohlc.is_set) return;

        is_set = true;
        if (partial.ohlc.open_ts_for_agg < first_tick_ts) {
            first_tick_ts = partial.ohlc.open_ts_for_agg;
            open_price = partial.ohlc.open;
        }
        if (partial.ohlc.close_ts_for_agg > last_tick_ts) {
            last_tick_ts = partial.ohlc.close_ts_for_agg;
            close_price = partial.ohlc.close;
        }
        high_price = std::max(high_price, partial.ohlc.high);
        low_price = std::min(low_price, partial.ohlc.low);
    }
};


void run_coordinator(int world_size) {
    std::cout << "Coordinator (Rank 0) started." << std::endl;
    int num_workers = world_size - 1;

    // Phase 1: Setup
    std::map<std::string, uint32_t> symbol_map = {{"AAPL", 0}, {"GOOG", 1}};

    // Phase 2: Data Ingestion & Distribution
    TickGenerator generator(symbol_map);
    const int BATCHES_TO_SEND = 10;
    const int TICKS_PER_BATCH = 4096; // Make it same as block size potentially
    const int NUM_SYMBOLS = symbol_map.size();

    // Ensure num_workers is appropriate for symbol distribution, e.g. >= NUM_SYMBOLS or handle appropriately
    if (num_workers == 0) {
        std::cerr << "Coordinator Error: No workers available for data distribution." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
        return;
    }

    // Distribute distinct symbols to workers. If more symbols than workers, symbols will be hashed.
    // If fewer symbols than workers, some workers might not get initial direct assignments for all symbols they could handle.
    // The generator currently creates batches for one symbol at a time based on its internal state.
    // We need to make sure the TickGenerator produces data for specific symbols to test this.
    // For now, let's assume TickGenerator produces batches of mixed symbols, or we iterate through symbols.
    // The current TickGenerator generates a batch for a single symbol, then round-robins to the next symbol.

    for (int i = 0; i < BATCHES_TO_SEND; ++i) {
        // The generator will pick a symbol (e.g. round robin or based on its internal logic)
        std::vector<RawTick> batch = generator.generate_batch(TICKS_PER_BATCH);

        if (batch.empty()) {
            std::cout << "Coordinator: Generator produced an empty batch, skipping." << std::endl;
            continue;
        }

        uint32_t symbol_id_for_batch = batch[0].symbol_id;
        // Hash symbol_id to a worker. Add 1 because worker ranks are 1 to N.
        int target_worker = (symbol_id_for_batch % num_workers) + 1;

        // std::cout << "Coordinator: Sending batch for symbol " << symbol_id_for_batch
        //           << " (source batch index " << i << ") to worker " << target_worker << std::endl;

        MPI_Send(batch.data(), batch.size() * sizeof(RawTick), MPI_BYTE,
                 target_worker, 0, MPI_COMM_WORLD);
    }

    // Signal workers that data sending is done
    for (int i = 1; i < world_size; ++i) {
        MPI_Send(nullptr, 0, MPI_BYTE, i, 1, MPI_COMM_WORLD); // Use tag 1 for termination signal
    }
    std::cout << "Coordinator: All data sent. Starting query phase." << std::endl;

    // Phase 3: Querying
    // Query q_avg_spread = {QueryType::AVG_SPREAD, 0, std::numeric_limits<uint64_t>::max(), 0 /*AAPL_ID*/};
    // Query q_vwap = {QueryType::VWAP, 0, std::numeric_limits<uint64_t>::max(), 0 /*AAPL_ID*/};
    Query q_ohlc = {QueryType::OHLC, 0, std::numeric_limits<uint64_t>::max(), 0 /*AAPL_ID*/};

    Query current_query = q_ohlc; // CHOOSE QUERY TO RUN HERE
    MPI_Bcast(&current_query, sizeof(Query), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    // Phase 4: Aggregation
    PartialResult aggregated_result; // Stores sum/count for AVG_SPREAD/VWAP
    aggregated_result.sum = 0.0;
    aggregated_result.count = 0;
    aggregated_result.type = current_query.type;

    CoordinatorOHLCAggregator ohlc_aggregator;


    for (int i = 1; i < world_size; ++i) {
        PartialResult partial;
        MPI_Recv(&partial, sizeof(PartialResult), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Ensure type consistency, mainly for safety, workers should send back the type they processed
        if (i == 1) { // First worker response sets the type for aggregation
            aggregated_result.type = partial.type;
        } else if (aggregated_result.type != partial.type) {
            std::cerr << "Coordinator Error: Mismatch in query types from workers!" << std::endl;
            // Handle error, maybe skip this partial result or abort
            continue;
        }

        switch (partial.type) {
            case QueryType::AVG_SPREAD:
                aggregated_result.sum += partial.sum;
                aggregated_result.count += partial.count;
                break;
            case QueryType::VWAP:
                aggregated_result.sum += partial.sum;     // sum of (price * volume)
                aggregated_result.count += partial.count; // sum of volume
                break;
            case QueryType::OHLC:
                ohlc_aggregator.aggregate(partial);
                break;
            default:
                std::cerr << "Coordinator Warning: Unknown query type in partial result." << std::endl;
        }
    }

    std::cout << "--------------------------------\n";
    std::cout << "Final Query Result:\n";

    switch (aggregated_result.type) {
        case QueryType::AVG_SPREAD:
            if (aggregated_result.count > 0) {
                double avg_spread = aggregated_result.sum / aggregated_result.count;
                std::cout << "Query Type: AVG_SPREAD\n";
                std::cout << "Average Spread: " << avg_spread << "\n";
                std::cout << "Based on " << aggregated_result.count << " ticks.\n";
            } else {
                std::cout << "Query Type: AVG_SPREAD\n";
                std::cout << "No data found for the given query parameters." << std::endl;
            }
            break;
        case QueryType::VWAP:
            if (aggregated_result.count > 0) { // count is total volume here
                double vwap = aggregated_result.sum / aggregated_result.count;
                std::cout << "Query Type: VWAP\n";
                std::cout << "Volume Weighted Average Price: " << vwap << "\n";
                std::cout << "Total Volume: " << aggregated_result.count << "\n";
            } else {
                std::cout << "Query Type: VWAP\n";
                std::cout << "No data or zero volume for the given query parameters." << std::endl;
            }
            break;
        case QueryType::OHLC:
            std::cout << "Query Type: OHLC\n";
            if (ohlc_aggregator.is_set) {
                std::cout << "Open: " << ohlc_aggregator.open_price << "\n";
                std::cout << "High: " << ohlc_aggregator.high_price << "\n";
                std::cout << "Low: " << ohlc_aggregator.low_price << "\n";
                std::cout << "Close: " << ohlc_aggregator.close_price << "\n";
            } else {
                std::cout << "No data found for the given query parameters." << std::endl;
            }
            break;
        default:
            std::cout << "Unknown query type processed." << std::endl;
    }
    std::cout << "--------------------------------\n";
}
}
