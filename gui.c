#include "vga.h"
#include "bmp.h"
#include "gui.h"

byte *font;

interface_button make_button(char* text, int x, int y, int width, int height, void (*handler) ()) {
    interface_button newifacebtn;
    newifacebtn.text = text;
    newifacebtn.top = y;
    newifacebtn.left = x;
    newifacebtn.width = width;
    newifacebtn.height = height;
    newifacebtn.handler = handler;
    return newifacebtn;
}

void load_font(char* filename) {
    bmp_head header;
    palette_color palette_table[256];
    font = load_bmp(&header, palette_table, filename);
}

void draw_char(byte* target, char c, int x, int y) {
    int i = 0;
    int cx, cy;
    if(c < 65 || c > 90) {
        if(c >= 97 && c <= 122) {
            c -= 97;
        }
        else if((c < 48) || (c > 57)) {
            if((c < 42) || (c > 47)) {
                return;
            } else {
                c += 16;
            }
        }
    } else {
        c -= 65;
    }
    

    cx = (c & 7) << 3;
    cy = c & 248;
    cy = 63 - cy;

    target += x + timesWidth(y);

    for(i = 0; i < 8; i++) {
        memcpy_skip_zeroes(target, font + cx + (cy * 64), 8);
        cy --;
        target += 320;
    }
}

void draw_text(byte* target, char* str, int x, int y) {
    while(*str != 0) {
        draw_char(target,*str, x, y);
        x += 8;
        str++;
    }
}


void rect(byte* target, int x, int y, int width, int height, byte color) {
    int i, addr;
    if(x < 0) {
        width += x;
        x = 0;
        if(width <= 0) return;
    }
    if(y < 0) {
        height += y;
        y = 0;
        if(height <= 0) return;
    }
    addr = timesWidth(y) + x;
    if(x >= 320) x = 319;
    if(y >= 200) y = 199;
    if(x + width > 320) width = 320 - x;
    if(y + height > 200) height = 200 - y;
    for(i = 0; i < height; i++) {
        memset(target + addr, color, width);
        addr += 320;
    }
}

void bevel_box(byte* target, int x, int y, int width, int height, bool raised) {
    int inc = raised ? 1 : -1;
    rect(target, x+2, y+2, width, height, 26-(inc*4));
    rect(target, x+1, y+1, width, height, 26-(inc*2));
    rect(target, x-2, y-2, width, height, 26+(inc*4));
    rect(target, x-1, y-1, width, height, 26+(inc*2));
}

void draw_button(byte* target, interface_button b) {

    bevel_box(target, b.left, b.top, b.width, b.height, true);

    rect(target, b.left, b.top, b.width, b.height, 26);

    draw_text(target, b.text, b.left, b.top);
}

bool point_on_button(interface_button b, int x, int y) {
    return (x >= b.left) && (x < (b.left + b.width)) && (y >= b.top) && (y < (b.top + b.height));
}