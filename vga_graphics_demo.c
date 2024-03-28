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


// We are using pins 4 and 5, but see the GPIO function select table in the
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
uint8_t g_channel;


enum SETTINGS_STATES {SS_CHANNEL, SS_ZOOM, SS_FREQ, SS_NO_OF_PINS, SS_PINS_BASE, SS_TRIGGER_PIN_BASE, SS_TRIGGER_TYPE, SS_COUNT};

uint8_t settings_state = SS_CHANNEL;

// Timer interrupt
bool repeating_timer_callback(struct repeating_timer *t) {

    time_accum += 1 ;
    return true;
}

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

const uint CAPTURE_PIN_BASE = HSYNC; // 16 = hsync, 17 = vsync // 22 = hsync2
const uint CAPTURE_PIN_COUNT = 4;
const uint CAPTURE_TRIGGER_PIN = VSYNC; // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2 NB IGNORED FOR NOW!
const uint CAPTURE_N_SAMPLES = SCREEN_WIDTH * 96; // enough for 48 screen width's worth of data
const uint CAPTURE_SAMPLE_FREQ_DIVISOR = 5 * 4 * 1; /*271.267*/ // was 5 * 4

uint g_sample_frequency = CAPTURE_SAMPLE_FREQ_DIVISOR;
uint8_t g_no_of_captured_pins = CAPTURE_PIN_COUNT;
uint8_t g_pins_base = CAPTURE_PIN_BASE;
uint g_capture_n_samples = CAPTURE_N_SAMPLES;

uint8_t g_no_of_pins_to_capture = CAPTURE_PIN_COUNT;

uint8_t g_trigger_pin_base = HSYNC;

enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE, TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_COUNT};

uint8_t g_trigger_type = TT_VGA_VSYNC;


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
    static uint16_t capture_prog_instr;
    static struct pio_program capture_prog;
    static uint offset;

    if (capture_prog_instr) {
        // We need to re-initialise, presumably to change the pin base, the pin count,
        // or the frequency divisor, so I believe we need to remove the old program first.
        pio_remove_program(pio, &capture_prog, offset);
    // } else {
        // This should be the first time we've called this, as capture_prog_instr is
        // a static variable, and static variables are zeroed on reset, which means
        // we don't need to do anything here, so let's comment it out.
    }

    capture_prog_instr = pio_encode_in(pio_pins, pin_count);

    capture_prog.instructions = &capture_prog_instr;
    capture_prog.length = 1;
    capture_prog.origin = -1;

    offset = pio_add_program(pio, &capture_prog);

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


// Immediately execute an instruction on a state machine and wait for up to
// a number of microseconds for it to complete.
bool pio_sm_exec_timeout_us(PIO pio, uint sm, uint instr, uint32_t timeout_us) {
    pio_sm_exec(pio, sm, instr);
    uint32_t start_time = time_us_32();
    while (time_us_32() - start_time < timeout_us) {
        if (!pio_sm_is_exec_stalled(pio, sm)) {
            return true;
        }
    }
    return false;
}


// Immediately execute an instruction on a state machine and wait for it to
// complete unless an expiry time is reached sooner.
bool pio_sm_exec_expiry_time_us(PIO pio, uint sm, uint instr, uint32_t expiry_time_us) {
    pio_sm_exec(pio, sm, instr);
    // uint32_t start_time = time_us_32();
    while (time_us_32() < expiry_time_us) {
        if (!pio_sm_is_exec_stalled(pio, sm)) {
            return true;
        }
    }
    return false;
}


bool logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, uint8_t trigger_type) {

    uart_puts(UART_ID, "\nArming trigger...\n");

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

    #define TRIGGER_TIMEOUT_US 2000000 // 2 seconds

    // Wait for a rising edge on VSYNC, which involves:
    // 1. Waiting until VSYNC is at the opposite level of the trigger level.

    bool triggered = false; // assume fail


// enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE, TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_COUNT};

    switch (trigger_type) {

        case TT_NONE:
            triggered = true;
            break;

        case TT_LOW_LEVEL:
            triggered = pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US);
            break;

        case TT_HIGH_LEVEL:
            triggered = pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US);
            break;

        case TT_RISING_EDGE:
            if (pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US)) {
                triggered = pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US);
            }
            break;

        case TT_FALLING_EDGE:
            if (pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US)) {
                triggered = pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US);
            }
            break;

        case TT_ANY_EDGE:
            // read the state of the trigger pin
            bool trigger_pin_state = gpio_get(trigger_pin);

            // wait for it to change to the opposite state
            triggered = pio_sm_exec_timeout_us(pio, sm, pio_encode_wait_gpio(!trigger_pin_state, trigger_pin), TRIGGER_TIMEOUT_US);
            break;


        case TT_VGA_VSYNC:
        case TT_VGA_RGB:
        case TT_VGA_VFRONT_PORCH:
            int x = 1; // number of pulses 
            switch (trigger_type) {
                case TT_VGA_VSYNC:
                    x = 524; // capture just before the next VSYNC pulse
                    break;
                case TT_VGA_RGB:
                    x = 34; // capture just before the active phase
                    break;
                case TT_VGA_VFRONT_PORCH:
                    x = 480 + 33 + 2 - 1; // capture just before the vertical front porch phase
                    break;
            }


            uint32_t expiry_time = time_us_32() + TRIGGER_TIMEOUT_US;

            if (pio_sm_exec_expiry_time_us(pio, sm, pio_encode_wait_gpio(1, trigger_pin + 1), expiry_time)) {
                // VSYNC is high
                if (pio_sm_exec_expiry_time_us(pio, sm, pio_encode_wait_gpio(0, trigger_pin + 1), expiry_time)) {
                    // VSYNC is low
                    triggered = true; // assume pass
                    // Wait for x HSYNC pulses before starting the capture
                    for (int i = 0; i < x; i++) {
                        if (pio_sm_exec_expiry_time_us(pio, sm, pio_encode_wait_gpio(1, trigger_pin), expiry_time)) {
                            if (!pio_sm_exec_expiry_time_us(pio, sm, pio_encode_wait_gpio(0, trigger_pin), expiry_time)) {
                                triggered = false;
                                break;
                            }
                        } else {
                            triggered = false;
                            break;
                        }
                    }
                } else {
                    // uart_puts(UART_ID, "trigger 2 failed\n");
                }
            } else {
                // uart_puts(UART_ID, "trigger 1 failed\n");
            }
            break;
    }

    if (!triggered) {
        // Failed to trigger, so restart the state machine otherwise it'll be stuck
        // in a latched EXEC instruction, and we'll never fill up the capture buffer.
        uart_puts(UART_ID, "failed to trigger, restarting the state machine...\n");
        pio_sm_restart(pio, sm);
    }

    // Let the PIO take over from here
    pio_sm_set_enabled(pio, sm, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    pio_sm_set_enabled(pio, sm, false); // Disable the state machine, which might save a bit of power? (todo - find out)

    g_no_of_captured_pins = g_no_of_pins_to_capture;

    return triggered;
}


