/**
 * Hunter Adams (vha3@cornell.edu)
 * modifed for 16 colors by BRL4
 * 
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 470 ohm resistor ---> VGA Green 
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - GPIO 21 ---> 330 ohm resistor ---> VGA Red
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels 0, 1, 2, and 3
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 * NOTE
 *  - This is a translation of the display primitives
 *    for the PIC32 written by Bruce Land and students
 *
 */

#include <stdint.h>

// #define SYS_CLOCK_FREQ_KHZ 125000u
// #define SYS_CLOCK_FREQ_KHZ 250000u
// SYS_CLOCK_KHZ is now an SDK #define, I believe.

// Give the I/O pins that we're using some names that make sense - usable in main()
 enum vga_pins {HSYNC=16, VSYNC, LO_GRN, HI_GRN, BLUE_PIN, RED_PIN} ;

 enum test_vga_pins {HSYNC2=6, VSYNC2, LO_GRN2, HI_GRN2, BLUE2_PIN, RED2_PIN};

// We can only produce 16 (4-bit) colors, so let's give them readable names - usable in main()

/* 
enum colors {BLACK, DARK_GREEN, MED_GREEN, GREEN,
            DARK_BLUE, BLUE, LIGHT_BLUE, CYAN,
            RED, DARK_ORANGE, ORANGE, YELLOW, 
            MAGENTA, PINK, LIGHT_PINK, WHITE} ;

*/

#define DARK_BLUE_BIT 0
#define MED_BLUE_BIT 1

#define DARK_GREEN_BIT 2 
#define MED_GREEN_BIT 3

#define DARK_RED_BIT 4 
#define MED_RED_BIT 5 


// Hunter Adams' 16-colour palette definitions as 64-bit colours

#define BLACK       0
// #define DARK_GREEN  0b000001
#define DARK_GREEN  (1 << DARK_GREEN_BIT)

// #define MED_GREEN  0b000010
#define MED_GREEN   (1 << MED_GREEN_BIT)

// #define GREEN  0b000011
#define GREEN       (MED_GREEN | DARK_GREEN)

// #define DARK_BLUE   0b001100
#define DARK_BLUE   ((1 << MED_BLUE_BIT) | (1 << DARK_BLUE_BIT))

// #define BLUE        0b001101
#define BLUE        (DARK_BLUE | DARK_GREEN)

// #define LIGHT_BLUE  0b001110
#define LIGHT_BLUE  (DARK_BLUE | MED_GREEN)

// #define CYAN        0b001111
#define CYAN        (DARK_BLUE | GREEN)

// #define RED         0b110000
#define RED         ((1 << MED_RED_BIT) | (1 << DARK_RED_BIT))

// #define DARK_ORANGE 0b110001
#define DARK_ORANGE (RED | DARK_GREEN)

// #define ORANGE      0b110010
#define ORANGE      (RED | MED_GREEN)

// #define YELLOW      0b110011
#define YELLOW      (RED | GREEN)

// #define MAGENTA     0b111100
#define MAGENTA     (RED | DARK_BLUE)

// #define PINK        0b111101
#define PINK        ((RED | DARK_BLUE) | DARK_GREEN)

// #define LIGHT_PINK  0b111110
#define LIGHT_PINK  ((RED | DARK_BLUE) | MED_GREEN)

// #define WHITE       0b111111
#define WHITE       (RED | DARK_BLUE | GREEN)

// end of Hunter Adams' 16-colour palette definitions as 64-bit colours


// 64-bit colour palette

#define DARK_RED_64    (1 << DARK_RED_BIT)

#define DARK_GREEN_64  (1 << DARK_GREEN_BIT)

#define DARK_BLUE_64  (1 << DARK_BLUE_BIT)

#define DARK_GREY_64  DARK_BLUE_64 | DARK_GREEN_64 | DARK_BLUE_64



#define WORDS_PER_LINE (1 + 20)

#define NO_OF_LINES 480

#define TXCOUNT_2 WORDS_PER_LINE * NO_OF_LINES

extern uint32_t vga_1bit_data_array[TXCOUNT_2];


// VGA primitives - usable in main
// void set_line_colors(uint16_t line, uint8_t back_colour, uint8_t fore_colour);
void set_line_colors(uint16_t line, uint8_t back_colour, uint8_t fore_colour, uint8_t colour_2, uint8_t colour_3);
void set_text_padding(uint16_t padding);

void initVGA(void) ;
void drawPixel(short x, short y, char color) ;
void drawVLine(short x, short y, short h, char color) ;
void drawHLine(short x, short y, short w, char color) ;
void drawLine(short x0, short y0, short x1, short y1, char color) ;
void drawRect(short x, short y, short w, short h, char color);
void drawCircle(short x0, short y0, short r, char color) ;
void drawCircleHelper( short x0, short y0, short r, unsigned char cornername, char color) ;
void fillCircle(short x0, short y0, short r, char color) ;
void fillCircleHelper(short x0, short y0, short r, unsigned char cornername, short delta, char color) ;
void drawRoundRect(short x, short y, short w, short h, short r, char color) ;
void fillRoundRect(short x, short y, short w, short h, short r, char color) ;
void fillRect(short x, short y, short w, short h, char color) ;
void drawChar(short x, short y, unsigned char c, char color, char bg, unsigned char size) ;
void setCursor(short x, short y);
void setTextColor(char c);
void setTextColor2(char c, char bg);
void setTextSize(unsigned char s);
void setTextWrap(char w);
void tft_write(unsigned char c) ;
void writeString(char* str) ;
// === added 10/11/2023 brl4
void drawCharBig(short x, short y, unsigned char c, char color, char bg) ;
void writeStringBig(char* str) ;
void setTextColorBig(char, char); //works, but can use usual setTextColor2
// 5x7 font
void writeStringBold(char* str);