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

// #include "hardware/timer.h"
// #include "hardware/structs/bus_ctrl.h"
#include <string.h>

#define UART_ID uart1
#define BAUD_RATE 115200


// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 4
#define UART_RX_PIN 5

uint8_t last_uart_char = 0;

// Some globals for storing timer information
volatile unsigned int time_accum = 0;
unsigned int time_accum_old = 0 ;
char timetext[40];


int g_mag = 0;
int g_scrollx = 0;

// Timer interrupt
bool repeating_timer_callback(struct repeating_timer *t) {

    time_accum += 1 ;
    return true;
}

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

const uint CAPTURE_PIN_BASE = HSYNC2; // 16 = hsync, 17 = vsync // 22 = hsync2
const uint CAPTURE_PIN_COUNT = 4;
const uint CAPTURE_TRIGGER_PIN = VSYNC2; // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2 NB IGNORED FOR NOW!
const uint CAPTURE_N_SAMPLES = SCREEN_WIDTH * 32; // enough for 32 screen width's worth of data
const uint CAPTURE_SAMPLE_FREQ_DIVISOR = 5 * 4 * 1; /*271.267*/ // was 5 * 4

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
    pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(!trigger_level, CAPTURE_PIN_BASE + 1));
    
    // level trigger
    pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(trigger_level, CAPTURE_PIN_BASE + 1));
    
    // level trigger
    pio_sm_exec(pio, sm, pio_encode_wait_gpio(1, LO_GRN));

    for (int i = 0; i < (513 /*+ 10*/ /*+ 45*/); i++) {
    // for (int i = 0; i < (513 + 45 /*+ 10*/ /*+ 45*/); i++) {
        pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(!trigger_level, CAPTURE_PIN_BASE));
        pio_sm_exec_wait_blocking(pio, sm, pio_encode_wait_gpio(trigger_level, CAPTURE_PIN_BASE));
    }

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


int inverse_mag_factor(int value) {
    if (g_mag < 0) {
        return value / (abs(g_mag) + 1);
    } else {
        return value * (g_mag + 1);
    }
}


int mag_factor(int value) {
    if (g_mag < 0) {
        return value * (abs(g_mag) + 1);
    } else {
        return value / (g_mag + 1);
    }
}

void uart_putcf(uart_inst_t *uart, const char *s, int c) {
    char str[80];  
    sprintf(str, s, c);
    uart_puts(uart, str);
}

