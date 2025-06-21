#pragma once

#include "Common.h"
#include <vector>
#include <map>
#include <string>

class TickGenerator {
public:
    TickGenerator(const std::map<std::string, uint32_t>& symbol_map);
    std::vector<RawTick> generate_batch(size_t batch_size);

private:
    std::vector<uint32_t> symbol_ids_;
    uint64_t current_timestamp_;
    double last_aapl_price_;
    double last_goog_price_;
};
