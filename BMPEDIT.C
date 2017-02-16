#include <stdio.h>
#include <stdlib.h>
#include <dos.h>

#include "bmp.h"
#include "vga.h"
#include "keyb.h"
#include "mouse.h"

#define timesWidth(x) ((x<<8)+(x<<6))

#define overpixel(x,y) VGA[(y<<8)+(y<<6)+x]
#define QUIT_KEY 1

#define BUTTON_COUNT 6
#define SAVE_BUTTON_INDEX 0
#define CHFRAME_BUTTON_INDEX 1
#define NEXTFRAME_BUTTON_INDEX 2
#define PREVFRAME_BUTTON_INDEX 3
#define COPY_BUTTON_INDEX 4
#define PASTE_BUTTON_INDEX 5
#define true 1
#define false 0

#define MAX_FRAMES 64

typedef int bool;

int frame_width = 64;
int frame_height = 64;

char coords_text[16];
char frames_text[16];

typedef struct interface_button
{
    char* text;
    int top, left, width, height;
    bool pressed;
    void (*handler) ();
} interface_button;

/*byte *overlay_buffer;*/

byte *font;
byte* copybuf;
int frame_count = 1;
int current_frame = 0;
byte** art_frames;
char* current_file = NULL;

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

bool point_in_palette_area(int x, int y) {
    return (x < 64) && (y < 64);
}

bool point_in_draw_area(int x, int y) {
    return (x >= 128) && (x < 256) && (y >= 16) && (y < 144);
}

