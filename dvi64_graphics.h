/*
* BSD 3-Clause License
* 
* Copyright (c) 2026, Peter Stansfeld
*/

#ifndef DVI_GRAPHICS_H
#define DVI_GRAPHICS_H

#include <stdint.h>

#define MODE_H_SYNC_POLARITY 0
#define MODE_H_FRONT_PORCH   16
#define MODE_H_SYNC_WIDTH    96
#define MODE_H_BACK_PORCH    48
#define MODE_H_ACTIVE_PIXELS 640

#define MODE_V_SYNC_POLARITY 0
#define MODE_V_FRONT_PORCH   10
#define MODE_V_SYNC_WIDTH    2
#define MODE_V_BACK_PORCH    33
#define MODE_V_ACTIVE_LINES  480

#define RED_BITS        0b110000
#define DARK_GREEN_BITS 0b000100
#define MED_GREEN_BITS  0b001000
#define GREEN_BITS      (MED_GREEN_BITS | DARK_GREEN_BITS)
#define BLUE_BITS       0b000011

enum DVI_COLOURS {
    DC_BLACK, DC_DARK_GREEN, DC_MED_GREEN, DC_GREEN,
    DC_DARK_BLUE, DC_BLUE, DC_LIGHT_BLUE, DC_CYAN,
    DC_RED, DC_DARK_ORANGE, DC_ORANGE, DC_YELLOW, 
    DC_MAGENTA, DC_PINK, DC_LIGHT_PINK, DC_WHITE};

// We can pack 5 6-bit (RGB222) pixels into 32 bits (4 bytes),
// so our frame buffer is 4/5 the size, saving 307,200 - 245,760 = 61440 bytes.
// extern char dvi_framebuf[((640 * 4) / 5) * 480];
extern char* dvi_framebuf;

uint8_t get_four_bit_col(uint8_t);

// Fill the dvi_framebuf with 16 vertical coloured bars.
void dvi_testbars();

// Initialises the HSTX peripheral to drive a DVI monitor at 640 x 480
// and then calls dvi_testbars().
void dvi_init();


void dvi_reinit();

void dvi_deinit();

uint32_t dvi_get_expand_tmds();

uint32_t dvi_get_expand_shift();

uint32_t dvi_get_csr();

uint32_t dvi_get_v_scanline();


#endif
