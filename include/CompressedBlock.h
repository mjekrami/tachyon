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
    // This will eventually hold the state for iterating over compressed data
    const CompressedBlock& block_ref_;
    uint32_t current_tick_index_ = 0;
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
    uint64_t start_timestamp_ = 0;
    uint64_t end_timestamp_ = 0;
    uint32_t symbol_id_ = 0;
    uint32_t num_ticks_ = 0;

    // --- Data Payload ---
    std::vector<uint8_t> compressed_data_;
};
