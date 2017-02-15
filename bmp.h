#ifndef BMP_H
#define BMP_H
#include "vga.h"

typedef unsigned short word;
typedef unsigned long  dword;

typedef struct palette_color
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char index;
} palette_color;

typedef struct bmp_head
{
    word type;
    dword size;
    dword reserved;
    dword offset;
    dword header_size;
    dword width;
    dword height;
    word planes;
    word bits_per_pixel;
    dword compression;
    dword size_image;
    dword x_pixels_per_meter;
    dword y_pixels_per_meter;
    dword colors_used;
    dword colors_important;
} bmp_head;

bmp_head make_bmp_head(int width, int height);

void save_bmp(bmp_head *info, palette_color* palette_table, unsigned char* img_buf, char* filename);

byte* load_bmp(bmp_head *info, palette_color* palette_table, char* filename);
#endif