int get_channel_sample(const uint32_t *buf, int pin, uint pin_count, int index, uint record_size_bits) {

    uint bit_index = pin + index * pin_count;
    uint word_index = bit_index / record_size_bits;
    // Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
    uint word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);
    //uart_puts(UART_ID, buf[word_index] & word_mask ? "-" : "_");

    return buf[word_index] & word_mask ? 1 :0;
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
            uart_puts(UART_ID, get_channel_sample(buf, pin, pin_count, sample, record_size_bits) ? "-" : "_");
        }
        uart_puts(UART_ID, "\n");
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


// writes a formatted integer to the VGA framebuffer
void write_intf(const char *s, int c) {
    char str[80];
    sprintf(str, s, c);
    writeString(str);
}


#define FONT_WIDTH 6
#define FONT_HEIGHT 8

#define TOOLBAR_LEFT 1
#define TOOLBAR_WIDTH SCREEN_WIDTH - (2 * TOOLBAR_LEFT)
#define TOOLBAR_HEIGHT 10
// #define TOOLBAR_TOP SCREEN_HEIGHT - TOOLBAR_HEIGHT - 1
#define TOOLBAR_TOP 55
#define TOOLBAR_TEXT_PADDING 1

#define TOOLBAR_COLOR DARK_BLUE
#define TOOLBAR_ITEM_PADDING 0

// #define TOOLBAR_HINT_WIDTH (TOOLBAR_WIDTH) / 3
 #define TOOLBAR_HINT_WIDTH 100


// #define CHANNEL_NO_LEFT (SCREEN_WIDTH / 2)

#define CHANNEL_NO_LEFT TOOLBAR_LEFT + TOOLBAR_TEXT_PADDING + TOOLBAR_HINT_WIDTH + TOOLBAR_TEXT_PADDING

#define CHANNEL_NO_WIDTH (FONT_WIDTH * 6) + TOOLBAR_ITEM_PADDING

#define MAG_LEFT CHANNEL_NO_LEFT + CHANNEL_NO_WIDTH + TOOLBAR_ITEM_PADDING
#define MAG_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING


#define FREQ_LEFT MAG_LEFT + MAG_WIDTH + TOOLBAR_ITEM_PADDING
#define FREQ_WIDTH (FONT_WIDTH * 10) + TOOLBAR_ITEM_PADDING

#define NO_OF_PINS_LEFT FREQ_LEFT + FREQ_WIDTH + TOOLBAR_ITEM_PADDING
#define NO_OF_PINS_WIDTH (FONT_WIDTH * 8) + TOOLBAR_ITEM_PADDING

#define PINS_BASE_LEFT NO_OF_PINS_LEFT + NO_OF_PINS_WIDTH + TOOLBAR_ITEM_PADDING
#define PINS_BASE_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING

#define TRIGGER_BASE_LEFT PINS_BASE_LEFT + PINS_BASE_WIDTH + TOOLBAR_ITEM_PADDING
#define TRIGGER_BASE_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING

#define TRIGGER_TYPE_LEFT TRIGGER_BASE_LEFT + TRIGGER_BASE_WIDTH + TOOLBAR_ITEM_PADDING
#define TRIGGER_TYPE_WIDTH (FONT_WIDTH * 13) + TOOLBAR_ITEM_PADDING

#define PLOT_TOP 75
#define PLOT_HEIGHT 36
#define PLOT_PADDING 4

// #define MINIMAP_BOTTOM TOOLBAR_TOP - 4

// #define STATUSBAR_LEFT 1
// #define STATUSBAR_WIDTH SCREEN_WIDTH - (2 * STATUSBAR_LEFT)
// #define STATUSBAR_HEIGHT 10
// // #define TOOLBAR_TOP SCREEN_HEIGHT - TOOLBAR_HEIGHT - 1
// #define STATUSBAR_TOP SCREEN_HEIGHT - TOOLBAR_HEIGHT - 1
// #define STATUSBAR_TEXT_PADDING 1

