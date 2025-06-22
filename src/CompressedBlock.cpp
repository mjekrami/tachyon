#include "CompressedBlock.h"
#include "BitStream.h" // Added
#include <iostream>
#include <limits>     // Added for numeric_limits

// Helper union for float/double to uint conversion
union FloatUInt64 {
    double d;
    uint64_t u;
};

// --- CompressedBlock ---

void CompressedBlock::compress(const std::vector<RawTick>& ticks) {
    if (ticks.empty()) return;

    num_ticks_ = ticks.size();
    symbol_id_ = ticks.front().symbol_id;
    start_timestamp_ = ticks.front().timestamp;
    end_timestamp_ = ticks.back().timestamp;

    BitStreamWriter writer;

    // 1. Write Header (already stored as member variables, will be used by scanner)
    // For actual compressed_data_, we start with first values, then deltas.

    uint64_t prev_timestamp = 0;
    uint64_t prev_delta_timestamp = 0;

    FloatUInt64 prev_bid_price_u;
    prev_bid_price_u.d = 0.0;
    FloatUInt64 prev_ask_price_u;
    prev_ask_price_u.d = 0.0;

    // Store initial full values for reconstruction
    first_timestamp_ = ticks[0].timestamp;
    first_bid_price_ = ticks[0].bid_price;
    first_ask_price_ = ticks[0].ask_price;
    first_bid_size_ = ticks[0].bid_size;
    first_ask_size_ = ticks[0].ask_size;


    for (size_t i = 0; i < ticks.size(); ++i) {
        const auto& tick = ticks[i];

        // Timestamps: Delta-of-Delta
        if (i == 0) {
            // Store the first timestamp (already done via first_timestamp_)
            // No bits written for delta-of-delta for the very first timestamp
            prev_timestamp = tick.timestamp;
        } else if (i == 1) {
            uint64_t delta = tick.timestamp - prev_timestamp;
            // Write delta (variable bits, e.g., using Elias gamma or just fixed reasonable size for simplicity here)
            // For simplicity, let's assume deltas fit in a certain range or use a simple VLI-like scheme.
            // A full VLI is more complex, so we'll use fixed-size for now or a marker.
            // Let's try a simple scheme: if delta fits in 7 bits, write 0 + 7 bits. If 14, 10 + 14 bits etc.
            // This is a placeholder for a more robust VLI.
            // For now, let's assume deltas are not massive and write them directly
            // This part NEEDS a proper VLI or adaptive bit sizing.
            // For this example, let's assume positive deltas and write them if they are small.
            if (delta < (1ULL << 7)) { // Fits in 7 bits
                writer.write_bits(0, 1); // Control bit
                writer.write_bits(delta, 7);
            } else if (delta < (1ULL << 14)) { // Fits in 14 bits
                writer.write_bits(1, 2); // Control bits "10"
                writer.write_bits(0,1);
                writer.write_bits(delta, 14);
            } else { // Larger delta (up to 32 bits for this example)
                writer.write_bits(1,2); // Control bits "11"
                writer.write_bits(1,1);
                writer.write_bits(delta, 32); // Max 32 bits for this example
            }
            prev_delta_timestamp = delta;
            prev_timestamp = tick.timestamp;
        } else {
            uint64_t current_delta = tick.timestamp - prev_timestamp;
            int64_t delta_of_delta = static_cast<int64_t>(current_delta) - static_cast<int64_t>(prev_delta_timestamp);

            // Encoding delta_of_delta (Gorilla-like)
            // 0: delta_of_delta is 0
            // 10 + N bits: delta_of_delta fits in N bits
            // 110 + M bits: delta_of_delta fits in M bits
            // 1110 + P bits: delta_of_delta fits in P bits
            // 1111 + 64 bits for full value if very large (or store original delta)
            if (delta_of_delta == 0) {
                writer.write_bits(0, 1);
            } else if (delta_of_delta >= -63 && delta_of_delta <= 64) { // Fits in 7 bits (sign + 6 bits magnitude)
                writer.write_bits(0b10, 2);
                writer.write_bits(static_cast<uint64_t>(delta_of_delta), 7); // Store with sign bit
            } else if (delta_of_delta >= -255 && delta_of_delta <= 256) { // Fits in 9 bits
                writer.write_bits(0b110, 3);
                writer.write_bits(static_cast<uint64_t>(delta_of_delta), 9);
            } else if (delta_of_delta >= -2047 && delta_of_delta <= 2048) { // Fits in 12 bits
                writer.write_bits(0b1110, 4);
                writer.write_bits(static_cast<uint64_t>(delta_of_delta), 12);
            }
            else { // Fallback, store full delta (or original timestamp if DoD is too large)
                   // For simplicity, storing current_delta if DoD is too large
                writer.write_bits(0b1111, 4);
                // This should be a rare case. For now, let's assume current_delta fits in 32 bits.
                // A more robust solution would handle larger deltas or store the original timestamp.
                writer.write_bits(current_delta, 32);
            }
            prev_delta_timestamp = current_delta;
            prev_timestamp = tick.timestamp;
        }

        // Prices: RLE/XOR (inspired by Gorilla)
        FloatUInt64 current_bid_price_u, current_ask_price_u;
        current_bid_price_u.d = tick.bid_price;
        current_ask_price_u.d = tick.ask_price;

        // Bid Price
        if (i == 0) {
            // First price stored in header (first_bid_price_)
            prev_bid_price_u = current_bid_price_u;
        } else {
            uint64_t xor_val_bid = prev_bid_price_u.u ^ current_bid_price_u.u;
            if (xor_val_bid == 0) { // RLE: Price unchanged
                writer.write_bits(0, 1); // Control bit '0'
            } else {
                writer.write_bits(1, 1); // Control bit '1'
                // Gorilla style: check leading/trailing zeros for XOR value
                // Simplified: if XOR value is small enough, store it directly with fewer bits.
                // Or, store significant bits. For now, full XOR value if changed.
                // A more advanced version counts leading/trailing zeros.
                // Let's assume a fixed number of bits for the XOR diff for simplicity,
                // or use a scheme similar to timestamps.
                // For now, we write the full 64 bits if changed. This is not optimal.
                // A better way: count leading zeros (clz) and trailing zeros (ctz)
                // uint32_t clz = __builtin_clzll(xor_val_bid); // Needs compiler intrinsics or equivalent
                // uint32_t ctz = __builtin_ctzll(xor_val_bid);
                // if (clz > some_threshold && ctz > some_threshold) { writer.write_bits(xor_val_bid >> ctz, 64 - clz - ctz); }
                // else { writer.write_bits(xor_val_bid, 64); }
                // Simplified: write full XOR for now.
                writer.write_bits(xor_val_bid, 64);
                prev_bid_price_u = current_bid_price_u;
            }
        }
        // Ask Price (similar logic to Bid Price)
        if (i == 0) {
            prev_ask_price_u = current_ask_price_u;
        } else {
            uint64_t xor_val_ask = prev_ask_price_u.u ^ current_ask_price_u.u;
            if (xor_val_ask == 0) {
                writer.write_bits(0, 1);
            } else {
                writer.write_bits(1, 1);
                writer.write_bits(xor_val_ask, 64); // Simplified: full XOR
                prev_ask_price_u = current_ask_price_u;
            }
        }

        // Sizes: VarInt encoding (simple version)
        // Bid Size
        uint32_t bid_size = tick.bid_size;
        if (i == 0) { /* first_bid_size_ stored */ }
        // For subsequent, store delta or full if significantly different
        // Simple VarInt for now:
        // If fits in 7 bits: 0xxxxxxx
        // If fits in 14 bits: 10xxxxxx xxxxxxxx
        // etc.
        // This is a common VarInt scheme.
        auto write_varint = [&](uint32_t val) {
            while (val >= 0x80) { // More bytes to come
                writer.write_bits((val & 0x7F) | 0x80, 8);
                val >>= 7;
            }
            writer.write_bits(val, 8); // Last byte
        };

        if (i > 0) { // Don't write first size, it's stored in header.
             write_varint(bid_size);
        }


        // Ask Size (similar to Bid Size)
        uint32_t ask_size = tick.ask_size;
         if (i > 0) {
            write_varint(ask_size);
        }
    }
    compressed_data_ = writer.get_buffer();
}

