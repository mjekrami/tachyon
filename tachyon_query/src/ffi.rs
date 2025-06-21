use libc::{c_char,c_int,c_void};

#[repr(C)]]
pub struct C_TickBatch {
    pub timestamps: *mut u64,
    pub bid_prices: *mut f64,
    pub ask_prices: *mut f64,
    pub num_ticks: u32,
}

#[link(name = "tachyon_lib")]
extern "C" {
    pub fn tachyon_open_scanner(symbol: *const c_char) -> *mut c_void;
    pub fn tachyon_scanner_next_batch(
        scanner_handle: *mut c_void,
        batch_buffer: *mut C_TickBatch,
        max_ticks: u32,
    ) -> c_int;
    pub fn tachyon_free_scanner(scanner_handle: *mut c_void);
}