// #define MINIMAP_TOP 420
#define MINIMAP_HEIGHT 6
#define MINIMAP_PADDING 1
#define MINIMAP_BOTTOM STATUSBAR_TOP - 4


#define STATUSBAR_HEIGHT 10
#define STATUSBAR_TOP SCREEN_HEIGHT - STATUSBAR_HEIGHT - 1
#define STATUSBAR_LEFT 1
#define STATUSBAR_WIDTH SCREEN_WIDTH - (2 * STATUSBAR_LEFT)

// #define STATUSBAR_TOP 55
#define STATUSBAR_TEXT_PADDING 1
#define STATUSBAR_COLOR DARK_BLUE
#define STATUSBAR_ITEM_PADDING 0

// #define STATUSBAR_HINT_WIDTH (TOOLBAR_WIDTH) / 3
#define STATUSBAR_HINT_WIDTH TOOLBAR_WIDTH


void plot_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples, int magnification,
                        int scrollx, bool show_numbers) {
    // Display the capture buffer in graphical form, like this:
    // 00: __--__--__--__--__--__--
    // 01: ____----____----____----
    // ...only with lines
    // uart_puts(UART_ID, "Capture:\n");
    // Each FIFO record may be only partially filled with bits, depending on
    // whether pin_count is a factor of 32.


    // Plot colours:  HSYNC, VSYNC, LO_GREEN, HI_GREEN, BLUE & RED
    char colours[] = {YELLOW, ORANGE, MED_GREEN, GREEN, BLUE, RED, MAGENTA, CYAN};

    uint record_size_bits = bits_packed_per_word(pin_count);

    int trace_height = PLOT_HEIGHT;
    int y_padding = PLOT_PADDING;
    int y = PLOT_TOP;

    int max_screen_x = MIN(scrollx + mag_factor(SCREEN_WIDTH), n_samples);


    if (!show_numbers) {
        trace_height = MINIMAP_HEIGHT;
        y_padding = MINIMAP_PADDING;
        // y = MINIMAP_TOP;
        y = MINIMAP_BOTTOM - ((MINIMAP_HEIGHT + MINIMAP_PADDING) * pin_count);

        if (g_capture_n_samples >= SCREEN_WIDTH) {
            // we need to zoom out
            int factor = g_capture_n_samples / SCREEN_WIDTH;
            magnification = -(factor - 1);
        } else {
            // we need to zoom in
            int factor = SCREEN_WIDTH / g_capture_n_samples;
            magnification = (factor - 1);
        }

        scrollx = 0;
        max_screen_x = n_samples;
    }





    char str[80];

    setTextSize(1);

    // fillRect(0, y, SCREEN_WIDTH, (((trace_height + y_padding) * g_no_of_captured_pins) + y_padding), BLACK); // clear plot area

    // y += y_padding;

    for (int pin = 0; pin < pin_count; ++pin) {

        fillRect(0, y, SCREEN_WIDTH, (trace_height), BLACK); // clear plot area


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

            uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);

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
        // int max_screen_x = MIN(scrollx + mag_factor(SCREEN_WIDTH), n_samples);
        // if ((pin == 0)) {
        //     uart_putcf(UART_ID, "scrollx: %d", scrollx); //
        //     uart_putcf(UART_ID, "max_screen_x: %d", max_screen_x); //
        // }

        for (int i = scrollx; i < max_screen_x; i++) {

            uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);

            // Check to see whether we've drawn a pixel at this x location, which only
            // happens when we're zoomed out (zoom < 0). If not draw it.

            if (x != last_pixel_x) {
                //
                drawPixel(x, y + (sample ? 0 : trace_height - 1), line_col);
                last_pixel_x = x;
            }

            // Check to see whether we've vertical line a pixel at this x location, which only
            // happens when we're zoomed out (zoom < 0). If not draw it.

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
                if (show_numbers) {
                    sprintf(str,"%d", i - last_i);
                    int str_width = strlen(str) * FONT_WIDTH;


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
                        if (cursor_x - str_width < SCREEN_WIDTH) {
                            setCursor(cursor_x, y + (trace_height / 2) - (FONT_HEIGHT / 2));
                            writeString(str);
                        }
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

            uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);

            // if (x != last_pixel_x) {
                // drawPixel(x, y + (sample ? 0 : trace_height - 1), line_col);
                // last_pixel_x = x;
            // }

            if (show_numbers && (last_sample != sample)) {


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
                int str_width = strlen(str) * FONT_WIDTH;
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
                    setCursor(cursor_x, y + (trace_height / 2) - (FONT_HEIGHT / 2));
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




// Measures the number of samples between various transitions. Intended for VGA signal analysis.
int measure(const uint32_t *buf) {
    uart_puts(UART_ID, "\nMeasuring...\n");


    char pin = 2;
    uint record_size_bits = bits_packed_per_word(g_no_of_captured_pins);

    int i;

    int green_start;

    for (i = 0; i < g_capture_n_samples; i++) {
        uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

        if (sample) {
            green_start = i;
            uart_putcf(UART_ID, "green_start: %d\n", green_start);
            break;
        }

    }

    int hsync_end;

    pin = 0;

    for (i = green_start; i >= 0; i--) {
        uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

        if (!sample) {
            hsync_end = i + 1; // this sample is still high (we're going backwards)
            uart_putcf(UART_ID, "hsync_end: %d\n", hsync_end);
            break;
        }

    }

    uart_putcf(UART_ID, "hsync_end..green_start: %d\n", green_start - (hsync_end + 0));

    pin = 1; // vsync
    // uint record_size_bits = bits_packed_per_word(CAPTURE_PIN_COUNT);

    // int i;

    // int p2;
    int vsync_start;

    for (i = 0; i < g_capture_n_samples; i++) {
        uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

        if (!sample) {
            vsync_start = i;
            uart_putcf(UART_ID, "vsync_start: %d\n", vsync_start);
            break;
        }

    }


    int vsync_end;

    for (i = vsync_start; i < g_capture_n_samples; i++) {
        uint sample = (buf, pin, g_no_of_captured_pins, i, record_size_bits);

        if (sample) {
            vsync_end = i;
            uart_putcf(UART_ID, "vsync_end: %d\n", vsync_end);
            break;
        }

    }

    uart_putcf(UART_ID, "vsync_start..vsync_end: %d\n", vsync_end - (vsync_start + 0));

    pin = 0; // hsync

    // int hsync_end;

    for (i = vsync_end; i >= 0; i--) {
        uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

        if (!sample) {
            hsync_end = i + 1;
            uart_putcf(UART_ID, "hsync_end: %d\n", hsync_end);
            break;
        }

    }


    uart_putcf(UART_ID, "hsync_end..vsync_start: %d\n", vsync_end - (hsync_end + 0));

    uart_putcf(UART_ID, "vsync_end..green_start: %d\n", green_start - (vsync_end + 0));

    pin = 0;

/*
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
*/

    // uart_putcf(UART_ID, "p0..p2: %d\n", p2 - (p0 + 1));
    return 0;
}


// Finds the previous or next transition.
int find_transition(const uint32_t *buf, uint8_t pin, int from_sample, bool next) {
    // uart_puts(UART_ID, "\nFinding transition...\n");

    uint record_size_bits = bits_packed_per_word(g_no_of_captured_pins);

    int i;

    int green_start;

    uint first_sample;

    int next_sample = from_sample;


    if (next) {

        for (i = from_sample; i < g_capture_n_samples; i++) {
            uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

            if (i == from_sample) {
                first_sample = sample;
            }

            if (sample != first_sample) {
                next_sample = i;
                // uart_putcf(UART_ID, "next_sample: %d\n", next_sample);
                break;
            }

        }

    } else {

        for (i = from_sample - 1; i >= 0; i--) {
            uint sample = get_channel_sample(buf, pin, g_no_of_captured_pins, i, record_size_bits);

            if (i == from_sample - 1) {
                first_sample = sample;
            }

            if (sample != first_sample) {
                next_sample = i + 1;
                // uart_putcf(UART_ID, "next_sample: %d\n", next_sample);
                break;
            }

        }

    }




    return next_sample;
}


// Demonstrates the graphics primitives.
void demo() {
    // color chooser
    static char color_index = 0 ;
   // position of the disc primitive
    static short disc_x = 0 ;
    // position of the box primitive
    static short box_x = 0 ;
    // position of vertical line primitive
    static short Vline_x = 350;
    // position of horizontal line primitive
    static short Hline_y = 250;
    // circle radii:
    static short circle_x = 0 ;

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

enum UI_COMMANDS {
    UIC_NONE,
    UIC_LEFT,
    UIC_RIGHT,
    UIC_CTRL_LEFT,
    UIC_CTRL_RIGHT,
    UIC_HOME,
    UIC_END,
    UIC_EQUALS,
    UIC_PLUS,
    UIC_MINUS,
    UIC_Z,
    UIC_M,
    UIC_C,
    UIC_H,
    UIC_SPACEBAR,
    UIC_UP,
    UIC_DOWN,
    UIC_PAGE_UP,
    UIC_PAGE_DOWN,
    UIC_0,
    UIC_TAB,
    UIC_SHIFT_TAB
 };


#define HINT_LEFT STATUSBAR_LEFT + 1

void draw_hint(char *s) {
    fillRect(STATUSBAR_LEFT, STATUSBAR_TOP, STATUSBAR_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_COLOR);
    setCursor(HINT_LEFT, STATUSBAR_TOP + STATUSBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
    writeString(s);
}


void draw_setting_helper(uint left, uint8_t label_len, uint8_t str_len) {
    fillRect(left + (FONT_WIDTH * label_len), TOOLBAR_TOP, (FONT_WIDTH * str_len), TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    setCursor(left + (FONT_WIDTH * label_len), TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}


void draw_channel_no() {
    draw_setting_helper(CHANNEL_NO_LEFT, 2, 3);
    if (settings_state == SS_CHANNEL) {
        uart_putcf(UART_ID, "ch: %d\n", g_channel);
        setTextColor2(TOOLBAR_COLOR, WHITE);
    }
    write_intf(" %d ", g_channel);
}


void draw_magnification() {
    draw_setting_helper(MAG_LEFT, 4, 7);
    if (settings_state == SS_ZOOM) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_puts(UART_ID, "zoom: ");
        if (g_mag < 0) {
            uart_putcf(UART_ID, "1:%d\n", abs(g_mag - 1));
        } else {
            uart_putcf(UART_ID, "%d:1\n", abs(g_mag + 1));
        }
    }
    if (g_mag < 0) {
        write_intf(" 1:%d ", abs(g_mag - 1));
    } else {
        write_intf(" %d:1 ", g_mag + 1);
    }
}


void draw_no_of_pins() {
    draw_setting_helper(NO_OF_PINS_LEFT, 4, 4);
    if (settings_state == SS_NO_OF_PINS) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "pins: %d\n", g_no_of_pins_to_capture);
    }
    write_intf(" %d ", g_no_of_pins_to_capture);
}


void draw_pins_base() {
    draw_setting_helper(PINS_BASE_LEFT, 4, 7);
    if (settings_state == SS_PINS_BASE) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "base: %d\n", g_pins_base);
    }
    write_intf(" GP%d ", g_pins_base);
}


void draw_sample_frequency() {
    draw_setting_helper(FREQ_LEFT, 4, 5);
    if (settings_state == SS_FREQ) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "fdiv: %d\n", g_sample_frequency);
    }
    write_intf(" %d ", g_sample_frequency);
}


