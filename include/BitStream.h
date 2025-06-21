#pragma once
#include <vector>
#include <cstdint>

class BitStreamWriter {
public:
    void write_bits(uint64_t data, int num_bits);
    const std::vector<uint8_t>& get_buffer() const;

private:
    std::vector<uint8_t> buffer_;
    int bit_position_ = 0; // 0-7
};

class BitStreamReader {
public:
    BitStreamReader(const std::vector<uint8_t>& buffer);
    uint64_t read_bits(int num_bits);

private:
    const std::vector<uint8_t>& buffer_;
    size_t byte_index_ = 0;
    int bit_position_ = 0; // 0-7
};
