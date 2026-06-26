#!/usr/bin/env python3
"""Simple BMP -> raw RGB C array converter."""

import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from bmp_convert import read_bmp, decode_bmp, to_c_array

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 bmp_to_rgb_array.py input.bmp [-o output.h]")
        sys.exit(1)

    path = sys.argv[1]
    out_path = None
    i = 2
    while i < len(sys.argv):
        if sys.argv[i] == '-o' and i + 1 < len(sys.argv):
            out_path = sys.argv[i + 1]
            i += 2
        else:
            i += 1

    data = read_bmp(path)
    w, h, pixels = decode_bmp(data)

    # Output as a C array of raw RGB bytes (no BMP header)
    arr_name = os.path.splitext(os.path.basename(path))[0].replace('-', '_').replace('.', '_')
    lines = [f'static const unsigned char {arr_name}_rgb[] = {{']
    for i in range(0, len(pixels), 12):
        chunk = pixels[i:i+12]
        hex_str = ', '.join(f'0x{b:02X}' for b in chunk)
        lines.append(f'    {hex_str},')
    lines.append('};')
    lines.append(f'#define {arr_name.upper()}_RGB_SIZE {len(pixels)}')
    lines.append(f'#define {arr_name.upper()}_RGB_WIDTH {w}')
    lines.append(f'#define {arr_name.upper()}_RGB_HEIGHT {h}')
    text = '\n'.join(lines) + '\n'

    if out_path:
        with open(out_path, 'w') as f:
            f.write(text)
        print(f'Wrote {out_path} ({w}x{h}, {len(pixels)} bytes)', file=sys.stderr)
    else:
        print(text)
