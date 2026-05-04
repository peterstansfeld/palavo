#ifndef GLOBALS_H
#define GLOBALS_H

// have moved these items here mainly so that dvi_graphics can have access to them

#include <stdint.h>
#include "hardware/pio.h"

#define WORDS_PER_LINE (1 + 20)

#define NO_OF_LINES 480

#define TXCOUNT_2 WORDS_PER_LINE * NO_OF_LINES

extern uint32_t vga_1bit_data_array[TXCOUNT_2];

extern PIO vga_capture_pio;
extern uint vga_capture_sm;

extern PIO vga_out_pio;
extern uint rgb5_sm;

extern uint vga_capture_offset;

#define USE_VGA_IN 1

#endif
