#ifndef GUI_H
#define GUI_H

#include "misc.h"
#include "vga.h"


typedef struct interface_button
{
    char* text;
    int top, left, width, height;
    bool pressed;
    void (*handler) ();
} interface_button;

interface_button make_button(char* text, int x, int y, int width, int height, void (*handler) ());

void load_font(char* filename);

void draw_char(byte* target, char c, int x, int y);

void draw_text(byte* target, char* str, int x, int y);

void rect(byte* target, int x, int y, int width, int height, byte color);

void bevel_box(byte* target, int x, int y, int width, int height, bool raised);

void draw_button(byte* target, interface_button b);

bool point_on_button(interface_button b, int x, int y);

#endif