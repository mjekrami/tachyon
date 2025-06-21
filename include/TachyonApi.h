#pragma once

#include <cstdint>

#ifdef _WIN32
  #define TACHYON_API __declspec(dllexport)
#else
  #define TACHYON_API
#endif

struct C_TickBatch {
  uint64_t* timestamps;
  double* bid_prices;
  double* ask_prices;
  uint32_t num_ticks;
};

#ifdef __cplusplus
extern "C" {
#endif
TACHYON_API void* tachyon_open_scanner(const char* symbol);
TACHYON_API int tachyon_scanner_next_batch(void* scanner_handle, C_TickBatch* batch_buffer, uint32_t max_ticks);
TACHYON_API void tachyon_free_scanner(void* scanner_handle);
}
#ifdef __cplusplus
}
#endif
