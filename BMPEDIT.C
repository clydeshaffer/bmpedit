#include <stdio.h>
#include <stdlib.h>
#include <dos.h>

#include "bmp.h"
#include "vga.h"
#include "keyb.h"
#include "mouse.h"

#define timesWidth(x) ((x<<8)+(x<<6))
#define sgn(x) (x>0)?1:-(x<0)

#define overpixel(x,y) VGA[(y<<8)+(y<<6)+x]
#define QUIT_KEY 1

#define BUTTON_COUNT 9
#define SAVE_BUTTON_INDEX 0
#define CHFRAME_BUTTON_INDEX 1
#define NEXTFRAME_BUTTON_INDEX 2
#define PREVFRAME_BUTTON_INDEX 3
#define COPY_BUTTON_INDEX 4
#define PASTE_BUTTON_INDEX 5
#define PENCIL_BUTTON_INDEX 6
#define FILL_BUTTON_INDEX 7
#define LINE_BUTTON_INDEX 8
#define true 1
#define false 0

#define MAX_FRAMES 64

#define FILL_QUEUE_SIZE 4096

#define MODE_PENCIL 0
#define MODE_FILL 1
#define MODE_LINE_START 2
#define MODE_LINE_END 3

typedef int bool;

int tool_mode = 0;

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

typedef struct coords {
    int x, y;
} coords;

int fill_queue_head = 0, fill_queue_tail = 0;
coords fill_queue[FILL_QUEUE_SIZE];

coords line_start;

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