void plot_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples, int magnification, int scrollx) {
    // Display the capture buffer in graphical form, like this:
    // 00: __--__--__--__--__--__--
    // 01: ____----____----____----
    // ...only with lines
    // uart_puts(UART_ID, "Capture:\n");
    // Each FIFO record may be only partially filled with bits, depending on
    // whether pin_count is a factor of 32.


    // Plot colours:  HSYNC, VSYNC, LO_GREEN, HI_GREEN, BLUE & RED
    char colours[] = {YELLOW, ORANGE, DARK_GREEN, GREEN, BLUE, RED};

    uint record_size_bits = bits_packed_per_word(pin_count); 
    int trace_height = 40;
    int y_padding = 4;
    int y = 51;

    char str[80];

    setTextSize(1);

    fillRect(0, y, SCREEN_WIDTH, (((trace_height + y_padding) * CAPTURE_PIN_COUNT) + y_padding), BLACK); // clear plot area
        
    y += y_padding;

    char font_width = 6;
    char font_height = 8;


    for (int pin = 0; pin < pin_count; ++pin) {
        int x = 0;

        uint magIndex = 0;

        char line_col = colours[pin];

        int last_sample = 1;
        int last_x = 0;

        // fillRect(0, y, 640, trace_height, BLACK); // colour boxes
        setTextColor(line_col);
        
        last_sample = 1; // fix at high for now
        last_x = 0;

        int last_i = 0;
        int cursor_x = 0;
        
        // uart_puts(UART_ID, "%02d: ", pin + pin_base);
        // uart_puts(UART_ID, "todo");
        int i;
        
        for (i = scrollx; i >= 0; i--) {
            // Go back from the current scroll position and find the first
            // sample that's different from the sample at the scroll position.
            uint bit_index = pin + i * pin_count;
            uint word_index = bit_index / record_size_bits;
            // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            
            uint sample = buf[word_index] & word_mask ? 1 :0;

            if (i == scrollx) {
                last_sample = sample;
            }

            if (last_sample != sample) {
                last_i = i + 1;
                if (last_i == scrollx) {
                    // the sample at scrollx - 1 is the inverse of the scrollx 
                    // so it needs inverting in order to display the transition

                    // this seems to work for anything other than mag = 0
                    last_sample = !last_sample;
                }
                break;
            }
             
            if (magnification < 0) {
                if (++magIndex >= abs(magnification) + 1) {
                    x--;
                    magIndex = 0;
                } 
            } else {
                x-= magnification + 1;
            }
        }

        last_x = x + 1;


        int last_pixel_x = -1;
        int last_v_line_x = -1;

        x = 0;

        char debug1 = 0;

        // for (int i = scrollx; i < n_samples; ++i) {

        // int max_screen_x = scrollx + mag_factor(640);
        // if (max_screen_x > n_samples) {
        //     max_screen_x = n_samples;
        // }

        // int max_screen_x = MIN(scrollx + mag_factor(640), n_samples);
        int max_screen_x = MIN(scrollx + mag_factor(SCREEN_WIDTH), n_samples);
        // if ((pin == 0)) {
        //     uart_putcf(UART_ID, "scrollx: %d", scrollx); // 
        //     uart_putcf(UART_ID, "max_screen_x: %d", max_screen_x); // 
        // }

        for (int i = scrollx; i < max_screen_x; i++) {

            //

            uint bit_index = pin + i * pin_count;
            uint word_index = bit_index / record_size_bits;
            // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            //uart_puts(UART_ID, buf[word_index] & word_mask ? "-" : "_");
            
            uint sample = buf[word_index] & word_mask ? 1 :0;
            
            if (x != last_pixel_x) {
                drawPixel(x, y + (sample ? 0 : trace_height - 1), line_col);
                last_pixel_x = x;
            }

            if (last_sample != sample) {
                // if (x < 640) {
                    if (x != last_v_line_x) {
                        drawVLine(x, y, trace_height, line_col);
                        last_v_line_x = x;
                    }                    
                // } else {
                    // not quite sure why we get here, but we do just twice when the mag is < 0 - todo
                    // if (pin == 0) {
                    //     uart_putcf(UART_ID, "x: %d ", x);
                    // } 
                // }
                sprintf(str,"%d", i - last_i);
                int str_width = strlen(str) * font_width;


                // if ((pin == 0) && (i - last_i == 25)) uart_putc(UART_ID, '~');


                if (str_width < (x - last_x)) { // todo - undo this line - don't delet
                
                    cursor_x = (last_x + ((x - last_x) / 2)) - (str_width / 2);
                    if (cursor_x < 0) {
                        cursor_x = 0;
                        if (cursor_x + str_width >= x) {
                            cursor_x = x - str_width - 1;
                        }
                    }
                    // cursor_x = (last_i + ((i - last_i) / 2)) - (str_width / 2) - scrollx;
                    // if ((cursor_x >= 0) && (cursor_x - str_width < 640)) {
                    if ((cursor_x - str_width < SCREEN_WIDTH)) {
                        setCursor(cursor_x, y + (trace_height / 2) - (font_height / 2));
                        writeString(str);
                    }
                }
                last_sample = sample;
                last_x = x;
                last_i = i;
            }
            if (magnification < 0) {
                if (++magIndex >= abs(magnification) + 1) {
                    x++;
                    magIndex = 0;
                } 
            } else {
                x+= magnification + 1;
            }
        }

        for (int i = max_screen_x; i < n_samples; i++) {

            uint bit_index = pin + i * pin_count;
            uint word_index = bit_index / record_size_bits;
            // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
            uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
            //uart_puts(UART_ID, buf[word_index] & word_mask ? "-" : "_");
            
            uint sample = buf[word_index] & word_mask ? 1 :0;
            
            // if (x != last_pixel_x) {
                // drawPixel(x, y + (sample ? 0 : trace_height - 1), line_col);
                // last_pixel_x = x;
            // }

            if (last_sample != sample) {


                // uart_putc(UART_ID, '$');


                // if (x < 640) {
                    // if (x != last_v_line_x) {
                        // drawVLine(x, y, trace_height, line_col);
                        // last_v_line_x = x;
                    // }                    
                // } else {
                    // uart_putc(UART_ID, '?');
                // }
                sprintf(str,"%d", i - last_i);
                int str_width = strlen(str) * font_width;
                if (str_width < (x - last_x)) {

                    // if ((pin == 0) && (i - last_i == 25)) {
                    //     // uart_putc(UART_ID, '=');
                    //     uart_putcf(UART_ID, "last_i: %d", last_i); // last_i: 1394 
                    //     uart_putcf(UART_ID, " i: %d", i); // i: 1419 - it does indeed == 25

                    // }

                    // if ((pin == 0) && (sample == 1) && (i - last_i != 24)) {
                    //     uart_putcf(UART_ID, " i - li: %d", i - last_i); // i: 1419 - it does indeed == 25
                    // }

                    // uart_putc(UART_ID, '$');
                    cursor_x = (last_x + ((x - last_x) / 2)) - (str_width / 2);
                    if (cursor_x + str_width >= SCREEN_WIDTH) {
                        // some of the string is off screen and to the right
                        // if (pin == 0) uart_putc(UART_ID, '$');
                        cursor_x = last_x + 3;
                        if (cursor_x + str_width < SCREEN_WIDTH) {
                            // if (pin == 0) uart_putc(UART_ID, '%');
                        // if (pin == 0) uart_putc(UART_ID, '*');
                        // cursor_x = 640 - str_width - 1;
                            cursor_x = SCREEN_WIDTH - str_width - 1;

                        // if (cursor_x + str_width >= x) {
                            // uart_putc(UART_ID, '*');
                            // cursor_x = x - str_width - 1;
                            // setCursor(cursor_x, y + (trace_height / 2) - (font_height / 2));
                            // writeString(str);
                        // } else {
                            // uart_putc(UART_ID, '+');
                            // cursor_x = last_x + 3;
                        }

                    } else if (cursor_x < 0) {
                        cursor_x = 0;
                    }
                    setCursor(cursor_x, y + (trace_height / 2) - (font_height / 2));
                    writeString(str);

                    // }
                    // cursor_x = (last_i + ((i - last_i) / 2)) - (str_width / 2) - scrollx;
                    // if ((cursor_x >= 0) && (cursor_x - str_width < 640)) {
                    // if ((cursor_x - str_width  640)) {
                        // setCursor(cursor_x, y + (trace_height / 2) - (font_height / 2));
                        // writeString(str);
                    // }

                }

                break;

                // last_sample = sample;
                // last_x = x;
                // last_i = i;
            }
            if (magnification < 0) {
                if (++magIndex >= abs(magnification) + 1) {
                    x++;
                    magIndex = 0;
                } 
            } else {
                x+= magnification + 1;
            }
        }



        y += trace_height + y_padding;
    }
}