void draw_trigger_pin_base() {
    draw_setting_helper(TRIGGER_BASE_LEFT, 4, 6);
    if (settings_state == SS_TRIGGER_PIN_BASE) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "tpin: %d\n", g_trigger_pin_base);
    }
    write_intf(" GP%d ", g_trigger_pin_base);
}


void draw_trigger_type() {

// enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE, TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_COUNT};

    unsigned char tt_chars[9][8] = {" NONE ", " LOW ", " HIGH ", " RISE ", " FALL ", " EDGE ", " VSYNC ", " RGB ", " VFPOR "};
    draw_setting_helper(TRIGGER_TYPE_LEFT, 5, 7);
    if (settings_state == SS_TRIGGER_TYPE) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_puts(UART_ID, "ttype:");
        uart_puts(UART_ID, tt_chars[g_trigger_type]);
        uart_puts(UART_ID, "\n");
    }
    writeString(tt_chars[g_trigger_type]);
}


void draw_settings() {
    draw_channel_no();
    draw_magnification();
    draw_sample_frequency();
    draw_no_of_pins();
    draw_pins_base();
    draw_trigger_pin_base();
    draw_trigger_type();
}


void draw_toolbar() {
    fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_WIDTH, TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    setTextColor(WHITE);
    setCursor(CHANNEL_NO_LEFT, TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextSize(1);
    writeString("ch    zoom       fdiv      pins    base       tpin       ttype");
    draw_settings();
}


void set_toolbar_text() {
    setCursor(TOOLBAR_LEFT + TOOLBAR_TEXT_PADDING, TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}


void clear_toolbar_hint() {
    set_toolbar_text();
    fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_HINT_WIDTH, TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    setCursor(2, SCREEN_HEIGHT - 1 - 8);
    setTextColor(WHITE);
    setTextSize(1);
}


void draw_statusbar() {
    fillRect(STATUSBAR_LEFT, STATUSBAR_TOP, STATUSBAR_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_COLOR);
    // draw_settings();
}


void set_statusbar_text() {
    setCursor(STATUSBAR_LEFT + STATUSBAR_TEXT_PADDING, STATUSBAR_TOP + STATUSBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}


void clear_statusbar_hint() {
    set_statusbar_text();
    fillRect(STATUSBAR_LEFT, STATUSBAR_TOP, STATUSBAR_HINT_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_COLOR);
    setCursor(2, SCREEN_HEIGHT - 1 - 8);
    setTextColor(WHITE);
    setTextSize(1);
}


uint check_keyboard() {

    uint ui_command = UIC_NONE;

    if ((last_uart_char == 27) || uart_is_readable(UART_ID)) {


        clear_statusbar_hint();
        set_statusbar_text();
        // fillRect(1, SCREEN_HEIGHT - 1 - 8, SCREEN_WIDTH - 2, 8, BLUE);
        // setCursor(2, SCREEN_HEIGHT - 1 - 8);
        // setTextColor(WHITE);
        // setTextSize(1);

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
        // if (uart_is_writable(UART_ID)) {
        //     uart_putc(UART_ID, ch);
        // }


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
                    // if (uart_is_writable(UART_ID)) {
                    //     uart_putc(UART_ID, ch);
                    // }
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
                            // writeString("up");
                            ui_command = UIC_UP;
                            break;
                        case 'B':
                            // writeString("down");
                            ui_command = UIC_DOWN;
                            break;
                        case 'C':
                            ui_command = UIC_RIGHT;
                            break;
                        case 'D':
                            ui_command = UIC_LEFT;
                            break;
                        case 'Z':
                            ui_command = UIC_SHIFT_TAB;
                            break;

                        default:
                            break;
                    }
                } else if ((char)escape_seq[0] == 'O') {
                    if ((char)escape_seq[1] == 'F') {
                        ui_command = UIC_END;
                    }
                }
            } else if (strlen(escape_seq) == 3) {
                if ((char)escape_seq[2] == '~') {
                    if ((char)escape_seq[1] == '1') {
                        ui_command = UIC_HOME;

                    } else if ((char)escape_seq[1] == '5') {
                        ui_command = UIC_PAGE_UP;

                    } else if ((char)escape_seq[1] == '6') {
                        ui_command = UIC_PAGE_DOWN;
                    }
                }
            } else if (strlen(escape_seq) == 5) {
                if (strcmp(escape_seq, "[1;5D") == 0) {
                        ui_command = UIC_CTRL_LEFT;

                } else if (strcmp(escape_seq, "[1;5C") == 0) {
                    ui_command = UIC_CTRL_RIGHT;
                }
            }

        } else if ((char)ch == ' ') {
//            pause = !pause; // toggle pause
            ui_command = UIC_SPACEBAR;
        } else if ((char)ch == '-') {
            ui_command = UIC_MINUS;
        } else if ((char)ch == '+' ) {
            ui_command = UIC_PLUS;
        } else if ((char)ch == '=') {
            ui_command = UIC_EQUALS;
        } else if ((char)ch == 'c') {
            ui_command = UIC_C;
        } else if ((char)ch == 'z') {
            ui_command = UIC_Z;
        } else if ((char)ch == 'm') {
            ui_command = UIC_M;
        } else if ((char)ch == '0') {
            ui_command = UIC_0;
        } else if ((char)ch == 'h') {
            ui_command = UIC_H;
        } else if (ch == 9) {
            ui_command = UIC_TAB;
            // uart_puts(UART_ID, "TAB\n");

        /*
        } else if ((char)ch == 'n') {
            ui_command = UIC_NEXT_TRANSITION;
        } else if ((char)ch == 'p') {
            ui_command = UIC_PREV_TRANSITION;
        */
        }

    }
    return ui_command;
}


