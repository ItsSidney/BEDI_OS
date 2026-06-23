#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::slice;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

// ── C FFI ──────────────────────────────────────────────────────
mod ffi {
    extern "C" {
        pub fn klog(msg: *const u8);
        pub fn kmalloc(size: usize) -> *mut u8;
        pub fn kfree(ptr: *mut u8);
        pub fn print_string(s: *const u8);
        pub fn gfx_fill_rect(x: i32, y: i32, w: i32, h: i32, color: u32);
        pub fn gfx_draw_string_transparent(x: i32, y: i32, s: *const u8, color: u32);
        pub fn get_current_task_id() -> i32;
        pub fn get_task_count() -> i32;
        pub fn timer_get_ms() -> u64;
    }
}

// ── Process Info Structure ─────────────────────────────────────
#[repr(C)]
#[derive(Copy, Clone)]
pub struct ProcessInfo {
    pub pid: i32,
    pub state: i32,
    pub name: [u8; 32],
    pub stack_usage: u64,
}

// ── Process Registry (flat arrays, O(N) linear scan) ─────────
const MAX_PROCS: usize = 64;

// Use separate arrays for keys, values, and taken flags
// All initialized to zero/false
static mut PROC_KEYS: [i32; MAX_PROCS] = [0; MAX_PROCS];
static mut PROC_TAKEN: [u8; MAX_PROCS] = [0; MAX_PROCS];
static mut PROC_PIDS: [i32; MAX_PROCS] = [0; MAX_PROCS];
static mut PROC_STATES: [i32; MAX_PROCS] = [0; MAX_PROCS];
static mut PROC_NAMES: [[u8; 32]; MAX_PROCS] = [[0; 32]; MAX_PROCS];
static mut PROC_STACK: [u64; MAX_PROCS] = [0; MAX_PROCS];
static mut PROC_COUNT: usize = 0;

#[no_mangle]
pub extern "C" fn rust_process_register(pid: i32, name: *const u8, state: i32) {
    unsafe {
        for i in 0..MAX_PROCS {
            if PROC_TAKEN[i] != 0 && PROC_KEYS[i] == pid {
                PROC_STATES[i] = state;
                return;
            }
        }
        for i in 0..MAX_PROCS {
            if PROC_TAKEN[i] == 0 {
                let cstr = slice::from_raw_parts(name, 31);
                let len = cstr.iter().position(|&c| c == 0).unwrap_or(31);
                PROC_KEYS[i] = pid;
                PROC_PIDS[i] = pid;
                PROC_STATES[i] = state;
                PROC_STACK[i] = 0;
                for j in 0..len {
                    PROC_NAMES[i][j] = cstr[j];
                }
                if len < 32 { PROC_NAMES[i][len] = 0; }
                PROC_TAKEN[i] = 1;
                PROC_COUNT += 1;
                return;
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn rust_process_get_count() -> i32 {
    unsafe { PROC_COUNT as i32 }
}

// ── Kernel Service in Rust ────────────────────────────────────
#[no_mangle]
pub extern "C" fn rust_kernel_init() {
    unsafe {
        ffi::klog("Rust kernel module initialized\0".as_ptr());
    }
}

#[no_mangle]
pub extern "C" fn rust_alloc(size: usize) -> *mut u8 {
    unsafe { ffi::kmalloc(size) }
}

#[no_mangle]
pub extern "C" fn rust_free(ptr: *mut u8) {
    unsafe { ffi::kfree(ptr) }
}

#[no_mangle]
pub extern "C" fn rust_get_tick_count() -> u64 {
    unsafe { ffi::timer_get_ms() }
}

#[no_mangle]
pub extern "C" fn rust_collect_garbage() {
    unsafe {
        for i in 0..MAX_PROCS {
            if PROC_TAKEN[i] != 0 && PROC_PIDS[i] < 0 {
                PROC_TAKEN[i] = 0;
                PROC_COUNT -= 1;
            }
        }
    }
}

// ── Rust Tetris (for backward compatibility) ───────────────────
#[no_mangle]
pub extern "C" fn rust_tetris_init(_win_id: i32) {
    unsafe {
        ffi::print_string("Rust Tetris: initialized\0".as_ptr());
    }
}

#[no_mangle]
pub extern "C" fn rust_tetris_render(x: i32, y: i32, w: i32, h: i32) {
    unsafe {
        ffi::gfx_fill_rect(x, y, w, h, 0x1C2128);
        ffi::gfx_draw_string_transparent(x + 16, y + 16,
            "Rust Kernel Module Active\0".as_ptr(), 0x58A6FF);

        let tick = ffi::timer_get_ms();
        let mut buf = [0u8; 32];
        let s = format_buf(tick, &mut buf);
        ffi::gfx_draw_string_transparent(x + 16, y + 40,
            s.as_ptr() as *const u8, 0xF0F6FC);
    }
}

fn format_buf(val: u64, buf: &mut [u8; 32]) -> &[u8] {
    let prefix = b"Uptime: ";
    let plen = prefix.len();
    buf[..plen].copy_from_slice(prefix);
    let mut n = val;
    let mut i = 31;
    loop {
        i -= 1;
        buf[i] = b'0' + (n % 10) as u8;
        n /= 10;
        if n == 0 || i == plen { break; }
    }
    let ms = b"ms";
    let last = plen + (31 - i);
    if last + 2 <= 32 {
        buf[last] = ms[0];
        buf[last + 1] = ms[1];
        &buf[..last + 2]
    } else {
        &buf[..plen]
    }
}

#[no_mangle]
pub extern "C" fn rust_tetris_handle_key(_key: i8) {
    unsafe {
        ffi::print_string("Rust: key event\n\0".as_ptr());
    }
}

#[no_mangle]
pub extern "C" fn rust_tetris_update() {}
