#include "CompressedBlock.h"
#include <iostream>

// --- CompressedBlock ---

void CompressedBlock::compress(const std::vector<RawTick>& ticks) {
    if (ticks.empty()) return;

    // --- REAL IMPLEMENTATION NEEDED HERE ---
    // This is the core of your project. For now, we'll just store the header.
    // Your goal is to use BitStreamWriter to compress timestamps, prices, etc.
    // into the 'compressed_data_' vector.
    
    num_ticks_ = ticks.size();
    symbol_id_ = ticks.front().symbol_id;
    start_timestamp_ = ticks.front().timestamp;
    end_timestamp_ = ticks.back().timestamp;

    // Placeholder: A real implementation would serialize to binary format
}

bool CompressedBlock::overlaps_with(uint64_t query_start, uint64_t query_end) const {
    return (start_timestamp_ <= query_end && end_timestamp_ >= query_start);
}

uint32_t CompressedBlock::get_num_ticks() const {
    return num_ticks_;
}


// --- CompressedBlockScanner ---

CompressedBlockScanner::CompressedBlockScanner(const CompressedBlock& block) : block_ref_(block) {}

bool CompressedBlockScanner::has_next() const {
    // We haven't implemented decompression, so we simulate having data.
    return current_tick_index_ < block_ref_.get_num_ticks();
}

ScannedTickData CompressedBlockScanner::next() {
    // --- REAL IMPLEMENTATION NEEDED HERE ---
    // This is the other half of the core project. You need to use BitStreamReader
    // to decompress one tick at a time from the 'compressed_data_' buffer.

    // For now, return dummy data to make the query logic work.
    ScannedTickData dummy_data;
    dummy_data.timestamp = block_ref_.start_timestamp_ + current_tick_index_ * 10000; // Fake timestamp
    dummy_data.ask_price = 150.0;
    dummy_data.bid_price = 149.98;

    current_tick_index_++;
    return dummy_data;
}
