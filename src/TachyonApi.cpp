#include "TachyonApi.h"
#include "CompressedBlock.h" // Your existing header
#include <vector>
#include <string>
#include <iostream>

// For simplicity, we'll assume blocks are just stored in memory.
// A real implementation would load these from files.
struct ScannerHandle {
    std::string symbol;
    std::vector<CompressedBlock> blocks; // All blocks for this symbol
    int current_block_idx = 0;
    std::unique_ptr<CompressedBlockScanner> block_scanner;

    ScannerHandle(const std::string& sym) : symbol(sym) {
        // --- MOCK LOADING ---
        // In a real system, you would find and load files like
        // "data/AAPL_0.tachyon", "data/AAPL_1.tachyon", etc.
        // For this example, let's just create some dummy blocks.
        if (sym == "AAPL") {
            // Create and add a few mock blocks... this is where your MPI
            // app's output would be read.
            std::cout << "[C++] Mock-loading data for AAPL" << std::endl;
        }
    }
};

TACHYON_API void* tachyon_open_scanner(const char* symbol) {
    try {
        // This would find and open all files for the given symbol
        return new ScannerHandle(symbol);
    } catch (...) {
        return nullptr;
    }
}

TACHYON_API int tachyon_scanner_next_batch(void* scanner_handle, C_TickBatch* batch_buffer, uint32_t max_ticks) {
    auto* handle = static_cast<ScannerHandle*>(scanner_handle);
    if (!handle) return 0;

    uint32_t ticks_read = 0;
    while (ticks_read < max_ticks) {
        // Do we have an active block scanner? If not, try to get the next block.
        if (!handle->block_scanner || !handle->block_scanner->has_next()) {
            if (handle->current_block_idx >= handle->blocks.size()) {
                break; // No more blocks to read
            }
            handle->block_scanner = std::make_unique<CompressedBlockScanner>(handle->blocks[handle->current_block_idx]);
            handle->current_block_idx++;
        }

        // Decompress one tick
        ScannedTickData tick = handle->block_scanner->next();
        
        // Fill the C struct passed from Rust
        batch_buffer->timestamps[ticks_read] = tick.timestamp;
        batch_buffer->bid_prices[ticks_read] = tick.bid_price;
        batch_buffer->ask_prices[ticks_read] = tick.ask_price;
        ticks_read++;
    }

    batch_buffer->num_ticks = ticks_read;
    return ticks_read > 0; // Return 1 if data was read, 0 if stream ended
}

TACHYON_API void tachyon_free_scanner(void* scanner_handle) {
    delete static_cast<ScannerHandle*>(scanner_handle);
}
