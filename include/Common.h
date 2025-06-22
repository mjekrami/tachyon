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
    uint32_t bid_size; // Added
    uint32_t ask_size; // Added
};

// Example Query
enum class QueryType { AVG_SPREAD, VWAP, OHLC }; // Added VWAP, OHLC
struct Query {
    QueryType type;
    uint64_t  start_time;
    uint64_t  end_time;
    uint32_t  symbol_id;
};

// Partial result sent from Worker to Coordinator
struct OHLCResult {
    double open = 0.0;
    double high = 0.0;
    double low = 0.0;
    double close = 0.0;
    bool is_set = false;
    // For coordinator aggregation, to correctly determine overall open/close
    uint64_t open_ts_for_agg = 0;
    uint64_t close_ts_for_agg = 0;
};

struct PartialResult {
    double   sum = 0.0; // Used for AVG_SPREAD, VWAP numerator (sum of price*volume)
    uint64_t count = 0; // Used for AVG_SPREAD (tick count), VWAP denominator (sum of volume)
    OHLCResult ohlc;    // Used for OHLC query
    QueryType type;     // To help coordinator aggregate correctly
};