void draw_minimap_indicator() {
    int mini_x = (g_scrollx * SCREEN_WIDTH) / g_capture_n_samples;
    int mini_w = (mag_factor(SCREEN_WIDTH * SCREEN_WIDTH) / g_capture_n_samples) + 1; // add one to round up and/or ensure a visible indicator


    uint y = MINIMAP_BOTTOM - ((MINIMAP_HEIGHT + MINIMAP_PADDING) * g_no_of_captured_pins) - 2;


    drawHLine(0, y, SCREEN_WIDTH, BLACK);
    drawHLine(mini_x, y, mini_w, WHITE);

    // uart_putcf(UART_ID, "mini_x: %d; ", mini_x);
    // uart_putcf(UART_ID, "mini_w: %d\n", mini_w);
}


bool set_scroll_x(int x) {
    bool changed = x != g_scrollx;
    if (changed) {
        g_scrollx = x;
    }
    draw_minimap_indicator();
    return changed;
}


bool set_mag(int mag) {
    bool changed = mag != g_mag;
    if (changed) {
        g_mag = mag;
        draw_magnification();
        draw_minimap_indicator();
    }
    return changed;
}


void set_channel (uint8_t ch) {
    if (g_channel != ch) {
        g_channel = ch;
        draw_channel_no();
    }
}


