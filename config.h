// Bit locations in PALAVO_CONFIG
// See README.md for more details on PALAVO_CONFIG

#ifndef _PALAVO_CONFIG_H
#define _PALAVO_CONFIG_H

// PALAVO_CONFIG bit locations
#define PC_BIT_USE_DVI 0

// Leave all of the [Raspberry Pi] Pico's GPIO (0 to 22, 26, 27 & 28) free to
// be used as inputs and move the user interface GPIO to beyond GPIO30 
#define PC_BIT_USE_GPIO_31_47 1

#define PC_BIT_USE_VGA_IN_TO_DVI 2
#define PC_BIT_USE_IR 3
#define PC_BIT_USE_CSYNC 4
#define PC_BIT_USE_USB_STDIO 5

#endif