void draw_line_screen(byte* target, int ax, int ay, int bx, int by, byte color) {
    int dx = bx - ax,
        dy = by - ay,
        absx = abs(dx),
        absy = abs(dy),
        sx = sgn(dx),
        sy = sgn(dy),
        stx = absx >> 1,
        sty = absy >> 1,
        i, px = ax, py = ay;

    int *dmajor, *smajor, *sminor, *stminor, *absmajor, *absminor, *pmajor, *pminor;

    if(abs(dx) > abs(dy)) {
        dmajor = &dx;
        smajor = &sx;
        sminor = &sy;
        stminor = &sty;
        absmajor = &absx;
        absminor = &absy;   
        pmajor = &px;
        pminor = &py;   
    } else {
        dmajor = &dy;
        smajor = &sy;
        sminor = &sx;
        stminor = &stx;
        absmajor = &absy;
        absminor = &absx;   
        pmajor = &py;
        pminor = &px;
    }

    for(i = 0; i <= abs(*dmajor); i ++) {
        *stminor += *absminor;
        if(*stminor >= *absmajor) {
            *stminor -= *absmajor;
            *pminor += *sminor;
        }
        *pmajor += *smajor;
        bufpixel(target,px, py) = color;
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

void select_pencil() {
    tool_mode = MODE_PENCIL;
}

void select_fill() {
    tool_mode = MODE_FILL;
}

void select_line_start() {
    if(tool_mode != MODE_LINE_END) {
        tool_mode = MODE_LINE_START;
    }
}

void mark_line_start(int x, int y) {
    line_start.x = x;
    line_start.y = y;
    tool_mode = MODE_LINE_END;
}

void reset_fill_queue() {
    fill_queue_head = 0;
    fill_queue_tail = 0;
}

void enqueue_fill(coords v) {
    if(v.x < 0 || v.x >= frame_width) return;
    if(v.y < 0 || v.y >= frame_height) return;
    fill_queue[fill_queue_tail] = v;
    fill_queue_tail = (fill_queue_tail+1) % FILL_QUEUE_SIZE;
}

coords dequeue_fill() {
    coords head = fill_queue[fill_queue_head];
    fill_queue_head = (fill_queue_head+1) % FILL_QUEUE_SIZE;
    return head;
}

bool coords_in_frame(coords v) {
    if(v.x < 0 || v.x >= frame_width) return false;
    if(v.y < 0 || v.y >= frame_height) return false;
    return true;
}

byte* pointer_for_xy(int x, int y) {
    return &((art_frames[current_frame])[((frame_height - y - 1) * frame_width) + x]);
}

byte* pointer_for_coords(coords v) {
    return &((art_frames[current_frame])[((frame_height - v.y - 1) * frame_width) + v.x]);
}

coords get_north(coords v) {
    v.y--;
    return v;
}

coords get_south(coords v) {
    v.y++;
    return v;
}

coords get_east(coords v) {
    v.x++;
    return v;
}

coords get_west(coords v) {
    v.x --;
    return v;
}

void flood_fill(int x, int y, byte old_color, byte new_color) {
    coords point;
    if(old_color == new_color) return;
    point.x = x;
    point.y = y;
    if(*(pointer_for_coords(point)) != old_color) return;
    reset_fill_queue();
    enqueue_fill(point);
    *(pointer_for_coords(point)) = new_color;
    while(fill_queue_head != fill_queue_tail) {
        coords N, S, E, W;
        point = dequeue_fill();
        W = get_west(point);
        E = get_east(point);
        N = get_north(point);
        S = get_south(point);
        if(coords_in_frame(W) && (*(pointer_for_coords(W)) == old_color)) {
            enqueue_fill(W);
            *(pointer_for_coords(W)) = new_color;
        }
        if(coords_in_frame(E) && (*(pointer_for_coords(E)) == old_color)) {
            enqueue_fill(E);
            *(pointer_for_coords(E)) = new_color;
        }
        if(coords_in_frame(N) && (*(pointer_for_coords(N)) == old_color)) {
            enqueue_fill(N);
            *(pointer_for_coords(N)) = new_color;
        }
        if(coords_in_frame(S) && (*(pointer_for_coords(S)) == old_color)) {
            enqueue_fill(S);
            *(pointer_for_coords(S)) = new_color;
        }
    }
}

void draw_line_artbuf(int ax, int ay, int bx, int by, byte color) {
    int dx = bx - ax,
        dy = by - ay,
        absx = abs(dx),
        absy = abs(dy),
        sx = sgn(dx),
        sy = sgn(dy),
        stx = absx >> 1,
        sty = absy >> 1,
        i, px = ax, py = ay;

    int *dmajor, *smajor, *sminor, *stminor, *absmajor, *absminor, *pmajor, *pminor;

    if(abs(dx) > abs(dy)) {
        dmajor = &dx;
        smajor = &sx;
        sminor = &sy;
        stminor = &sty;
        absmajor = &absx;
        absminor = &absy;   
        pmajor = &px;
        pminor = &py;   
    } else {
        dmajor = &dy;
        smajor = &sy;
        sminor = &sx;
        stminor = &stx;
        absmajor = &absy;
        absminor = &absx;   
        pmajor = &py;
        pminor = &px;
    }

    for(i = 0; i <= abs(*dmajor); i ++) {
        *stminor += *absminor;
        if(*stminor >= *absmajor) {
            *stminor -= *absmajor;
            *pminor += *sminor;
        }
        *pmajor += *smajor;
        *(pointer_for_xy(px, py)) = color;
    }
}

void main(int argc, char** argv) {
    int keystates = 0;
    unsigned i, k;
    int cursorX = 0, cursorY = 0, buttons = 0, prev_buttons = 0;
    
    byte current_color = 14;
    interface_button iface_buttons[BUTTON_COUNT];

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

    iface_buttons[SAVE_BUTTON_INDEX].text = "SAVE";
    iface_buttons[SAVE_BUTTON_INDEX].top = 32;
    iface_buttons[SAVE_BUTTON_INDEX].left = 264;
    iface_buttons[SAVE_BUTTON_INDEX].width = 32;
    iface_buttons[SAVE_BUTTON_INDEX].height = 8;
    iface_buttons[SAVE_BUTTON_INDEX].pressed = false;
    iface_buttons[SAVE_BUTTON_INDEX].handler = save_pressed;

    iface_buttons[CHFRAME_BUTTON_INDEX].text = "SET FRAMES";
    iface_buttons[CHFRAME_BUTTON_INDEX].top = 100;
    iface_buttons[CHFRAME_BUTTON_INDEX].left = 4;
    iface_buttons[CHFRAME_BUTTON_INDEX].width = 80;
    iface_buttons[CHFRAME_BUTTON_INDEX].height = 8;
    iface_buttons[CHFRAME_BUTTON_INDEX].pressed = false;
    iface_buttons[CHFRAME_BUTTON_INDEX].handler = ch_frame;

    iface_buttons[PREVFRAME_BUTTON_INDEX].text = "-";
    iface_buttons[PREVFRAME_BUTTON_INDEX].top = 112;
    iface_buttons[PREVFRAME_BUTTON_INDEX].left = 4;
    iface_buttons[PREVFRAME_BUTTON_INDEX].width = 8;
    iface_buttons[PREVFRAME_BUTTON_INDEX].height = 8;
    iface_buttons[PREVFRAME_BUTTON_INDEX].pressed = false;
    iface_buttons[PREVFRAME_BUTTON_INDEX].handler = prev_frame;

    iface_buttons[NEXTFRAME_BUTTON_INDEX].text = "+";
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
    iface_buttons[PASTE_BUTTON_INDEX].top = 188;
    iface_buttons[PASTE_BUTTON_INDEX].left = 128;
    iface_buttons[PASTE_BUTTON_INDEX].width = 40;
    iface_buttons[PASTE_BUTTON_INDEX].height = 8;
    iface_buttons[PASTE_BUTTON_INDEX].pressed = false;
    iface_buttons[PASTE_BUTTON_INDEX].handler = paste_frame;

    iface_buttons[PENCIL_BUTTON_INDEX].text = "PENCIL";
    iface_buttons[PENCIL_BUTTON_INDEX].top = 160;
    iface_buttons[PENCIL_BUTTON_INDEX].left = 224;
    iface_buttons[PENCIL_BUTTON_INDEX].width = 48;
    iface_buttons[PENCIL_BUTTON_INDEX].height = 8;
    iface_buttons[PENCIL_BUTTON_INDEX].pressed = false;
    iface_buttons[PENCIL_BUTTON_INDEX].handler = select_pencil;

    iface_buttons[FILL_BUTTON_INDEX].text = "FILL";
    iface_buttons[FILL_BUTTON_INDEX].top = 172;
    iface_buttons[FILL_BUTTON_INDEX].left = 224;
    iface_buttons[FILL_BUTTON_INDEX].width = 32;
    iface_buttons[FILL_BUTTON_INDEX].height = 8;
    iface_buttons[FILL_BUTTON_INDEX].pressed = false;
    iface_buttons[FILL_BUTTON_INDEX].handler = select_fill;

    iface_buttons[LINE_BUTTON_INDEX].text = "LINE";
    iface_buttons[LINE_BUTTON_INDEX].top = 184;
    iface_buttons[LINE_BUTTON_INDEX].left = 224;
    iface_buttons[LINE_BUTTON_INDEX].width = 32;
    iface_buttons[LINE_BUTTON_INDEX].height = 8;
    iface_buttons[LINE_BUTTON_INDEX].pressed = false;
    iface_buttons[LINE_BUTTON_INDEX].handler = select_line_start;

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
                if(tool_mode == MODE_PENCIL) {
                    (art_frames[current_frame])[((frame_height - frameY - 1) * frame_width) + frameX] = current_color;
                    /*show_frame(current_frame);*/
                    /*pixel(cursorX, cursorY) = current_color;*/
                    pixel(miniX, miniY) = current_color;
                    rect(graphic_buffer, boxX, boxY, 2, 2, current_color);
                } else if((tool_mode == MODE_FILL) && (!(prev_buttons & 1))) {
                    byte old_color = (art_frames[current_frame])[((frame_height - frameY - 1) * frame_width) + frameX];
                    flood_fill(frameX, frameY, old_color, current_color);
                    show_frame(current_frame);
                } else if((tool_mode == MODE_LINE_START) && (!(prev_buttons & 1))) {
                    mark_line_start(frameX, frameY);
                } else if((tool_mode == MODE_LINE_END) && (!(prev_buttons & 1))) {
                    draw_line_artbuf(line_start.x, line_start.y, frameX, frameY, current_color);
                    show_frame(current_frame);
                    tool_mode = MODE_LINE_START;
                }
                
            } else if(!(prev_buttons & 1)) {
                for(i = 0; i < BUTTON_COUNT; i++) {
                    if(point_on_button(iface_buttons[i], cursorX, cursorY)) {
                        iface_buttons[i].handler();
                    }
                }
            }
        } else if((buttons & 2) && !(prev_buttons & 2)) {
            if(tool_mode == MODE_LINE_END) {
                tool_mode == MODE_LINE_START;
            } else {
                current_color = pixel(cursorX, cursorY);
            }
        }

        if(point_in_draw_area(cursorX, cursorY)) {
            sprintf(coords_text, "%d,%d", frameX, frameY);
        }

        if(keystates & 2) {
            prev_frame();
        } else if(keystates & 4) {
            next_frame();
        }

        wait_retrace();
        show_buffer();

        rect(graphic_buffer, 0, 64, 32, 16, pixel(cursorX, cursorY));
        rect(graphic_buffer, 32, 64, 32, 16, current_color);

        /* draw cursor */
        if(tool_mode == MODE_LINE_END && point_in_draw_area(cursorX, cursorY)) {
            draw_line_screen(VGA, (line_start.x << 1) + 128, (line_start.y << 1) + 16, cursorX, cursorY, current_color);
        } else {
            
            rect(VGA, cursorX - 2, cursorY, 5, 1, 15);
            rect(VGA, cursorX, cursorY - 2, 1, 5, 15);
            overpixel(cursorX, cursorY) = pixel(cursorX, cursorY);   
        }
        
        draw_text(VGA, coords_text, 128, 152);
        draw_text(VGA, frames_text, 4, 124);
        /*printf("%d,%d\r", (cursorX - 128) >> 1, (cursorY - 16) >> 1);*/
        keystates = update_keystates(keystates, keymap, 8);
        prev_buttons = buttons;
    }

    fade_out();
    wait_retrace();

    set_mode(VGA_TEXT_MODE);
    deinit_keyboard();
    free(font);
    /*free(overlay_buffer);*/
    printf("Thanks for using BMPEDIT! -Clyde");
    return;
}

