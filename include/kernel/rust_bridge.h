#ifndef RUST_BRIDGE_H
#define RUST_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

// ── Rust Kernel Module Init ──────────────────────────────────
void rust_kernel_init(void);

// ── Rust Memory Allocator (wraps C kmalloc/kfree) ────────────
void* rust_alloc(size_t size);
void  rust_free(void* ptr);

// ── Rust Process Registry ────────────────────────────────────
void rust_process_register(int pid, const char* name, int state);
int  rust_process_get_count(void);

// ── Rust Utility ─────────────────────────────────────────────
uint64_t rust_get_tick_count(void);
void    rust_collect_garbage(void);

// ── Rust Tetris (backward compat) ────────────────────────────
void rust_tetris_init(int win_id);
void rust_tetris_update(void);
void rust_tetris_render(int x, int y, int w, int h);
void rust_tetris_handle_key(char key);

// ── C functions callable from Rust ───────────────────────────
void print_string(const char* s);
void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_string_transparent(int x, int y, const char* s, uint32_t color);
void klog(const char* msg);
int  get_current_task_id(void);
int  get_task_count(void);

#endif
