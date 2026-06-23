#ifndef BMP_H
#define BMP_H

#include <stdint.h>

typedef struct {
    int width;
    int height;
    int bpp;
    uint8_t* pixels;
} bmp_image_t;

int bmp_decode(const uint8_t* data, int len, bmp_image_t* img);
void bmp_free(bmp_image_t* img);

#endif