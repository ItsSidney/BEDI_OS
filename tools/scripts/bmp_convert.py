#!/usr/bin/env python3
"""
BMP conversion tool for BEDI OS.
Converts 8-bit / RLE-compressed / indexed BMPs to 24-bit uncompressed,
optionally resizes to fit within MAX_FILE_SIZE (8192 bytes).

Usage:
    python3 bmp_convert.py input.bmp [-o output.bmp] [-s max_size]
    python3 bmp_convert.py input.bmp --c-array            # output as C byte array
"""

import struct
import sys
import os

MAX_FILE_SIZE = 8192

def read_bmp(path):
    with open(path, 'rb') as f:
        return f.read()

def decode_bmp(data):
    if data[0:2] != b'BM':
        raise ValueError('Not a BMP file')

    pix_off = struct.unpack('<I', data[10:14])[0]
    hdr_sz = struct.unpack('<I', data[14:18])[0]
    w = abs(struct.unpack('<i', data[18:22])[0])
    h = abs(struct.unpack('<i', data[22:26])[0])
    bpp = struct.unpack('<H', data[28:30])[0]
    compression = struct.unpack('<I', data[30:34])[0] if hdr_sz >= 40 else 0
    colors_used = struct.unpack('<I', data[46:50])[0] if hdr_sz >= 40 else 0

    # Read palette
    pal = []
    if bpp <= 8:
        max_c = {1: 2, 4: 16, 8: 256}[bpp]
        pal_count = colors_used if colors_used else max_c
        entry_sz = 3 if hdr_sz == 12 else 4
        for i in range(pal_count):
            off = 14 + hdr_sz + i * entry_sz
            b, g, r = data[off], data[off+1], data[off+2]
            pal.append((r, g, b))

    # Decompress
    row_size = ((w * bpp + 31) // 32) * 4
    pixels = [0] * (w * h * 3)

    if compression in (1, 2) and bpp in (4, 8):
        rle_buf = [0] * (w * h)
        src = pix_off
        x = y = 0
        while y < h and src < len(data):
            b1 = data[src]; src += 1
            if src >= len(data): break
            b2 = data[src]; src += 1
            if b1 == 0:
                if b2 == 0:
                    y += 1; x = 0
                elif b2 == 1:
                    break
                elif b2 == 2:
                    x += data[src]; src += 1
                    y += data[src]; src += 1
                else:
                    for k in range(b2):
                        if x < w and y < h:
                            if bpp == 8:
                                rle_buf[y * w + x] = data[src]
                            else:
                                rle_buf[y * w + x] = (data[src] >> ((1 - (k & 1)) * 4)) & 0x0F
                        if bpp == 8:
                            src += 1
                        elif k & 1:
                            src += 1
                        x += 1
                    if bpp == 8 and (b2 & 1):
                        src += 1
                    if bpp == 4 and ((b2 + 1) // 2) & 1:
                        src += 1
            else:
                for k in range(b1):
                    if x < w and y < h:
                        if bpp == 8:
                            rle_buf[y * w + x] = b2
                        else:
                            rle_buf[y * w + x] = (b2 >> ((1 - (k & 1)) * 4)) & 0x0F
                    x += 1

        for y in range(h):
            for x in range(w):
                idx = rle_buf[y * w + x]
                r, g, b = pal[idx] if idx < len(pal) else (0, 0, 0)
                off = (y * w + x) * 3
                pixels[off:off+3] = [r, g, b]
    else:
        for y in range(h):
            src_y = h - 1 - y
            src_off = pix_off + src_y * row_size
            for x in range(w):
                off = (y * w + x) * 3
                if bpp == 24:
                    p = src_off + x * 3
                    b, g, r = data[p:p+3]
                elif bpp == 32:
                    p = src_off + x * 4
                    b, g, r = data[p], data[p+1], data[p+2]
                elif bpp == 8:
                    idx = data[src_off + x]
                    r, g, b = pal[idx] if idx < len(pal) else (0, 0, 0)
                elif bpp == 4:
                    byte = data[src_off + x // 2]
                    idx = (byte >> ((1 - x % 2) * 4)) & 0x0F
                    r, g, b = pal[idx] if idx < len(pal) else (0, 0, 0)
                elif bpp == 1:
                    byte = data[src_off + x // 8]
                    idx = (byte >> (7 - x % 8)) & 1
                    r, g, b = pal[idx] if idx < len(pal) else (0, 0, 0)
                else:
                    r = g = b = 0
                pixels[off:off+3] = [r, g, b]

    return w, h, pixels

def resize_pixels(pixels, w, h, dw, dh):
    out = []
    for dy in range(dh):
        for dx in range(dw):
            sx_start = dx * w // dw
            sx_end = (dx + 1) * w // dw
            sy_start = dy * h // dh
            sy_end = (dy + 1) * h // dh
            if sx_end <= sx_start: sx_end = sx_start + 1
            if sy_end <= sy_start: sy_end = sy_start + 1
            r = g = b = cnt = 0
            for sy in range(sy_start, sy_end):
                for sx in range(sx_start, sx_end):
                    off = (sy * w + sx) * 3
                    r += pixels[off]; g += pixels[off+1]; b += pixels[off+2]; cnt += 1
            if cnt:
                out.extend([r // cnt, g // cnt, b // cnt])
    return out

def encode_24bit_bmp(w, h, pixels):
    row_size = ((w * 24 + 31) // 32) * 4
    data = bytearray()
    total = 14 + 40 + row_size * h
    data += b'BM'
    data += struct.pack('<I', total)
    data += struct.pack('<HH', 0, 0)
    data += struct.pack('<I', 14 + 40)
    data += struct.pack('<I', 40)
    data += struct.pack('<i', w)
    data += struct.pack('<i', h)
    data += struct.pack('<HH', 1, 24)
    data += b'\x00' * 24
    for y in range(h):
        row = pixels[y * w * 3 : (y + 1) * w * 3]
        data.extend(row)
        data.extend(b'\x00' * (row_size - w * 3))
    return bytes(data)

def to_c_array(data, var_name='bmp_data'):
    lines = [f'static const unsigned char {var_name}[] = {{']
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
        lines.append(f'    {hex_str},')
    lines.append('};')
    lines.append(f'#define {var_name.upper()}_SIZE {len(data)}')
    return '\n'.join(lines)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    data = read_bmp(path)
    w, h, pixels = decode_bmp(data)

    out_path = None
    as_c_array = False
    max_size = MAX_FILE_SIZE

    i = 2
    while i < len(sys.argv):
        if sys.argv[i] == '-o' and i + 1 < len(sys.argv):
            out_path = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == '-s' and i + 1 < len(sys.argv):
            max_size = int(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == '--c-array':
            as_c_array = True
            i += 1
        else:
            i += 1

    # Resize if too large
    final_w, final_h = w, h
    final_pixels = pixels
    bmp_bytes = encode_24bit_bmp(w, h, pixels)
    while len(bmp_bytes) > max_size and final_w > 8 and final_h > 8:
        final_w = max(final_w // 2, 8)
        final_h = max(final_h // 2, 8)
        final_pixels = resize_pixels(pixels, w, h, final_w, final_h)
        bmp_bytes = encode_24bit_bmp(final_w, final_h, final_pixels)
        if final_w <= 8 and final_h <= 8:
            break

    if final_w != w or final_h != h:
        print(f'Resized from {w}x{h} to {final_w}x{final_h} ({len(bmp_bytes)} bytes)', file=sys.stderr)

    if as_c_array:
        print(to_c_array(bmp_bytes, 'bmp_data'))
    elif out_path:
        with open(out_path, 'wb') as f:
            f.write(bmp_bytes)
        print(f'Wrote {out_path} ({final_w}x{final_h}, {len(bmp_bytes)} bytes)', file=sys.stderr)
    else:
        sys.stdout.buffer.write(bmp_bytes)

if __name__ == '__main__':
    main()
