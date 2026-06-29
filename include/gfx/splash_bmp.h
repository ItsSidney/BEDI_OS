#ifndef GFX_SPLASH_BMP_H
#define GFX_SPLASH_BMP_H

#include <stdint.h>

/* Embedded BEDI_BANNER.bmp (1500x500, 24-bit) */
extern const unsigned char bedi_banner_bmp[];
extern const unsigned int bedi_banner_bmp_len;
extern const int bedi_banner_w;
extern const int bedi_banner_h;

/* Embedded logo.bmp (400x400, 24-bit) */
extern const unsigned char bedi_logo_bmp[];
extern const unsigned int bedi_logo_bmp_len;
extern const int bedi_logo_w;
extern const int bedi_logo_h;

/* Draw a 24-bit BMP (Windows 3.x, 54-byte header) into a back buffer.
   Pass destination buffer, stride, and width/height explicitly.
   src_w/src_h allow drawing a sub-region of the source BMP (0 = full size).
   Returns 0 on success, -1 on parse failure. */
int draw_bmp(int x, int y, const unsigned char* data, unsigned int len,
             uint32_t* dst_base, int dst_stride, int dst_w, int dst_h,
             unsigned int src_w, unsigned int src_h,
             int draw_w, int draw_h);

/* Same as draw_bmp, but treats pure black (0,0,0) as transparent. */
int draw_bmp_black_transparent(int x, int y, const unsigned char* data, unsigned int len,
                               uint32_t* dst_base, int dst_stride, int dst_w, int dst_h,
                               unsigned int src_w, unsigned int src_h,
                               int draw_w, int draw_h);

#endif