void set_sample_frequency(uint f) {
    if (g_sample_frequency != f) {
        g_sample_frequency = f;
        draw_sample_frequency();
    }
}


void set_no_of_pins(uint8_t no_of_pins) {
    if (g_no_of_pins_to_capture != no_of_pins) {
        g_no_of_pins_to_capture = no_of_pins;
        draw_no_of_pins();
    }
}


void set_pins_base(uint8_t pins_base) {
    if (g_pins_base != pins_base) {
        g_pins_base = pins_base;
        draw_pins_base();
    }
}


void set_trigger_pin_base(uint8_t trigger_pin_base) {
    if (g_trigger_pin_base != trigger_pin_base) {
        g_trigger_pin_base = trigger_pin_base;
        draw_trigger_pin_base();
    }
}


void set_trigger_type(uint8_t trigger_type) {
    if (g_trigger_type != trigger_type) {
        g_trigger_type = trigger_type;
        draw_trigger_type();
    }
}


void draw_setting(uint8_t setting) {
    switch (setting) {
        case SS_CHANNEL:
            draw_channel_no();
            break;

        case SS_FREQ:
            draw_sample_frequency();
            break;

        case SS_NO_OF_PINS:
            draw_no_of_pins();
            break;

        case SS_PINS_BASE:
            draw_pins_base();
            break;

        case SS_TRIGGER_PIN_BASE:
            draw_trigger_pin_base();
            break;

        case SS_TRIGGER_TYPE:
            draw_trigger_type();
            break;

        case SS_ZOOM:
            draw_magnification();
            break;
    }
    // if (setting == settings_state) {
    //     draw_hint("Press UP / DOWN to increase / decrease the selected setting.");
    // }
}


void set_settings_state(uint8_t state) {
    uint8_t previous_state = settings_state;
    settings_state = state;
    draw_setting(previous_state);
    draw_setting(settings_state);
    
    // draw_settings();

    // draw a line under, or something, under the appropriate item in the toolbar
    // the one that now will respond to the up and down keys
    // uart_putcf(UART_ID, "State: %d\n", settings_state);
}


