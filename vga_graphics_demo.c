/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 *
 * HARDWARE CONNECTIONS
   - GPIO 16 ---> VGA Hsync 
   - GPIO 17 ---> VGA Vsync 
   - GPIO 18 ---> VGA Green lo-bit --> 470 ohm resistor --> VGA_Green
   - GPIO 19 ---> VGA Green hi_bit --> 330 ohm resistor --> VGA_Green
   - GPIO 20 ---> 330 ohm resistor ---> VGA-Blue 
   - GPIO 21 ---> 330 ohm resistor ---> VGA-Red 
   - RP2040 GND ---> VGA-GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels obtained by claim mechanism
 *  - 153.6 kBytes of RAM (for pixel color data)
 *
 */

// VGA graphics library
#include "vga16_graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/uart.h"

#define UART_ID uart1
#define BAUD_RATE 115200

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// Some globals for storing timer information
volatile unsigned int time_accum = 0;
unsigned int time_accum_old = 0 ;
char timetext[40];

// Timer interrupt
bool repeating_timer_callback(struct repeating_timer *t) {

    time_accum += 1 ;
    return true;
}


int main() {

    // Initialize stdio
    stdio_init_all();

    printf("Initialising VGA...\n");


    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_puts(UART_ID, "\n Hello, UART!\n");


    // Initialize the VGA screen
    initVGA() ;

    uart_puts(UART_ID, "\n Hello, UART!\n");



    // animation pause
    bool pause = false;

    // circle radii:
    short circle_x = 0 ;

    // color chooser
    char color_index = 0 ;

    // position of the disc primitive
    short disc_x = 0 ;
    // position of the box primitive
    short box_x = 0 ;
    // position of vertical line primitive
    short Vline_x = 350;
    // position of horizontal line primitive
    short Hline_y = 250;

    // Draw some filled rectangles
    fillRect(64, 0, 176, 50, BLUE); // blue box
    fillRect(250, 0, 176, 50, RED); // red box:
    fillRect(435, 0, 176, 50, GREEN); // green box
    
//    drawVLine(Vline_x, 300, (Vline_x>>2), color_index);

    drawVLine(0, 0, 16, WHITE);
    drawVLine(2, 0, 16, WHITE);

    drawVLine(9, 0, 16, WHITE);
    drawVLine(11, 0, 16, WHITE);

 //   drawVLine(637, 0, 16, WHITE);

    drawHLine(0, 479, 16, WHITE);
    drawVLine(0, 480 - 16, 16, WHITE);

    drawHLine(640 - 16, 0, 16, WHITE);
    drawVLine(639, 0, 16, WHITE);

    drawVLine(639, 480 - 16, 16, WHITE);
    drawHLine(640 - 16, 479, 16, WHITE);

    // drawHLine(630, 480 - 1, 1, WHITE);


    for (int i = 0; i < 16; i++){
        drawPixel(i, 0, i); // test all colours in first horizontal line following sync
    }


    for (int i = 0; i < 16; i++){
        fillRect(i * 40, 51, 40, 40, i); // colour boxes
    }

    // Write some text
    setTextColor(WHITE) ;
    setCursor(65, 0) ;
    setTextSize(1) ;
    writeString("Raspberry Pi Pico") ;
    setCursor(65, 10) ;
    writeString("Graphics primitives demo") ;
    setCursor(65, 20) ;
    writeString("Hunter Adams") ;
    setCursor(65, 30) ;
    writeString("vha3@cornell.edu") ;
    setCursor(65, 40) ;
    writeString("4-bit mod by Bruce Land") ;
    setCursor(250, 0) ;
    setTextSize(2) ;
    writeString("Time Elapsed:") ;

    // Setup a 1Hz timer
    struct repeating_timer timer;
    add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);

    while(true) {

        if (!pause) {

            // Modify the color chooser
            if (color_index ++ == 15) color_index = 0 ;

            // A row of filled circles
            fillCircle(disc_x, 100, 20, color_index);
            disc_x += 35 ;
            if (disc_x > 640) disc_x = 0;

            // Concentric empty circles
            drawCircle(320, 200, circle_x, color_index);
            circle_x += 1 ;        
            if (circle_x > 130) circle_x = 0;

            // A series of rectangles
            drawRect(10, 300, box_x, box_x, color_index);
            box_x += 5 ;
            if (box_x > 195) box_x = 10;

            // Random lines
            drawLine(210+(rand()&0x7f), 350+(rand()&0x7f), 210+(rand()&0x7f), 
                    350+(rand()&0x7f), color_index);

            // Vertical lines
            drawVLine(Vline_x, 300, (Vline_x>>2), color_index);
            Vline_x += 2 ;
            if (Vline_x > 620) Vline_x = 350;
            
            // Horizontal lines
            drawHLine(400, Hline_y, 150, color_index);
            Hline_y += 2 ;
            if (Hline_y > 400) Hline_y = 240;

            // Timing text
            if (time_accum != time_accum_old) {
                time_accum_old = time_accum ;
                fillRect(250, 20, 176, 30, RED); // red box
                sprintf(timetext, "%d", time_accum) ;
                setCursor(250, 20) ;
                setTextSize(2) ;
                writeString(timetext) ;
            }
        }

        /*
        
        // cant do this getchar() is a blocking function
        int uart_char = getchar();
        if (uart_char != EOF) {
            // a char has been received
            if ((char)uart_char == ' ') {
                pause = !pause; // toggle pause
                printf(" SPACE ");
            } else {
                putchar(uart_char);
            }
        }
        */

// cant do this getchar() is a blocking function

        if (uart_is_readable(UART_ID)) {
            uint8_t ch = uart_getc(UART_ID);
            // Can we send it back?
            if (uart_is_writable(UART_ID)) {
                uart_putc(UART_ID, ch);
            }
            if ((char)ch == ' ') {
                pause = !pause; // toggle pause
            }
        }
        

        // A brief nap
        sleep_ms(10) ;

        // uart_putc(UART_ID, 'B');

   }

}