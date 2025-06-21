#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Raw, uncompressed data structure
struct RawTick {
    uint64_t timestamp;
    uint32_t symbol_id;
    double   bid_price;
    double   ask_price;
    uint32_t bid_size;
    uint32_t ask_size;
};

// Lightweight struct for query results from the scanner
struct ScannedTickData {
    uint64_t timestamp;
    double ask_price;
    double bid_price;
};

// Example Query
enum class QueryType { AVG_SPREAD };
struct Query {
    QueryType type;
    uint64_t  start_time;
    uint64_t  end_time;
    uint32_t  symbol_id;
};

// Partial result sent from Worker to Coordinator
struct PartialResult {
    double   sum = 0.0;
    uint64_t count = 0;
};
