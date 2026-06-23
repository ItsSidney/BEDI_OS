#include "libs/bmp.h"
#include "kernel/mem/kheap.h"
#include <stdint.h>

static uint32_t r32(const uint8_t* d, int o) {
    return (uint32_t)d[o] | ((uint32_t)d[o+1] << 8) | ((uint32_t)d[o+2] << 16) | ((uint32_t)d[o+3] << 24);
}
static uint16_t r16(const uint8_t* d, int o) {
    return (uint16_t)d[o] | ((uint16_t)d[o+1] << 8);
}
static int32_t r32s(const uint8_t* d, int o) {
    return (int32_t)r32(d, o);
}

int bmp_decode(const uint8_t* data, int len, bmp_image_t* img) {
    if (!data || !img || len < 26) return 0;
    if (data[0] != 'B' || data[1] != 'M') return 0;

    uint32_t pix_off = r32(data, 10);
    uint32_t hdr_sz  = r32(data, 14);
    int w = 0, h = 0;
    uint16_t bpp = 0;
    uint32_t compression = 0;
    int palette_colors = 0;
    int top_down = 0;

    if (hdr_sz == 12) {
        w = r16(data, 18);
        h = r16(data, 20);
        bpp = r16(data, 24);
    } else if (hdr_sz >= 40) {
        w = r32s(data, 18);
        h = r32s(data, 22);
        bpp = r16(data, 28);
        compression = r32(data, 30);
        palette_colors = (int)r32(data, 46);
    } else {
        return 0;
    }

    if (w <= 0 || w > 4096) return 0;
    if (h == 0 || h > 4096 || h < -4096) return 0;
    if (h < 0) { top_down = 1; h = -h; }
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) return 0;
    if (compression > 3) return 0;
    if (bpp >= 16 && compression == 1) return 0;
    if (bpp == 16 && compression > 3) return 0;
    if (bpp == 1 && compression > 0) return 0;

    if (pix_off < 14 + hdr_sz || pix_off >= (uint32_t)len) return 0;

    int pal_size = 0;
    if (bpp <= 8) {
        int max_colors = (bpp == 1) ? 2 : (bpp == 4) ? 16 : 256;
        int pal_entries = (palette_colors > 0 && palette_colors <= max_colors) ? palette_colors : max_colors;
        int entry_size = (hdr_sz == 12) ? 3 : 4;
        pal_size = pal_entries * entry_size;
        if (pix_off < 14 + hdr_sz + pal_size) return 0;
    }

    // Read bitfield masks for 16-bit
    uint32_t rm = 0, gm = 0, bm = 0;
    if (bpp == 16 && compression == 3 && hdr_sz >= 56) {
        rm = r32(data, 54);
        gm = r32(data, 58);
        bm = r32(data, 62);
    } else if (bpp == 16 && compression == 0) {
        rm = 0x7C00; gm = 0x03E0; bm = 0x001F;
    }

    // Read palette
    uint32_t pal[256];
    for (int i = 0; i < 256; i++) pal[i] = 0;
    if (bpp <= 8) {
        int max_colors = (bpp == 1) ? 2 : (bpp == 4) ? 16 : 256;
        int pal_entries = (palette_colors > 0 && palette_colors <= max_colors) ? palette_colors : max_colors;
        int entry_size = (hdr_sz == 12) ? 3 : 4;
        for (int i = 0; i < pal_entries; i++) {
            int off = 14 + hdr_sz + i * entry_size;
            if (off + 2 >= len) break;
            uint32_t b = data[off];
            uint32_t g = data[off + 1];
            uint32_t r = data[off + 2];
            pal[i] = (r << 16) | (g << 8) | b;
        }
    }

    int row_size = ((w * (int)bpp + 31) / 32) * 4;
    uint8_t* out = (uint8_t*)kmalloc((size_t)(w * h * 3));
    if (!out) return 0;

    // RLE8 decompression buffer
    uint8_t* rle_buf = 0;
    int rle_row_size = w;
    if ((compression == 1 && bpp == 8) || (compression == 2 && bpp == 4)) {
        rle_buf = (uint8_t*)kmalloc((size_t)(w * h));
        if (!rle_buf) { kfree(out); return 0; }
        for (int i = 0; i < w * h; i++) rle_buf[i] = 0;
        int src = (int)pix_off;
        int rx = 0, ry = 0;
        while (ry < h && src + 1 < len) {
            uint8_t b1 = data[src++];
            uint8_t b2 = data[src++];
            if (b1 == 0) {
                if (b2 == 0) { ry++; rx = 0; }
                else if (b2 == 1) break;
                else if (b2 == 2) {
                    if (src + 1 >= len) break;
                    rx += data[src++];
                    ry += data[src++];
                } else {
                    int abs_len = b2;
                    for (int k = 0; k < abs_len; k++) {
                        if (rx < w && ry < h) {
                            if (compression == 1) rle_buf[ry * w + rx] = data[src];
                            else { if (k & 1) rle_buf[ry * w + rx] = data[src] & 0x0F; else rle_buf[ry * w + rx] = (data[src] >> 4) & 0x0F; }
                        }
                        if (compression == 1) { src++; if (k & 1 && k == abs_len - 1) src++; }
                        else { if (k & 1) src++; }
                        rx++;
                    }
                    if (compression == 1 && (abs_len & 1)) src++;
                    if (compression == 2 && ((abs_len + 1) / 2) & 1) src++;
                }
            } else {
                for (int k = 0; k < b1; k++) {
                    if (rx < w && ry < h) {
                        if (compression == 1) rle_buf[ry * w + rx] = b2;
                        else { if (k & 1) rle_buf[ry * w + rx] = b2 & 0x0F; else rle_buf[ry * w + rx] = (b2 >> 4) & 0x0F; }
                    }
                    rx++;
                }
            }
        }
    }

    for (int y = 0; y < h; y++) {
        int src_y = top_down ? y : (h - 1 - y);
        int src_off = (int)pix_off + src_y * row_size;
        uint8_t* dst = out + y * w * 3;

        if (compression == 1 && bpp == 8 && rle_buf) {
            for (int x = 0; x < w; x++) {
                uint32_t c = pal[rle_buf[y * w + x]];
                dst[x * 3 + 0] = (c >> 16) & 0xFF;
                dst[x * 3 + 1] = (c >> 8) & 0xFF;
                dst[x * 3 + 2] = c & 0xFF;
            }
            continue;
        }
        if (compression == 2 && bpp == 4 && rle_buf) {
            for (int x = 0; x < w; x++) {
                uint32_t c = pal[rle_buf[y * w + x]];
                dst[x * 3 + 0] = (c >> 16) & 0xFF;
                dst[x * 3 + 1] = (c >> 8) & 0xFF;
                dst[x * 3 + 2] = c & 0xFF;
            }
            continue;
        }

        for (int x = 0; x < w; x++) {
            uint8_t r = 0, g = 0, b = 0;
            if (bpp == 24) {
                int p = src_off + x * 3;
                if (p + 2 < len) { b = data[p]; g = data[p+1]; r = data[p+2]; }
            } else if (bpp == 32) {
                int p = src_off + x * 4;
                if (p + 2 < len) { b = data[p]; g = data[p+1]; r = data[p+2]; }
            } else if (bpp == 16) {
                int p = src_off + x * 2;
                if (p + 1 < len) {
                    uint16_t v = r16(data, p);
                    uint32_t rv = 0, gv = 0, bv = 0;
                    if (rm) { rv = (v & rm); rv = (rv * 255) / (rm >> __builtin_ctz(rm)); }
                    else rv = (v >> 10) & 0x1F; rv = (rv * 255) / 31;
                    if (gm) { gv = (v & gm); gv = (gv * 255) / (gm >> __builtin_ctz(gm)); }
                    else gv = (v >> 5) & 0x1F; gv = (gv * 255) / 31;
                    if (bm) { bv = (v & bm); bv = (bv * 255) / (bm >> __builtin_ctz(bm)); }
                    else bv = v & 0x1F; bv = (bv * 255) / 31;
                    r = (uint8_t)rv; g = (uint8_t)gv; b = (uint8_t)bv;
                }
            } else if (bpp <= 8) {
                int byte_off = src_off + (x * (int)bpp) / 8;
                int bit_off = (8 - (int)bpp) - (x * (int)bpp) % 8;
                if (byte_off < len) {
                    int idx = 0;
                    if (bpp == 8) idx = data[byte_off];
                    else if (bpp == 4) idx = (data[byte_off] >> bit_off) & 0x0F;
                    else if (bpp == 1) idx = (data[byte_off] >> bit_off) & 0x01;
                    uint32_t c = pal[idx];
                    r = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; b = c & 0xFF;
                }
            }
            dst[x * 3] = r; dst[x * 3 + 1] = g; dst[x * 3 + 2] = b;
        }
    }

    if (rle_buf) kfree(rle_buf);

    img->width = w;
    img->height = h;
    img->bpp = (int)bpp;
    img->pixels = out;
    return 1;
}

void bmp_free(bmp_image_t* img) {
    if (img && img->pixels) { kfree(img->pixels); img->pixels = 0; }
}
