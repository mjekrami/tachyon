#pragma once
#include "Common.h"
#include <vector>

class CompressedBlock; // Forward declaration

class CompressedBlockScanner {
public:
    CompressedBlockScanner(const CompressedBlock& block);
    bool has_next() const;
    ScannedTickData next();

private:
    const CompressedBlock& block_ref_;
    BitStreamReader reader_; // Added
    uint32_t current_tick_index_ = 0;

    // State for decompression
    uint64_t prev_timestamp_ = 0;
    uint64_t current_timestamp_ = 0;
    uint64_t prev_delta_timestamp_ = 0; // For Delta-of-Delta

    uint64_t current_bid_price_u_ = 0; // Store as uint64_t for XOR
    uint64_t current_ask_price_u_ = 0; // Store as uint64_t for XOR

    uint32_t current_bid_size_ = 0;
    uint32_t current_ask_size_ = 0;
};


class CompressedBlock {
    friend class CompressedBlockScanner; // Allow scanner to access private members
public:
    // --- Methods ---
    void compress(const std::vector<RawTick>& ticks);
    bool overlaps_with(uint64_t query_start, uint64_t query_end) const;
    uint32_t get_num_ticks() const;


private:
    // --- Header Fields ---
    uint64_t start_timestamp_ = 0; // Min timestamp in block
    uint64_t end_timestamp_ = 0;   // Max timestamp in block
    uint32_t symbol_id_ = 0;
    uint32_t num_ticks_ = 0;

    // Store first full values for delta calculations during decompression
    uint64_t first_timestamp_ = 0;
    double first_bid_price_ = 0.0;
    double first_ask_price_ = 0.0;
    uint32_t first_bid_size_ = 0;
    uint32_t first_ask_size_ = 0;

    // --- Data Payload ---
    std::vector<uint8_t> compressed_data_;
};