char* help_strings =
    "LEFT / RIGHT to scroll one sample period left / right\n"
    "CTRL-LEFT / CTRL-RIGHT to scroll to previous / next transition\n"
    "  on the selected channel (ch)\n"
    "PGUP / PGDN to scroll one page left / right\n"
    "HOME / END to scroll to the beginning / end\n"
    "TAB / SHIFT-TAB to select the next / previous setting\n"
    "UP / DOWN to increase / decrease the selected setting\n"
    "'c' to capture a sample with the settings'\n"
    "'z' to zoom to fit all the samples on one page'\n"
    "'+' / '-' to zoom in / out\n"
    "'=' to zoom to 1:1\n"
    "'m' to measure VGA timings\n"
    "'h' to show this help\n";

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

    uart_puts(UART_ID, help_strings);

    // We're going to capture into a u32 buffer, for best DMA efficiency. Need
    // to be careful of rounding in case the number of pins being sampled
    // isn't a power of 2.
    uint total_sample_bits = g_capture_n_samples * g_no_of_captured_pins;
    total_sample_bits += bits_packed_per_word(g_no_of_captured_pins) - 1;

    uint buf_size_words = total_sample_bits / bits_packed_per_word(g_no_of_captured_pins);
    uint32_t *capture_buf = malloc(buf_size_words * sizeof(uint32_t));
    hard_assert(capture_buf);

    // if we change the no of pins during the program then our CAPTURE_N_SAMPLES will possibly be wrong
    // so let's try and reverse the maths to give us CAPTURE_N_SAMPLES given buf_size_words and g_no_of_pins...

    // total_sample_bits = buf_size_words * bits_packed_per_word(g_no_of_pins);
    // total_sample_bits -= bits_packed_per_word(g_no_of_pins) - 1;
    // CAPTURE_N_SAMPLES = total_sample_bits / g_no_of_pins;

    // now to test it... at the start of the plotting function, perhaps...

    // Grant high bus priority to the DMA, so it can shove the processors out
    // of the way. This should only be needed if you are pushing things up to
    // >16bits/clk here, i.e. if you need to saturate the bus completely.

    // todo - uncomment this next line if we need to
    // bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    PIO pio = pio1;
    uint sm = 3;
    uint dma_chan = dma_claim_unused_channel(true);

    // logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 125000000 / (115200 * 4) /*271.267*/);
    logic_analyser_init(pio, sm, g_pins_base, g_no_of_pins_to_capture, g_sample_frequency);

    // logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 5); // this works

    // animation pause
    bool demo_paused = true;


    // Draw some filled rectangles
    fillRect(64, 0, 176, 50, BLUE); // blue box
    fillRect(250, 0, 176, 50, RED); // red box:
    fillRect(435, 0, 176, 50, GREEN); // green box

