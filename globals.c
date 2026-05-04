#include <stdint.h>
#include "hardware/pio.h"

PIO vga_capture_pio = pio0;
uint vga_capture_sm = 0;

PIO vga_out_pio = pio1;
uint rgb5_sm = 1;

uint vga_capture_offset;