/*
/// \tag::get_time[]
// Simplest form of getting 64 bit time from the timer.
// It isn't safe when called from 2 cores because of the latching
// so isn't implemented this way in the sdk
static uint64_t get_time(void) {
    // Reading low latches the high value
    uint32_t lo = timer_hw->timelr;
    uint32_t hi = timer_hw->timehr;
    return ((uint64_t) hi << 32u) | lo;
}
/// \end::get_time[]


bool my_uart_is_readable_within_us(uart_inst_t * uart, uint32_t us) {
    bool r = uart_is_readable(uart);
    if (!r) {
        uint32_t start = time_us_32();
        while (time_us_32() - start < us) {
            r = uart_is_readable(uart);
            if (r) {
                break;
            }
        }
    }
    return r;
}

*/


    // plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);



int measure(const uint32_t *buf) {
    // currently measures the time between hsyng going high and rgb going high on the first line of the VGA frame
    uart_puts(UART_ID, "\nMeasure something...\n");


    char pin = 2;
    uint record_size_bits = bits_packed_per_word(CAPTURE_PIN_COUNT);

    int i;

    int p2;
    int p0;

    for (i = 0; i < CAPTURE_N_SAMPLES; i++) {
        uint bit_index = pin + i * CAPTURE_PIN_COUNT;
        uint word_index = bit_index / record_size_bits;
        // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
        uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
        
        uint sample = buf[word_index] & word_mask ? 1 :0;
        
        if (sample) {
            p2 = i;
            uart_putcf(UART_ID, "first pin 2 high: %d\n", p2);
            break;
        }
        
    }

    pin = 0;

    for (i = i; i >= 0; i--) {
        uint bit_index = pin + i * CAPTURE_PIN_COUNT;
        uint word_index = bit_index / record_size_bits;
        // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
        uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
        
        uint sample = buf[word_index] & word_mask ? 1 :0;
        
        if (!sample) {
            p0 = i; // this sample is still high (we're going backwards)
            uart_putcf(UART_ID, "previous pin 0 low: %d\n", p0);
            break;
        }
        
    }

    uart_putcf(UART_ID, "p0..p2: %d\n", p2 - (p0 + 1));

    return p0 + 1;

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
    
    // todo - uncomment this next line
    // bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    PIO pio = pio1;
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

    drawHLine(0, SCREEN_HEIGHT - 1, 16, WHITE);
    drawVLine(0, SCREEN_HEIGHT - 16, 16, WHITE);

    drawHLine(SCREEN_WIDTH - 16, 0, 16, WHITE);
    drawVLine(639, 0, 16, WHITE);

    drawVLine(639, SCREEN_HEIGHT - 16, 16, WHITE);
    drawHLine(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 1, 16, WHITE);

    // drawHLine(630, 480 - 1, 1, WHITE);


    for (int i = 0; i < 16; i++){
        drawPixel(15 - i, 0, i); // test all colours in first horizontal line following sync
    }

    // drawPixel(0, 0, WHITE);

    // drawHLine(0, 0, 32, WHITE);

/*
    drawPixel(1, 0, WHITE);
    drawPixel(2, 0, 1);
    drawPixel(3, 0, 1);
    drawPixel(4, 0, 2);
    drawPixel(5, 0, 2);
*/


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
    plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);


    // fillRect(0, 0, 640, 480, WHITE); // green box

    while(true) {

        if (!pause) {

            // Modify the color chooser
            if (color_index ++ == 15) color_index = 0 ;

            // A row of filled circles
            fillCircle(disc_x, 100, 20, color_index);
            disc_x += 35 ;
            if (disc_x > SCREEN_WIDTH) disc_x = 0;

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

        }
        // Timing text
        if (time_accum != time_accum_old) {
            setTextColor(WHITE) ;
            time_accum_old = time_accum ;
            fillRect(250, 20, 176, 30, RED); // red box
            sprintf(timetext, "%d", time_accum) ;
            setCursor(250, 20) ;
            setTextSize(2) ;
            writeString(timetext) ;
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

        if ((last_uart_char == 27) || uart_is_readable(UART_ID)) {
            
            fillRect(1, SCREEN_HEIGHT - 1 - 8, SCREEN_WIDTH - 2, 8, BLUE);
            setCursor(2, SCREEN_HEIGHT - 1 - 8);
            setTextColor(WHITE);
            setTextSize(1);

            uint8_t ch = last_uart_char;

            if (ch != 27) {
                ch = uart_getc(UART_ID);
                last_uart_char = ch;
            }
            
            #define ESCAPE_SEQ_LEN 80
            uint8_t escape_seq[ESCAPE_SEQ_LEN + 1];
            escape_seq[0] = 0;

            char str[80];
            sprintf(str,"%d ", ch);
            writeString(str);
            if (uart_is_writable(UART_ID)) {
                uart_putc(UART_ID, ch);
            }


            if (ch == 27 /* ESC */) {
                ch = 0;
                uint8_t noofchars = 0;
            
                // Wait up to 2 byte times, which is (2 * 10 * 1,000,000) / 115,200 = 1737 uS.
                // This had me confused for ages. It seemed to make no difference whether
                // the "us" parameter was 1 or 1800. Turns out that pressing the Esc
                // key in minicom (serial terminal) has a little delay before it is transmitted
                // unlike any other key. Thank goodness for LEDs on the debug probe.
                while (uart_is_readable_within_us(UART_ID, 1800)) {                    
                    ch = uart_getc(UART_ID);
                    last_uart_char = ch;

                    if (ch == 27 /* ESC */) {
                        break;
                    
                    } else {   
                
                        // possibly build up string here for testing later?
                        if (noofchars < ESCAPE_SEQ_LEN){
                            escape_seq[noofchars] = ch;
                            escape_seq[noofchars + 1] = 0;
                        }
                        noofchars += 1;
                        
                        sprintf(str,"%d ", ch);
                        writeString(str);
                        if (uart_is_writable(UART_ID)) {
                            uart_putc(UART_ID, ch);
                        }
                    }
                }
                
                if (strlen(escape_seq) == 0) {
                    // Must have been caused by a single Esc key press
                    writeString("esc");
                    last_uart_char = 0;
                } else {
                    writeString(escape_seq);
                    writeString(" ");
                }
                
                if (strlen(escape_seq) == 2) {
                    if ((char)escape_seq[0] == '[') {
                        switch ((char)escape_seq[1]) {
                            case 'A':
                                writeString("up");
                                break;
                            case 'B':
                                writeString("down");
                                break;
                            case 'C':
                                writeString("right");
                                // uart_putcf(UART_ID,"gs:%d ", g_scrollx)
                                // g_scrollx += mag_factor(1); // 10% of the screen width
                                g_scrollx += 1;
                                // uart_putcf(UART_ID,"gs:%d ", g_scrollx);                              
                                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                                break;
                            case 'D':
                                writeString("left");
                                if (g_scrollx > 0) {
                                    // g_scrollx -= mag_factor(1); // 10% of the screen width                            
                                    g_scrollx -= 1;
                                    if (g_scrollx < 0) {
                                        g_scrollx = 0;
                                    }
                                    plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                                }
                                break;
                            default:
                                break;
                        }
                    } else if ((char)escape_seq[0] == 'O') {
                        if ((char)escape_seq[1] == 'F') {
                            writeString("end");
                            // it was crashing here for some reason
                            //writeString(" debug");
                            
                            g_scrollx = CAPTURE_N_SAMPLES - (mag_factor(SCREEN_WIDTH));
                            if (g_scrollx < 0) {
                                g_scrollx = 0;
                            }

                            plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                            
                        }
                    }
                } else if (strlen(escape_seq) == 3) {
                    if ((char)escape_seq[2] == '~') {
                        if ((char)escape_seq[1] == '1') {
                            writeString("home");
                            g_scrollx = 0;
                            plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                        
                        } else if ((char)escape_seq[1] == '5') {
                            writeString("page up");
                        } else if ((char)escape_seq[1] == '6') {
                            writeString("page down");
                        }
                    }
                } else if (strlen(escape_seq) == 5) {
                    if (strcmp(escape_seq, "[1;5D") == 0) {
                            writeString("ctrl-left");
                            if (g_scrollx > 0) {
                                g_scrollx -= mag_factor(SCREEN_WIDTH); // 100% of the screen width
                                if (g_scrollx < 0) {
                                    g_scrollx = 0;
                                }
                                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                            }



                    } else if (strcmp(escape_seq, "[1;5C") == 0) {
                            writeString("ctrl-right");
                            g_scrollx += mag_factor(SCREEN_WIDTH); // 100% of the screen width                                
                            plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
                    }
                }

            } else if ((char)ch == ' ') {
                pause = !pause; // toggle pause

            } else if (((char)ch == '-') || ((char)ch == '+') || ((char)ch == '=')) {
                if ((char)ch == '-') {
                    writeString("zoom out");
                    g_mag-= 1;
                } else if ((char)ch == '+' ) {
                    writeString("zoom in");
                    g_mag+= 1;
                } else {
                    writeString("no zoom");
                    g_mag = 0;
                }

                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);

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
                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);

            } else if ((char)ch == 'z') {
                // zoom in or out to fill the screen
                writeString("zoom to fill");

                if (CAPTURE_N_SAMPLES >= SCREEN_WIDTH) {
                    // we need to zoom out
                    int factor = CAPTURE_N_SAMPLES / SCREEN_WIDTH;
                    g_mag = -(factor - 1);

                } else {
                    // we need to zoom in
                    int factor = SCREEN_WIDTH / CAPTURE_N_SAMPLES;
                    g_mag = (factor - 1);
                }
                g_scrollx = 0;
                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
            
            } else if ((char)ch == 'm') {
                // measure
                writeString("measure ");
                g_scrollx = measure(capture_buf);
                // g_scrollx = 900;
                g_mag = 0;
                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES, g_mag, g_scrollx);
            }
            
        }
        

        // A brief nap
        sleep_ms(10) ;

        // uart_putc(UART_ID, 'B');

   }

}