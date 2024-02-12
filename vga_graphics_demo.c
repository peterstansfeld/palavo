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
// #include "hardware/structs/bus_ctrl.h"
#include <string.h>

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


const uint CAPTURE_PIN_BASE = HSYNC2; // 16 = hsync, 17 = vsync // 22 = hsync2
const uint CAPTURE_PIN_COUNT = 4;
const uint CAPTURE_TRIGGER_PIN = VSYNC2; // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2
const uint CAPTURE_N_SAMPLES = 640 * 2 * 4; // was 96
const uint CAPTURE_SAMPLE_FREQ_DIVISOR = 5 * 4 * 4 * 4; /*271.267*/ // was 5 * 4

static inline uint bits_packed_per_word(uint pin_count) {
    // If the number of pins to be sampled divides the shift register size, we
    // can use the full SR and FIFO width, and push when the input shift count
    // exactly reaches 32. If not, we have to push earlier, so we use the FIFO
    // a little less efficiently.
    const uint SHIFT_REG_WIDTH = 32;
    return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count);
}

void logic_analyser_init(PIO pio, uint sm, uint pin_base, uint pin_count, float div) {
    // Load a program to capture n pins. This is just a single `in pins, n`
    // instruction with a wrap.
    uint16_t capture_prog_instr = pio_encode_in(pio_pins, pin_count);
    struct pio_program capture_prog = {
            .instructions = &capture_prog_instr,
            .length = 1,
            .origin = -1
    };
    uint offset = pio_add_program(pio, &capture_prog);

    // Configure state machine to loop over this `in` instruction forever,
    // with autopush enabled.
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_in_pins(&c, pin_base);
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_clkdiv(&c, div);
    // Note that we may push at a < 32 bit threshold if pin_count does not
    // divide 32. We are using shift-to-right, so the sample data ends up
    // left-justified in the FIFO in this case, with some zeroes at the LSBs.
    sm_config_set_in_shift(&c, true, true, bits_packed_per_word(pin_count));
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    pio_sm_init(pio, sm, offset, &c);
}

void logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, bool trigger_level) {
    pio_sm_set_enabled(pio, sm, false);
    // Need to clear _input shift counter_, as well as FIFO, because there may be
    // partial ISR contents left over from a previous run. sm_restart does this.
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destination pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        true                // Start immediately
    );

    // If edge detect we need to first wait for the opposite level
    // Warning! As it's blocking instruction it will... block
    pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(!trigger_level, trigger_pin));
    
    // level trigger
    pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(trigger_level, trigger_pin));
    
    // level trigger
    // pio_sm_exec(pio, sm, pio_encode_wait_gpio(1, LO_GRN2));


    pio_sm_set_enabled(pio, sm, true);
}

void print_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
    // Display the capture buffer in text form, like this:
    // 00: __--__--__--__--__--__--
    // 01: ____----____----____----
    uart_puts(UART_ID, "Capture:\n");
    // Each FIFO record may be only partially filled with bits, depending on
    // whether pin_count is a factor of 32.
    uint record_size_bits = bits_packed_per_word(pin_count);
    for (int pin = 0; pin < pin_count; ++pin) {
        // uart_puts(UART_ID, "%02d: ", pin + pin_base);
        uart_puts(UART_ID, "todo");
        for (int sample = 0; sample < n_samples; ++sample) {
            uint bit_index = pin + sample * pin_count;
            uint word_index = bit_index / record_size_bits;
            // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            uart_puts(UART_ID, buf[word_index] & word_mask ? "-" : "_");
        }
        uart_puts(UART_ID, "\n");
    }
}


