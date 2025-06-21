#include "TickGenerator.h"
#include <random>

TickGenerator::TickGenerator(const std::map<std::string, uint32_t>& symbol_map) 
    : current_timestamp_(1000000000ULL), // Start at 1s past epoch
      last_aapl_price_(150.0),
      last_goog_price_(2800.0)
{
    for(const auto& pair : symbol_map) {
        symbol_ids_.push_back(pair.second);
    }
}

std::vector<RawTick> TickGenerator::generate_batch(size_t batch_size) {
    std::vector<RawTick> batch;
    batch.reserve(batch_size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> symbol_dist(0, symbol_ids_.size() - 1);
    std::normal_distribution<> price_change_dist(0.0, 0.01); // Small changes
    std::uniform_int_distribution<> time_delta_dist(1000, 50000); // 1-50 microseconds

    for (size_t i = 0; i < batch_size; ++i) {
        RawTick tick;
        tick.symbol_id = symbol_ids_[symbol_dist(gen)];
        tick.timestamp = current_timestamp_;

        if (tick.symbol_id == 0) { // AAPL
            last_aapl_price_ += price_change_dist(gen);
            tick.bid_price = last_aapl_price_;
            tick.ask_price = last_aapl_price_ + 0.02;
        } else { // GOOG
            last_goog_price_ += price_change_dist(gen);
            tick.bid_price = last_goog_price_;
            tick.ask_price = last_goog_price_ + 0.15;
        }
        
        tick.bid_size = 100;
        tick.ask_size = 100;

        batch.push_back(tick);
        current_timestamp_ += time_delta_dist(gen);
    }

    return batch;
}