//    drawVLine(Vline_x, 300, (Vline_x>>2), color_index);

    drawHLine(0, 0, 16, WHITE);
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

    draw_toolbar();

    draw_statusbar();

    // drawHLine(630, 480 - 1, 1, WHITE);


    for (int i = 0; i < 16; i++){
        drawPixel(16 + i , 0, i); // test all colours in first horizontal line following sync
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


    // for (int i = 0; i < 16; i++){
    //     fillRect(i * 40, 51, 40, 40, i); // colour boxes
    // }

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

    logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, g_trigger_pin_base, g_trigger_type);

    // print_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);
    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, true);

    // plot a fixed minimap of the above capture
    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);

    draw_minimap_indicator();

    // fillRect(0, 0, 640, 480, WHITE); // green box

    while(true) {

        if (!demo_paused) {
            demo();
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

        uint ui_command = check_keyboard();

        if (ui_command) {
            bool plot_required = false;
            switch (ui_command) {

                case UIC_SPACEBAR:
                    writeString("space");
                    demo_paused = !demo_paused;
                    break;

                case UIC_CTRL_RIGHT:
                case UIC_CTRL_LEFT:
                    writeString(ui_command == UIC_CTRL_RIGHT ? "next" : "previous");
                    writeString(" edge");
                    plot_required = set_scroll_x(find_transition(capture_buf, g_channel, g_scrollx, ui_command == UIC_CTRL_RIGHT));
                    break;

                case UIC_HOME:
                    writeString("scroll home");
                    plot_required = set_scroll_x(0);
                    break;

                case UIC_END:
                    writeString("scroll end");
                    plot_required = set_scroll_x(MAX(g_capture_n_samples - (mag_factor(SCREEN_WIDTH)), 0));
                    break;

                case UIC_RIGHT:
                    writeString("scroll right");
                    plot_required = set_scroll_x(g_scrollx + 1);
                    break;

                case UIC_LEFT:
                    writeString("scroll left");
                    plot_required = set_scroll_x(MAX(g_scrollx - 1, 0));
                    break;

                case UIC_PAGE_UP:
                    writeString("scroll left a page");
                    plot_required = set_scroll_x(MAX(g_scrollx - mag_factor(SCREEN_WIDTH), 0));
                    break;

                case UIC_PAGE_DOWN:
                    writeString("scroll right a page");
                    plot_required = set_scroll_x(g_scrollx + mag_factor(SCREEN_WIDTH)); // 100% of the screen width
                    break;

                case UIC_MINUS:
                    writeString("zoom out");
                    plot_required = set_mag(g_mag - 1);
                    break;

                case UIC_PLUS:
                    writeString("zoom in");
                    plot_required = set_mag(g_mag + 1);
                    break;

                case UIC_EQUALS:
                    writeString("no zoom");
                    plot_required = set_mag(0);
                    break;

                case UIC_M:
                    writeString("measure");
                    measure(capture_buf);
                    break;

                case UIC_Z:
                    writeString("zoom to fill");
                    if (g_capture_n_samples >= SCREEN_WIDTH) {
                        // we need to zoom out
                        int factor = g_capture_n_samples / SCREEN_WIDTH;
                        plot_required = set_mag(-(factor - 1));
                    } else {
                        // we need to zoom in
                        int factor = SCREEN_WIDTH / g_capture_n_samples;
                        plot_required = set_mag(factor - 1);
                    }
                    // set_scroll_x(0);
                    break;

                case UIC_C:
                    writeString("capture");
                    fillRect(0, PLOT_TOP, SCREEN_WIDTH, MINIMAP_BOTTOM - PLOT_TOP, BLACK);

                    logic_analyser_init(pio, sm, g_pins_base, g_no_of_pins_to_capture, g_sample_frequency);

                    if (!logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, g_trigger_pin_base, g_trigger_type)) {
                        writeString(" - failed to trigger");
                    }

                    // each bit on a uart travels at 115200 bits per second
                    // the clock goes at 125,000,000 hz (I think)

                    // so if we want to take, say, 4 samples per bit then we need to sample at  4 * 115200 = 460800 bps
                    // 125,000,000 / 460,800 = 271.267

                    // The logic analyser should have started capturing as soon as it saw the
                    // first transition. Wait until the last sample comes in from the DMA.

                    // before we plot, let's recalculate g_capture_n_samples as we may have changed the number of pins
                    // to capture...

                    // heres the code to reverse

                    // uint total_sample_bits = g_capture_n_samples * g_no_of_pins;
                    // total_sample_bits += bits_packed_per_word(g_no_of_pins) - 1;
                    // uint buf_size_words = total_sample_bits / bits_packed_per_word(g_no_of_pins);

                    // end of code to reverse

                    uart_putcf(UART_ID, "g_capture_n_samples before calc: %d\n", g_capture_n_samples);

                    total_sample_bits = buf_size_words * bits_packed_per_word(g_no_of_captured_pins);
                    uart_putcf(UART_ID, "total_sample_bits: %d\n", total_sample_bits);

                    // total_sample_bits -= bits_packed_per_word(g_no_of_pins) - 1; ***
                    // uart_putcf(UART_ID, "total_sample_bits: %d\n", total_sample_bits);

                    g_capture_n_samples = total_sample_bits / g_no_of_captured_pins;
                    uart_putcf(UART_ID, "g_capture_n_samples after calc: %d\n", g_capture_n_samples);

                    // it seems to work, but when not 4 - it throws the scaling out. for example the zoom to fill screen no
                    // longer calculates correctly - pressing 'END' crashes the program. think I know why, but not now
                    // actually it can't be quite right as the
                    // strangely removing the line with the *** aove seems to have fixed it. Hmm...


                    plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);

                    set_scroll_x(0);

                    plot_required = true;

                    break;

                case UIC_DOWN:
                    writeString("decrease setting");

                    switch (settings_state) {
                        case SS_CHANNEL:
                            set_channel(MAX(g_channel - 1, 0));
                            break;

                        case SS_FREQ:
                            set_sample_frequency(MAX(g_sample_frequency - 1, 1));
                            break;

                        case SS_NO_OF_PINS:
                            set_no_of_pins(MAX(g_no_of_pins_to_capture - 1, 1));
                            break;

                        case SS_PINS_BASE:
                            set_pins_base(MAX(g_pins_base - 1, 0));
                            break;

                        case SS_TRIGGER_PIN_BASE:
                            set_trigger_pin_base(MAX(g_trigger_pin_base - 1, 0));
                            break;

                        case SS_TRIGGER_TYPE:
                            set_trigger_type(MAX(g_trigger_type - 1, 0));
                            break;

                        case SS_ZOOM:
                            plot_required = set_mag(g_mag - 1);
                            break;

                    }
                    break;

                case UIC_UP:
                    writeString("increase setting");

                    switch (settings_state) {
                        case SS_CHANNEL:
                            set_channel(MIN(g_channel + 1, g_no_of_captured_pins - 1));
                            break;

                        case SS_FREQ:
                            set_sample_frequency(g_sample_frequency + 1);
                            break;

                        case SS_NO_OF_PINS:
                            set_no_of_pins(MIN(g_no_of_pins_to_capture + 1, 8));
                            break;

                        case SS_PINS_BASE:
                            set_pins_base(MIN(g_pins_base + 1, 29));
                            break;

                        case SS_TRIGGER_PIN_BASE:
                            set_trigger_pin_base(MIN(g_trigger_pin_base + 1, 29));
                            break;

                        case SS_TRIGGER_TYPE:
                            set_trigger_type(MIN(g_trigger_type + 1, TT_COUNT - 1));
                            break;

                        case SS_ZOOM:
                            plot_required = set_mag(g_mag + 1);
                            break;

                    }
                    break;
                /*
                case UIC_0:


                    logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, g_no_of_captured_pins, 5 * 4 * 16);

                    logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, CAPTURE_TRIGGER_PIN, false);

                    // each bit on a uart travels at 115200 bits per second
                    // the clock goes at 125,000,000 hz (I think)

                    // so if we want to take, say, 4 samples per bit then we need to sample at  4 * 115200 = 460800 bps
                    // 125,000,000 / 460,800 = 271.267

                    // The logic analyser should have started capturing as soon as it saw the
                    // first transition. Wait until the last sample comes in from the DMA.

                    plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);
                    plot_required = true;
                    break;
                */

                case UIC_TAB:
                    writeString("next setting");

                    set_settings_state((settings_state + 1) % SS_COUNT);
                    // draw a line under, or something under the appropriate item in the toolbar
                    // the one that now will respond to the up and down keys
                    // uart_putcf(UART_ID, "State: %d\n", settings_state);
                    break;

                case UIC_SHIFT_TAB:
                    writeString("previous setting");

                    if (settings_state <= 0) {
                        set_settings_state(SS_COUNT - 1);
                    } else {
                        set_settings_state(settings_state - 1);
                    }
                    // uart_putcf(UART_ID, "State: %d\n", settings_state);
                    break;

                case UIC_H:
                    writeString("help");
                    uart_puts(UART_ID, "\n");
                    uart_puts(UART_ID, help_strings);
                    uart_puts(UART_ID, "\n");
                    break;
            }

            if (plot_required) {
                plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, true);
            }

        } else {
            // A brief nap
            sleep_ms(10) ;
        }


        // uart_putc(UART_ID, 'B');

   }

}