void plot_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples) {
    // Display the capture buffer in graphical form, like this:
    // 00: __--__--__--__--__--__--
    // 01: ____----____----____----
    // ...only with lines
    // uart_puts(UART_ID, "Capture:\n");
    // Each FIFO record may be only partially filled with bits, depending on
    // whether pin_count is a factor of 32.


    char colours[] = {YELLOW, ORANGE, RED, GREEN, BLUE};

 

    uint record_size_bits = bits_packed_per_word(pin_count);
    
 //   fillRect(0, 51, 640, 30, BLACK); // colour boxes
    
    uint last_sample = 1;
    uint last_x = 0;

    char line_col;

    int trace_height = 20;

    uint y = 51; 
    uint y_padding = 2;


    char str[80];

    setTextSize(1);


    fillRect(0, y, 640, (trace_height * CAPTURE_PIN_COUNT) + CAPTURE_PIN_COUNT + 3, BLACK); // colour box
        
    char font_width = 5;
    char font_height = 7;

    // char colour = 0;

    for (int pin = 0; pin < pin_count; ++pin) {
        uint x = 0;

        line_col = colours[pin];


        // fillRect(0, y, 640, trace_height, BLACK); // colour boxes
        setTextColor(line_col);
        
        last_sample = 1; // fix at high for now
        last_x = 0;
        
        // uart_puts(UART_ID, "%02d: ", pin + pin_base);
        // uart_puts(UART_ID, "todo");
        for (int sample = 0; sample < n_samples; ++sample) {
            uint bit_index = pin + sample * pin_count;
            uint word_index = bit_index / record_size_bits;
            // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            //uart_puts(UART_ID, buf[word_index] & word_mask ? "-" : "_");
            
            uint sample = buf[word_index] & word_mask ? 1 :0;
            
            if (x == 0) {
                last_sample = sample;
            }


            if (sample) {
                //if (x < 640) {
                if (x < 640) {
                    drawPixel(x, y, line_col);
                }
               // }

                if (last_sample) {
                    // no need to draw yet
                    // drawPixel(x, y, line_col);
                } else {
                    if (x < 640) {
                        drawVLine(x, y, trace_height, line_col);

                        // drawHLine(last_x, y + trace_height - 1, x - last_x, line_col);
                        
                    }
                    if (x) {

                        sprintf(str,"%d", x - last_x);
                        setCursor(last_x + ((x - last_x) / 2) - ((strlen(str) * font_width) / 2 ), y + (trace_height / 2) - (font_height / 2));
                        writeString(str);                    

                    }
                    last_sample = 1;
                    last_x = x;
                }
                
                last_sample = 1;
            } else {
                // no need to draw yet
                if (x < 640) {
                    drawPixel(x, y + trace_height - 1, line_col);
                }
                if (last_sample == 0) {
                    // no need to draw yet
                    // drawPixel(x, y + 29, line_col);
                } else {
                    if (x < 640) {
                        drawVLine(x, y, trace_height, line_col);
                        //drawHLine(last_x, y, x - last_x, line_col);
                    }
                    
                    if (x) {

                        // if ((x) && (last_x < 640)){
                        sprintf(str,"%d", x - last_x);
                        // setCursor(last_x + 10, y + (trace_height % 2) + 2);
                        setCursor(last_x + ((x - last_x) / 2) - ((strlen(str) * font_width) / 2 ), y + (trace_height / 2) - (font_height / 2));
                        writeString(str);
                    }
                    last_sample = 0;
                    last_x = x;
                }


            }
            x++;
            /*
            if (x >= 640) {
                break;
            }
            */
        }

        /*
        if (last_sample) {
            drawHLine(last_x, y, x - last_x, line_col);
        } else {
            drawHLine(last_x, y + trace_height - 1, x - last_x, line_col);
        }
        */

        line_col = ORANGE;

        y += trace_height + y_padding;
        //trace_height += 10; // 

        //uart_puts(UART_ID, "\n");
    }
}








int main() {

    // Initialize stdio
    stdio_init_all();

    printf("Initialising VGA...");


    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // Initialize the VGA screen
    initVGA() ;

    uart_puts(UART_ID, "\nPress SPACE to play/pause animation.\n");
    uart_puts(UART_ID, "Press 'c' to capture.\n");


    // We're going to capture into a u32 buffer, for best DMA efficiency. Need
    // to be careful of rounding in case the number of pins being sampled
    // isn't a power of 2.
    uint total_sample_bits = CAPTURE_N_SAMPLES * CAPTURE_PIN_COUNT;
    total_sample_bits += bits_packed_per_word(CAPTURE_PIN_COUNT) - 1;
    uint buf_size_words = total_sample_bits / bits_packed_per_word(CAPTURE_PIN_COUNT);
    uint32_t *capture_buf = malloc(buf_size_words * sizeof(uint32_t));
    hard_assert(capture_buf);

    // Grant high bus priority to the DMA, so it can shove the processors out
    // of the way. This should only be needed if you are pushing things up to
    // >16bits/clk here, i.e. if you need to saturate the bus completely.
    
    // todo - uncomment this next line  64CAPTURE_PIN_BASE
    // bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    PIO pio = pio0;
    uint sm = 3;
    uint dma_chan = dma_claim_unused_channel(true);

    // logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 125000000 / (115200 * 4) /*271.267*/);
    logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_SAMPLE_FREQ_DIVISOR);

    // animation pause
    bool pause = true;

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
//    drawVLine(2, 0, 16, WHITE);

//    drawVLine(9, 0, 16, WHITE);
//    drawVLine(11, 0, 16, WHITE);

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

    // Wait for the pios to get warmed up. Probably not necessary.
    sleep_ms(10);

    uart_puts(UART_ID, "Arming trigger...\n");
    logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, CAPTURE_TRIGGER_PIN, false);
    dma_channel_wait_for_finish_blocking(dma_chan);

    // print_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);
    plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);

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
            } else if ((char)ch == 'c') {

                // printf("Arming trigger (Ctrl-C to exit)\n");

                uart_puts(UART_ID, "Arming trigger...\n");
                logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, CAPTURE_TRIGGER_PIN, false);

                // each bit on a uart travels at 115200 bits per second
                // the clock goes at 125,000,000 hz (I think)

                // so if we want to take, say, 4 samples per bit then we need to sample at  4 * 115200 = 460800 bps
                // 125,000,000 / 460,800 = 271.267

                // The logic analyser should have started capturing as soon as it saw the
                // first transition. Wait until the last sample comes in from the DMA.
                dma_channel_wait_for_finish_blocking(dma_chan);

                // print_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);
                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);

                        /* code */
            }
            
        }
        

        // A brief nap
        sleep_ms(10) ;

        // uart_putc(UART_ID, 'B');

   }

}