void memcpy_skip_zeroes(char* dest, char* origin, size_t count) {
    while(count > 0) {
        if(*origin != NULL) *dest = *origin;
        dest++;
        origin++;
        count--;
    }
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

void draw_button(byte* target, interface_button b) {

    rect(target, b.left+2, b.top+2, b.width, b.height, 22);
    rect(target, b.left+1, b.top+1, b.width, b.height, 24);

    rect(target, b.left-2, b.top - 2, b.width, b.height, 30);
    rect(target, b.left-1, b.top - 1, b.width, b.height, 28);

    rect(target, b.left, b.top, b.width, b.height, 26);

    draw_text(target, b.text, b.left, b.top);
}

bool point_on_button(interface_button b, int x, int y) {
    return (x >= b.left) && (x < (b.left + b.width)) && (y >= b.top) && (y < (b.top + b.height));
}

void load_art(char* filename) {
    bmp_head header;
    palette_color palette_table[256];
    byte* artbuf = load_bmp(&header, palette_table, filename);
    int i;

    if(artbuf == NULL) return;

    frame_count = header.height / header.width;
    frame_width = header.width;
    frame_height = header.width;

    for(i = 0; i < frame_count; i++) {
        art_frames[i] = artbuf + (sizeof(byte) * frame_width * frame_height * i);
    }

    /*for(i = 0; i < 64; i++) {
        memcpy(graphic_buffer + (320 * (i + 136)), artbuf+((63 - i)*64), 64);
    }

    for(i = 0; i < 4096; i++) {
        rect(graphic_buffer, ((i % 64) << 1) + 128, (126 - ((i / 64) << 1)) + 16, 2, 2, artbuf[i]);
    }*/
}

void save_art(char* filename) {
    union REGS regs;
    byte* artbuf;
    palette_color palette_table[256];
    bmp_head header;
    raw_color raw_palette[256];
    int i, rows;

    rows = frame_count * frame_height;
    artbuf = malloc(sizeof(byte) * frame_width * rows);

    memset(raw_palette, 0, 256 * 3);

    regs.x.ax = 0x1017;
    regs.x.bx = 0;
    regs.x.cx = 0x100;
    regs.x.dx = (int) raw_palette;

    int86(0x10, &regs, &regs);

    for(i = 0; i < frame_count; i++) {
        memcpy(artbuf + (i * frame_width * frame_height), art_frames[i], frame_width * frame_height);
    }

    for(i = 0; i < 256; i++) {
        palette_table[i].index = i;
        palette_table[i].red = raw_palette[i].r << 2;
        palette_table[i].green = raw_palette[i].g << 2;
        palette_table[i].blue = raw_palette[i].b << 2;
    }


    header = make_bmp_head(frame_width, rows);
    save_bmp(&header, palette_table, artbuf, filename);

}

void show_frame(int frame_num) {
    int i;

    rect(graphic_buffer, 128, 16, 128, 128, 0);
    rect(graphic_buffer, 0, 136, 64, 64, 0);

    for(i = 0; i < frame_height; i++) {
        memcpy(graphic_buffer + (320 * (i + 136)), art_frames[frame_num]+((63 - i)*64), 64);
    }

    for(i = 0; i < frame_width * frame_height; i++) {
        rect(graphic_buffer, ((i % frame_width) << 1) + 128, (126 - ((i / frame_width) << 1)) + 16, 2, 2, (art_frames[frame_num])[i]);
    }

    sprintf(frames_text, "%d/%d", current_frame+1, frame_count);
}

void copy_frame() {
    memcpy(copybuf, art_frames[current_frame], frame_width * frame_height);
}

void paste_frame() {
    memcpy(art_frames[current_frame], copybuf, frame_width * frame_height);
    show_frame(current_frame);
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

void change_frame_count(int new_frame_count) {
    int i;
    if(new_frame_count == 0) return;
    if(new_frame_count > MAX_FRAMES) new_frame_count = MAX_FRAMES;
    if(new_frame_count > frame_count) {
        for(i = frame_count; i < new_frame_count; i++) {
            if(art_frames[i] == NULL) {
                art_frames[i] = malloc(sizeof(byte) * frame_width * frame_height);
                memset(art_frames[i], 0, sizeof(byte) * frame_width * frame_height);
            }
        }
        frame_count = new_frame_count;
    } else {
        frame_count = new_frame_count;
    }
    sprintf(frames_text, "%d/%d", current_frame+1, frame_count);
}

void next_frame() {
    current_frame = (current_frame+1) % frame_count;
    show_frame(current_frame);
}

void prev_frame() {
    if(current_frame == 0) {
        current_frame = frame_count - 1;
    } else {
        current_frame--;
    }

    show_frame(current_frame);
}

void ch_frame() {
    int newframes;
    deinit_keyboard();
    printf("How many frames?\n");
    scanf("%d", &newframes);
    init_keyboard();
    change_frame_count(newframes);
}

void save_pressed() {
    if(current_file) {
        save_art(current_file);
        printf("SAVED\r");
    } else {
        show_buffer();
        draw_text(VGA, "TYPE FILENAME", 4, 84);
        current_file = malloc(32);
        deinit_keyboard();
        scanf("%16s", current_file);
        init_keyboard();
        save_art(current_file);
        rect(graphic_buffer, 128, 4, 192, 8, 26);
        draw_text(graphic_buffer, current_file, 128,4);
    }
}


void main(int argc, char** argv) {
    int keystates = 0;
    unsigned i, k;
    int cursorX = 0, cursorY = 0, buttons = 0, prev_buttons = 0;
    char* save_text = "SAVE";
    char* chframes_text = "SET FRAMES";
    char* next_text = "+";
    char* prev_text = "-";
    
    byte current_color = 14;
    interface_button iface_buttons[BUTTON_COUNT];
    raw_color grabbed_palette[256];

    int keymap[8] = {
        1,
        75,
        77,
        72,
        80,
        57,
        44,
        28
    };

    art_frames = malloc(sizeof(byte*) * MAX_FRAMES);
    copybuf = malloc(sizeof(byte*) * frame_width * frame_height);
    for(i=0;i<MAX_FRAMES;i++) {
        art_frames[i] = NULL;
    }

    iface_buttons[SAVE_BUTTON_INDEX].text = save_text;
    iface_buttons[SAVE_BUTTON_INDEX].top = 32;
    iface_buttons[SAVE_BUTTON_INDEX].left = 264;
    iface_buttons[SAVE_BUTTON_INDEX].width = 32;
    iface_buttons[SAVE_BUTTON_INDEX].height = 8;
    iface_buttons[SAVE_BUTTON_INDEX].pressed = false;
    iface_buttons[SAVE_BUTTON_INDEX].handler = save_pressed;

    iface_buttons[CHFRAME_BUTTON_INDEX].text = chframes_text;
    iface_buttons[CHFRAME_BUTTON_INDEX].top = 100;
    iface_buttons[CHFRAME_BUTTON_INDEX].left = 4;
    iface_buttons[CHFRAME_BUTTON_INDEX].width = 80;
    iface_buttons[CHFRAME_BUTTON_INDEX].height = 8;
    iface_buttons[CHFRAME_BUTTON_INDEX].pressed = false;
    iface_buttons[CHFRAME_BUTTON_INDEX].handler = ch_frame;

    iface_buttons[PREVFRAME_BUTTON_INDEX].text = prev_text;
    iface_buttons[PREVFRAME_BUTTON_INDEX].top = 112;
    iface_buttons[PREVFRAME_BUTTON_INDEX].left = 4;
    iface_buttons[PREVFRAME_BUTTON_INDEX].width = 8;
    iface_buttons[PREVFRAME_BUTTON_INDEX].height = 8;
    iface_buttons[PREVFRAME_BUTTON_INDEX].pressed = false;
    iface_buttons[PREVFRAME_BUTTON_INDEX].handler = prev_frame;

    iface_buttons[NEXTFRAME_BUTTON_INDEX].text = next_text;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].top = 112;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].left = 20;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].width = 8;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].height = 8;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].pressed = false;
    iface_buttons[NEXTFRAME_BUTTON_INDEX].handler = next_frame;

    iface_buttons[COPY_BUTTON_INDEX].text = "COPY";
    iface_buttons[COPY_BUTTON_INDEX].top = 176;
    iface_buttons[COPY_BUTTON_INDEX].left = 128;
    iface_buttons[COPY_BUTTON_INDEX].width = 32;
    iface_buttons[COPY_BUTTON_INDEX].height = 8;
    iface_buttons[COPY_BUTTON_INDEX].pressed = false;
    iface_buttons[COPY_BUTTON_INDEX].handler = copy_frame;

    iface_buttons[PASTE_BUTTON_INDEX].text = "PASTE";
    iface_buttons[PASTE_BUTTON_INDEX].top = 176;
    iface_buttons[PASTE_BUTTON_INDEX].left = 176;
    iface_buttons[PASTE_BUTTON_INDEX].width = 40;
    iface_buttons[PASTE_BUTTON_INDEX].height = 8;
    iface_buttons[PASTE_BUTTON_INDEX].pressed = false;
    iface_buttons[PASTE_BUTTON_INDEX].handler = paste_frame;

    /*overlay_buffer = malloc(64000L);*/

    init_mouse();
    init_keyboard();
    set_mode(VGA_256_COLOR_MODE);

    /*draw palette */

    for(i = 0; i < 256; i++) {
        int x = (i % 16) << 2;
        int y = (i >> 4) << 2;
        rect(graphic_buffer, x, y, 4, 4, i);
    }

    rect(graphic_buffer, 0, 0, 320, 200, 26);

    rect(graphic_buffer, 130, 18, 128, 128, 28);
    rect(graphic_buffer, 129, 17, 128, 128, 30);

    rect(graphic_buffer, 126, 14, 128, 128, 24);
    rect(graphic_buffer, 127, 15, 128, 128, 22);

    for(i = 0; i < 256; i++) {
        int x = (i % 16) << 2;
        int y = (i >> 4) << 2;
        rect(graphic_buffer, x, y, 4, 4, i);
    }

    rect(graphic_buffer, 128, 16, 128, 128, 0);

    rect(graphic_buffer, 0, 135, 64, 1, 24);
    rect(graphic_buffer, 64, 135, 1, 65, 28);
    rect(graphic_buffer, 0, 136, 64, 64, 0);

    load_font("FONT.BMP");

    if(argc == 2) {
        current_file = argv[1];
        load_art(current_file);
        draw_text(graphic_buffer, current_file, 128,4);
    } else {
        draw_text(graphic_buffer, "UNTITLED", 128,4);
        art_frames[0] = malloc(sizeof(byte) * frame_width * frame_height);
        memset(art_frames[0], 0, sizeof(byte) * frame_width * frame_height);
    }

    for(i = 0; i < BUTTON_COUNT; i++) {
        draw_button(graphic_buffer, iface_buttons[i]);
    }

    show_frame(0);
    sprintf(frames_text, "%d/%d", current_frame+1, frame_count);

    while(!(keystates & QUIT_KEY)) {
        int frameX, frameY, boxX, boxY, miniX, miniY;
        cursor_pos(&cursorX, &cursorY, &buttons);
        frameX = ((cursorX - 128) >> 1);
        frameY = ((cursorY - 16) >> 1);
        boxX = ((cursorX - 128)  & ~1) + 128;
        boxY = ((cursorY - 16) & ~1) + 16;
        miniX = ((cursorX - 128) >> 1);
        miniY = ((cursorY - 16) >> 1) + 136;

        
        if(buttons & 1) {
            if(point_in_palette_area(cursorX, cursorY)) {
                current_color = pixel(cursorX, cursorY);
            } else if(point_in_draw_area(cursorX, cursorY)) {
                (art_frames[current_frame])[((frame_height - frameY - 1) * frame_width) + frameX] = current_color;
                /*show_frame(current_frame);*/
                /*pixel(cursorX, cursorY) = current_color;*/
                pixel(miniX, miniY) = current_color;
                rect(graphic_buffer, boxX, boxY, 2, 2, current_color);
                
            } else if(!(prev_buttons & 1)) {
                for(i = 0; i < BUTTON_COUNT; i++) {
                    if(point_on_button(iface_buttons[i], cursorX, cursorY)) {
                        iface_buttons[i].handler();
                    }
                }
            }
        } else if(buttons & 2) {
            current_color = pixel(cursorX, cursorY);
        }

        if(point_in_draw_area(cursorX, cursorY)) {
            sprintf(coords_text, "%d,%d", cursorX, cursorY);
        }

        if(keystates & 2) {
            prev_frame();
        } else if(keystates & 4) {
            next_frame();
        }


        rect(graphic_buffer, 0, 64, 32, 16, pixel(cursorX, cursorY));
        rect(graphic_buffer, 32, 64, 32, 16, current_color);
        rect(VGA, cursorX - 2, cursorY, 5, 1, 15);
        rect(VGA, cursorX, cursorY - 2, 1, 5, 15);
        overpixel(cursorX, cursorY) = pixel(cursorX, cursorY);

        wait_retrace();
        show_buffer();
        draw_text(VGA, coords_text, 128, 152);
        draw_text(VGA, frames_text, 4, 124);
        /*printf("%d,%d\r", (cursorX - 128) >> 1, (cursorY - 16) >> 1);*/
        keystates = update_keystates(keystates, keymap, 8);
        prev_buttons = buttons;
    }

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
    wait_retrace();

    set_mode(VGA_TEXT_MODE);
    deinit_keyboard();
    free(font);
    /*free(overlay_buffer);*/
    printf("Thanks for using BMPEDIT! -Clyde");
    return;
}

