#include "vga.h"
#include <dos.h>

#define pixel(x,y) graphic_buffer[(y<<8)+(y<<6)+x]

byte *VGA=(byte *)0xA0000000L;

byte graphic_buffer[64000L];

void debug_pixel(byte color) {
    static int debug_cursor = 0;
    graphic_buffer[debug_cursor] = color;
    graphic_buffer[debug_cursor + 1] = color;
    debug_cursor = (debug_cursor + 2) % 32;
}

void wait_retrace() {
    while ((inp(INPUT_STATUS_1) & VRETRACE));
    while (!(inp(INPUT_STATUS_1) & VRETRACE));
}

void show_buffer() {

    memcpy(VGA, graphic_buffer, 64000L);
}

void set_mode(byte mode)
{
    union REGS regs;
    regs.h.ah = SET_MODE;
    regs.h.al = mode;
    int86(VIDEO_INT, &regs, &regs);
}

void setup_palette(byte *unshaded_colors) {
    int i, k;
    byte r, g, b,
        rstep, gstep, bstep;
    outp(0x03c8, 128);
    for(i = 0; i < 8; i++) {
        r = unshaded_colors[(i * 3)],
        g = unshaded_colors[(i*3) + 1],
        b = unshaded_colors[(i*3) + 2],
        rstep = r >> 5,
        gstep = g >> 5,
        bstep = b >> 5;
        for(k = 0; k < 16; k++) {
            outp(0x03c9, r);
            outp(0x03c9, g);
            outp(0x03c9, b);
            r -= rstep;
            g -= gstep;
            b -= bstep;
        }
    }
}

void get_palette(byte *dest) {
    union REGS regs;
    regs.x.ax = 0x1017;
    regs.x.bx = 0;
    regs.x.cx = 0x100;
    regs.x.dx = (int) dest;
    int86(0x10, &regs, &regs);
}

void submit_palette(byte *raw_palette) {
    int i;
    outp(0x03c8, 0);
    for(i = 0; i < 256; i++) {
        outp(0x03c9, *raw_palette);
        raw_palette++;
        outp(0x03c9, *raw_palette);
        raw_palette++;
        outp(0x03c9, *raw_palette);
        raw_palette++;
    }
}

void fade_out() {
    int i, k;
    raw_color grabbed_palette[256];
    get_palette(grabbed_palette);
    for(i = 0; i < 16; i ++) {
        for(k = 0; k < 256; k++) {
            if(grabbed_palette[k].r > 3) grabbed_palette[k].r -= 4;
            if(grabbed_palette[k].g > 3) grabbed_palette[k].g -= 4;
            if(grabbed_palette[k].b > 3) grabbed_palette[k].b -= 4;
        }
        submit_palette(grabbed_palette);
        wait_retrace();
    }
    memset(VGA, 0, 64000L);
}