bool CompressedBlock::overlaps_with(uint64_t query_start, uint64_t query_end) const {
    return (start_timestamp_ <= query_end && end_timestamp_ >= query_start);
}

uint32_t CompressedBlock::get_num_ticks() const {
    return num_ticks_;
}


// --- CompressedBlockScanner ---

CompressedBlockScanner::CompressedBlockScanner(const CompressedBlock& block)
    : block_ref_(block),
      reader_(block.compressed_data_) { // Initialize reader_
    // Initialize current values with the first tick's data stored in the block header
    if (block_ref_.get_num_ticks() > 0) {
        current_timestamp_ = block_ref_.first_timestamp_;

        FloatUInt64 bid_u, ask_u;
        bid_u.d = block_ref_.first_bid_price_;
        ask_u.d = block_ref_.first_ask_price_;
        current_bid_price_u_ = bid_u.u;
        current_ask_price_u_ = ask_u.u;

        current_bid_size_ = block_ref_.first_bid_size_;
        current_ask_size_ = block_ref_.first_ask_size_;
    }
    // prev_delta_timestamp_ is implicitly 0 before the first delta is read
}

bool CompressedBlockScanner::has_next() const {
    return current_tick_index_ < block_ref_.get_num_ticks();
}

ScannedTickData CompressedBlockScanner::next() {
    if (!has_next()) {
        throw std::out_of_range("No more ticks to scan in CompressedBlockScanner");
    }

    ScannedTickData tick_data;

    if (current_tick_index_ == 0) {
        // First tick's data is taken directly from the stored first values
        tick_data.timestamp = block_ref_.first_timestamp_;
        tick_data.bid_price = block_ref_.first_bid_price_;
        tick_data.ask_price = block_ref_.first_ask_price_;
        tick_data.bid_size = block_ref_.first_bid_size_; // Populated
        tick_data.ask_size = block_ref_.first_ask_size_; // Populated

        // Update internal state for next delta calculations
        prev_timestamp_ = current_timestamp_; // current_timestamp_ was initialized to first_timestamp_
                                            // prev_delta_timestamp_ remains 0 for the first delta calculation
    } else {
        // Decompress Timestamp (Delta-of-Delta)
        if (current_tick_index_ == 1) {
            // Read the first delta
            uint64_t control_bit_val = reader_.read_bits(1);
            uint64_t delta;
            if (control_bit_val == 0) { // 7 bits
                delta = reader_.read_bits(7);
            } else {
                uint64_t next_control_bit = reader_.read_bits(1);
                if (next_control_bit == 0) { // "10" -> 14 bits
                     delta = reader_.read_bits(14);
                } else { // "11" -> 32 bits
                     delta = reader_.read_bits(32);
                }
            }
            current_timestamp_ = prev_timestamp_ + delta;
            prev_delta_timestamp_ = delta;
        } else {
            // Read delta-of-delta
            uint64_t control_bit = reader_.read_bits(1);
            int64_t delta_of_delta;
            if (control_bit == 0) { // DoD is 0
                delta_of_delta = 0;
            } else {
                uint64_t control_bits_dod = reader_.read_bits(1); // Read next bit for 2-bit cases
                if (control_bits_dod == 0) { // "10" + 7 bits
                    delta_of_delta = static_cast<int64_t>(reader_.read_bits(7));
                    if (delta_of_delta & (1ULL << 6)) { // Sign extend if negative
                        delta_of_delta |= ~((1ULL << 7) -1);
                    }
                } else {
                    control_bits_dod = reader_.read_bits(1); // Read next bit for 3-bit cases
                    if (control_bits_dod == 0) { // "110" + 9 bits
                        delta_of_delta = static_cast<int64_t>(reader_.read_bits(9));
                         if (delta_of_delta & (1ULL << 8)) { delta_of_delta |= ~((1ULL << 9) -1); }

                    } else {
                        control_bits_dod = reader_.read_bits(1); // Read next bit for 4-bit cases
                        if (control_bits_dod == 0) { // "1110" + 12 bits
                            delta_of_delta = static_cast<int64_t>(reader_.read_bits(12));
                            if (delta_of_delta & (1ULL << 11)) { delta_of_delta |= ~((1ULL << 12) -1); }
                        } else { // "1111" + 32 bits (full delta)
                            // This was storing current_delta in compression, not delta_of_delta
                            uint64_t current_delta_fallback = reader_.read_bits(32);
                            current_timestamp_ = prev_timestamp_ + current_delta_fallback;
                            prev_delta_timestamp_ = current_delta_fallback; // Update prev_delta_timestamp
                            delta_of_delta = -1; // Special marker, timestamp already updated
                        }
                    }
                }
            }
            if (delta_of_delta != -1) { // If not fallback case
                uint64_t current_delta = prev_delta_timestamp_ + delta_of_delta;
                current_timestamp_ = prev_timestamp_ + current_delta;
                prev_delta_timestamp_ = current_delta;
            }
        }
        tick_data.timestamp = current_timestamp_;
        prev_timestamp_ = current_timestamp_;

        // Decompress Prices (RLE/XOR)
        FloatUInt64 temp_price_u;
        // Bid Price
        uint64_t rle_control_bid = reader_.read_bits(1);
        if (rle_control_bid == 0) { // Unchanged
            temp_price_u.u = current_bid_price_u_;
        } else { // Changed
            uint64_t xor_val_bid = reader_.read_bits(64); // Simplified: full XOR read
            temp_price_u.u = current_bid_price_u_ ^ xor_val_bid;
            current_bid_price_u_ = temp_price_u.u;
        }
        tick_data.bid_price = temp_price_u.d;

        // Ask Price
        uint64_t rle_control_ask = reader_.read_bits(1);
        if (rle_control_ask == 0) { // Unchanged
            temp_price_u.u = current_ask_price_u_;
        } else { // Changed
            uint64_t xor_val_ask = reader_.read_bits(64); // Simplified: full XOR read
            temp_price_u.u = current_ask_price_u_ ^ xor_val_ask;
            current_ask_price_u_ = temp_price_u.u;
        }
        tick_data.ask_price = temp_price_u.d;

        // Decompress Sizes (VarInt)
        auto read_varint = [&]() -> uint32_t {
            uint32_t result = 0;
            uint32_t shift = 0;
            uint64_t byte_val;
            do {
                byte_val = reader_.read_bits(8);
                result |= (byte_val & 0x7F) << shift;
                shift += 7;
            } while (byte_val & 0x80);
            return result;
        };

        current_bid_size_ = read_varint();
        current_ask_size_ = read_varint();
        tick_data.bid_size = current_bid_size_; // Populated
        tick_data.ask_size = current_ask_size_; // Populated
    }

    current_tick_index_++;
    return tick_data;
}
