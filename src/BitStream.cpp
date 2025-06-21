#include "BitStream.h"
#include <stdexcept>

// --- Writer ---
void BitStreamWriter::write_bits(uint64_t data, int num_bits) {
    if (num_bits > 64) throw std::invalid_argument("Cannot write more than 64 bits at once");

    for (int i = num_bits - 1; i >= 0; --i) {
        if (buffer_.empty() || bit_position_ == 8) {
            buffer_.push_back(0);
            bit_position_ = 0;
        }

        uint8_t bit = (data >> i) & 1;
        if (bit) {
            buffer_.back() |= (1 << (7 - bit_position_));
        }
        bit_position_++;
    }
}

const std::vector<uint8_t>& BitStreamWriter::get_buffer() const {
    return buffer_;
}


// --- Reader ---
BitStreamReader::BitStreamReader(const std::vector<uint8_t>& buffer) : buffer_(buffer) {}

uint64_t BitStreamReader::read_bits(int num_bits) {
    if (num_bits > 64) throw std::invalid_argument("Cannot read more than 64 bits at once");

    uint64_t result = 0;
    for (int i = 0; i < num_bits; ++i) {
        if (byte_index_ >= buffer_.size()) {
            throw std::out_of_range("Reading past the end of the buffer");
        }
        
        result <<= 1;
        uint8_t bit = (buffer_[byte_index_] >> (7 - bit_position_)) & 1;
        if (bit) {
            result |= 1;
        }
        
        bit_position_++;
        if (bit_position_ == 8) {
            bit_position_ = 0;
            byte_index_++;
        }
    }
    return result;
}
