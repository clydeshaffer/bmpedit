#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include "bmp.h"

bmp_head make_bmp_head(int width, int height)
{
	bmp_head result;
	result.type = 0x4d42;
	result.size = 54 + (width * height) + 1024;
	result.reserved = 0;
	result.offset = 54;
	result.header_size = 40;
	result.width = width;
	result.height = height;
	result.planes = 1;
	result.bits_per_pixel = 8;
	result.compression = 0;
	result.size_image = width * height;
	result.x_pixels_per_meter = 100;
	result.y_pixels_per_meter = 100;
	result.colors_used = 256;
	result.colors_important = 256;

	return result;
}

void save_bmp(bmp_head *info, palette_color* palette_table, unsigned char* img_buf, char* filename)
{
	FILE *write_ptr;
	write_ptr = fopen(filename, "wb");
	fwrite(info, 54, 1, write_ptr);
	fwrite(palette_table, 4, 256, write_ptr);
	fwrite(img_buf, 1, info->size_image, write_ptr);
	fclose(write_ptr);
}

byte* load_bmp(bmp_head *info, palette_color* palette_table, char* filename) {
	FILE *read_ptr;
	byte* image;
	read_ptr = fopen(filename, "rb");
	

	if(read_ptr == NULL) return NULL;

	fread(info, 54, 1, read_ptr);

	if(info->type != 0x4d42) {
		fclose(read_ptr);
		return NULL;
	}

	fread(palette_table, 4, 256, read_ptr);

	image = malloc(sizeof(byte) * info->size_image);
	
	fread(image, 1, info->size_image, read_ptr);

	fclose(read_ptr);
	return image;
}

