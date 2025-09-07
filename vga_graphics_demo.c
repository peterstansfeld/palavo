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

#include "hardware/clocks.h"
#include "nec_ir_rx.pio.h"

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/uart.h"

// #include "hardware/timer.h"
// #include "hardware/structs/bus_ctrl.h"
#include <string.h>

// VGA graphics library
#include "vga2_graphics.h"

#define UART_ID uart1
#define BAUD_RATE 115200

#define DEBUG_PIN 28


// We are using pins 4 and 5, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.

// #define UART_TX_PIN 0
// #define UART_RX_PIN 1

// #define UART_TX_PIN 0
// #define UART_RX_PIN 1

// Improve GPIO 

// Relying on PICO_PIO_USE_GPIO_BASE is an issue as sometimes a Pico 2 format
// alternative might be used instead, and it is not desired to have the VGA
// out port on GP31-37.

// So, we need to have a define to say that we are using a particular board
// and if that board has the extra GPIO exposed AND we want to move 



// Each board in the SDK has a unique identifier, for board detection,  e.g.

// RASPBERRYPI_PICO2
// PIMORONI_PICO_LIPO2XL_W_RP2350

// It's not available yet but there should be a

// PIMORONI_PICO_LIPO2XL_W_RP2350

// or something like that. We're using PIMORONI_PICO_LIPO2XL_W_RP2350 until one is
// available. However, we could always define our own, for now. 



// #if PICO_BOARD == pico2 // Can't do this, so what are the alternatives?

// USE_DVI chould be defined as 1, or not, in CMakeLists.txt depending on whether
// PICO_BOARD is "pico2", or something else.

// For developing with colour syntax highlighting it might be helpful to define it
// here too, but don't forget to undo that when done.

// There's probably a better way to do this, but I'm not aware of one. 

// #define USE_DVI 1

#ifndef RASPBERRYPI_PICO2
// when finished editing / testing comment out everything in this #ifndef..#endif section
// #define RASPBERRYPI_PICO2
#endif

#ifndef PIMORONI_PICO_LIPO2XL_W_RP2350
// when finished editing / testing comment out everything in this #ifndef..#endif section
// #define PIMORONI_PICO_LIPO2XL_W_RP2350
#endif


#ifdef RASPBERRYPI_PICO2
#define USE_DVI 1
#define USE_VGA_CAPTURE 1
#define LED_PIN PICO_DEFAULT_LED_PIN

#define USE_LED_AS_IR_DEBUG 0

#define UART_TX_PIN 20
#define UART_RX_PIN 21
#define CSYNC 22

#define HSYNC_IN 26
#define VSYNC_IN 27
#define RGB_IN_FIRST_PIN 0


#define IR_RX_PIN 28

#define GPIO_INPUT_MASK ((1 << VSYNC_IN) | (1 << HSYNC_IN) | (1 << IR_RX_PIN) | 0b0111111 /* 5-0*/) 

#else

#ifdef PIMORONI_PICO_LIPO2XL_W_RP2350

#define USE_DVI 0
#define USE_VGA_CAPTURE 0

// #define LED_PIN PICO_DEFAULT_LED_PIN

#define USE_LED_AS_IR_DEBUG 0

#define IR_RX_PIN 28

#define CSYNC 31

#define UART_TX_PIN 38
#define UART_RX_PIN 39

#define HSYNC_IN 0
#define VSYNC_IN 1

#define RGB_IN_FIRST_PIN 2


#define GPIO_INPUT_MASK ((1 << IR_RX_PIN)  | (1 << 27)  | (1 << 26) | 0x07fffff /* 22-0 */)


#else

#error Please specify a supported board

#endif

#endif


#ifdef RASPBERRYPI_PICO2

#pragma message "Building Palavo for RASPBERRYPI_PICO2"

#else

#ifdef PIMORONI_PICO_LIPO2XL_W_RP2350

#pragma message "Building Palavo for PIMORONI_PICO_LIPO2XL_W_RP2350"

#else

#error Can not build Palavo for unknown RPxxxx MCU

#endif

#endif


#if USE_DVI

#include "dvi64_graphics.h"

#if USE_VGA_CAPTURE

#include "vga_capture.pio.h"

#endif

#endif

uint8_t last_uart_char = 0;

#define MAX_NO_OF_CHANNELS 32

#define MAX_NO_OF_PINS 32

#ifdef PIMORONI_PICO_LIPO2XL_W_RP2350

#define MAX_BASE_PIN_NO 47

#else

#define MAX_BASE_PIN_NO 32

#endif

// Some globals for storing timer information
volatile unsigned int time_accum = 0;
unsigned int time_accum_old = 0 ;
char timetext[40];


int g_mag = 0;
int g_scrollx = 0;
uint8_t g_channel = 0;

int g_prev_scrollx = 0;

enum SETTINGS_STATES {SS_CHANNEL, SS_ZOOM, SS_FREQ, SS_PINS_BASE, SS_NO_OF_PINS, SS_TRIGGER_PIN_BASE, SS_TRIGGER_TYPE, SS_COUNT};

uint8_t settings_state = SS_CHANNEL;

// Timer interrupt
bool repeating_timer_callback(struct repeating_timer *t) {
    time_accum += 1 ;

#if USE_LED_AS_IR_DEBUG == 0

#ifdef LED_PIN
    gpio_xor_mask(1u << LED_PIN); // toggle LED_PIN
#endif

#endif

    return true;
}

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE,
    TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_VGA_CSYNC, TT_VGA_CRGB, TT_VGA_CVFRONT_PORCH, TT_COUNT};

#define SETTINGS 1

#if SETTINGS == 0

// const uint CAPTURE_PIN_BASE = HSYNC2; // 16 = hsync, 17 = vsync // 22 = hsync2
// #define CAPTURE_PIN_BASE HSYNC2 // 16 = hsync, 17 = vsync // 22 = hsync2
// #define CAPTURE_PIN_BASE HSYNC2 // 16 = hsync, 17 = vsync // 22 = hsync2
#define CAPTURE_PIN_BASE 6 // 16 = hsync, 17 = vsync // 22 = hsync2

// const uint CAPTURE_PIN_COUNT = 4;
#define CAPTURE_PIN_COUNT 4

// const uint CAPTURE_TRIGGER_PIN = VSYNC; // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2 NB IGNORED FOR NOW!
// #define CAPTURE_TRIGGER_PIN_BASE HSYNC2 // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2 NB IGNORED FOR NOW!
#define CAPTURE_TRIGGER_PIN_BASE 22 // 8 = hsync, 9 = vsync // 22 = hsync2, 23 = vsync2 NB IGNORED FOR NOW!

// const uint CAPTURE_N_SAMPLES = SCREEN_WIDTH * 96; // enough for 48 screen width's worth of data
// #define CAPTURE_N_SAMPLES SCREEN_WIDTH * 96 // enough for 48 screen width's worth of data

//set the default sample rate to the pixel clock rate of 25 MHz
#define CAPTURE_SAMPLE_FREQ_DIVISOR (SYS_CLK_KHZ / 25000u)


#define CAPTUTRE_TRIGGER_TYPE TT_VGA_CRGB


#elif SETTINGS == 1

#define CAPTURE_PIN_BASE 0 // HSYNC

#ifndef PIMORONI_PICO_LIPO2XL_W_RP2350

#define CAPTURE_PIN_COUNT 8 // HSYNC, VSYNC and BBGGRR (blue, green, red)
#define CAPTURE_TRIGGER_PIN_BASE 26 // HSYNC

#define CAPTUTRE_TRIGGER_TYPE TT_VGA_VSYNC

#if PICO_RP2350
// assume this is a PICO2350 (running at 150 MHz)
#define CAPTURE_SAMPLE_FREQ_DIVISOR 6
#else
// assume this is a PICO2040 (running at 125 MHz)
#define CAPTURE_SAMPLE_FREQ_DIVISOR 5

#endif

#else

#define CAPTURE_PIN_COUNT 27 // GPIO0 - GPIO26
#define CAPTURE_TRIGGER_PIN_BASE 0 // one of the keybord row drivers
#define CAPTURE_SAMPLE_FREQ_DIVISOR 12 // should capture two row-driving sequences
#define CAPTUTRE_TRIGGER_TYPE TT_FALLING_EDGE // 

#endif

// #define CAPTURE_SAMPLE_FREQ_DIVISOR (SYS_CLK_KHZ / 480)

#endif


uint g_sample_frequency = CAPTURE_SAMPLE_FREQ_DIVISOR;
uint8_t g_no_of_captured_pins = CAPTURE_PIN_COUNT;
uint8_t g_pins_base = CAPTURE_PIN_BASE;

#define HORIZONTAL_BLANKING_PIXELS 160
#define VERTICAL_BLANKING_PIXELS 45

// 800 * 525 = 420,000
#define SAMPLES_IN_640_480_FRAME ((SCREEN_WIDTH + HORIZONTAL_BLANKING_PIXELS) * \
                                  (SCREEN_HEIGHT + VERTICAL_BLANKING_PIXELS))

// 420,000
#define SAMPLES_TO_CAPTURE SAMPLES_IN_640_480_FRAME


#define CHANNELS_PER_SAMPLE 4

// 1,680,000
#define TOTAL_SAMPLE_BITS (SAMPLES_TO_CAPTURE * CHANNELS_PER_SAMPLE)

// 52,500
#define BUF_SIZE_WORDS (TOTAL_SAMPLE_BITS / 32)

// // equivalent to: uint8_t array[210,000] 
// uint32_t capture_buffer[BUF_SIZE_WORDS];
// use malloc to create the 'capture_buffer' in case we need to resize it at any stage

uint g_capture_n_samples = SAMPLES_TO_CAPTURE;

uint8_t g_no_of_pins_to_capture = CAPTURE_PIN_COUNT;

uint8_t g_trigger_pin_base = CAPTURE_TRIGGER_PIN_BASE;


// uint8_t g_trigger_type = TT_VGA_VSYNC;

uint8_t g_trigger_type = CAPTUTRE_TRIGGER_TYPE;

/*
    // uint total_sample_bits = g_capture_n_samples * g_no_of_captured_pins;
    // total_sample_bits += bits_packed_per_word(g_no_of_captured_pins) - 1;

    // uint buf_size_words = total_sample_bits / bits_packed_per_word(g_no_of_captured_pins);
    // uint32_t *capture_buf = malloc(buf_size_words * sizeof(uint32_t));

#define TOTAL_SAMPLE_BITS CAPTURE_N_SAMPLES * CAPTURE_PIN_COUNT + (32 - (32 % CAPTURE_PIN_COUNT) - 1)
// #define BUF_SIZE_WORDS TOTAL_SAMPLE_BITS / bits_packed_per_word(CAPTURE_PIN_COUNT)

#define BUF_SIZE_WORDS TOTAL_SAMPLE_BITS / (32 - (32 % CAPTURE_PIN_COUNT))


uint32_t new_capture_buf[];

    // hard_assert(capture_buf);


*/
static inline uint bits_packed_per_word(uint pin_count) {
    // If the number of pins to be sampled divides the shift register size, we
    // can use the full SR and FIFO width, and push when the input shift count
    // exactly reaches 32. If not, we have to push earlier, so we use the FIFO
    // a little less efficiently.
    const uint SHIFT_REG_WIDTH = 32;
    return SHIFT_REG_WIDTH - (SHIFT_REG_WIDTH % pin_count);
}


void uart_putuif(uart_inst_t *uart, const char *s, uint c) {
    char str[80];
    sprintf(str, s, c);
    uart_puts(uart, str);
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

#if PICO_PIO_USE_GPIO_BASE==1
    int res;
    if ((pin_base >= 16) && (pin_base + pin_count > 32)) { 
        res = pio_set_gpio_base(pio, 16);
        uart_putuif(UART_ID, "pio_set_gpio_base(pio, 16); res: %d\n", res);
    } else {
        res = pio_set_gpio_base(pio, 0);
        uart_putuif(UART_ID, "pio_set_gpio_base(pio, 0); res: %d\n", res);
    }
#endif

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


#ifdef PIMORONI_PICO_LIPO2XL_W_RP2350

// When this is #define'ed instead of a global variable the triggering respose is much slower
// Is it because it's stored in flash rather than ram? todo -find out

// #define la_trigger_pio pio0
PIO la_trigger_pio = pio0;
uint la_trigger_sm = 3;

void logic_analyser_trigger_init(bool init) {
    // Load a program to simply allow us to use this state machine for trigger detection.
    // This to allow a trigger pin that is not in the same GPIO_BASE as the capture pins.
    // It's just a `nop` instruction with a wrap.

    static bool initialised;
    static uint16_t trigger_prog_instr;
    static struct pio_program trigger_prog;
    static uint offset;

    if (init) {
        // we want to initialise it
        if (!initialised) {
            // we can
            trigger_prog_instr = pio_encode_nop();

            trigger_prog.instructions = &trigger_prog_instr;
            trigger_prog.length = 1;
            trigger_prog.origin = -1;

            offset = pio_add_program(la_trigger_pio, &trigger_prog);

            // Configure state machine to loop over this `in` instruction forever,
            // with autopush enabled.
            pio_sm_config c = pio_get_default_sm_config();
            // sm_config_set_in_pins(&c, pin_base);
            sm_config_set_wrap(&c, offset, offset);
            // sm_config_set_clkdiv(&c, 1);
            // Note that we may push at a < 32 bit threshold if pin_count does not
            // divide 32. We are using shift-to-right, so the sample data ends up
            // left-justified in the FIFO in this case, with some zeroes at the LSBs.
            // sm_config_set_in_shift(&c, true, true, bits_packed_per_word(pin_count));
            // sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
            pio_sm_init(la_trigger_pio, la_trigger_sm, offset, &c);
            initialised = true;
        }
    } else {
        // we want to deinitialise it
        if (initialised) {
            // we can
            pio_remove_program(la_trigger_pio, &trigger_prog, offset);
            initialised = false;
        }
    }
}

#endif


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


// define a structure for storing edge locations for each of the channels

struct Edges {
    int prev_start;
    int prev_end;
    int next_start;
    int next_end;
};

struct Edges edges[MAX_NO_OF_CHANNELS];

void clear_previous_edges() {
        memset(edges, 0, sizeof(edges[0]) * MAX_NO_OF_CHANNELS);
}

#ifndef PIMORONI_PICO_LIPO2XL_W_RP2350
    PIO ir_rx_pio = pio2;
    // #warning IR_RX is using pio2
#else
    PIO ir_rx_pio = pio2;
    // #warning IR_RX is using pio1
#endif

uint ir_rx_sm = 0;
uint ir_rx_offset;

bool ir_rx_sm_initialised;
// receive ir on pin 28


void init_ir_rx(bool init) {

    if (init) {

        if (!ir_rx_sm_initialised) {

            ir_rx_offset = pio_add_program(ir_rx_pio, &nec_ir_rx_program);

        #if USE_LED_AS_IR_DEBUG
        // todo tidy this up into one line and work out what we're doing with the LED
            // nec_ir_rx_program_init(ir_rx_pio, ir_rx_sm, ir_rx_offset, IR_RX_PIN, LED_PIN);
            nec_ir_rx_program_init(ir_rx_pio, ir_rx_sm, ir_rx_offset, IR_RX_PIN, -1);
        #else
            nec_ir_rx_program_init(ir_rx_pio, ir_rx_sm, ir_rx_offset, IR_RX_PIN, -1);
        #endif

            pio_enable_sm_mask_in_sync(ir_rx_pio, (1u << ir_rx_sm));

            ir_rx_sm_initialised = true;

    #if PICO_PIO_USE_GPIO_BASE==1
            pio_set_gpio_base(ir_rx_pio, 0);
    #endif

        }
    } else {
        if (ir_rx_sm_initialised) {

            // stop the state machine

            pio_sm_set_enabled(ir_rx_pio, ir_rx_sm, false);

            // and free it
            pio_remove_program(ir_rx_pio, &nec_ir_rx_program, ir_rx_offset);
            
            ir_rx_sm_initialised = false;

        }

    }
}

#define TRIGGER_TIMEOUT_US 2000000 // 2 seconds


bool logic_analyser_arm(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words,
                        uint trigger_pin, uint8_t trigger_type) {

    PIO trigger_pio = pio;
    uint trigger_sm = sm;

    PIO capture_pio = pio;
    uint capture_sm = sm;

    // free pio resources for the ir rx
    init_ir_rx(false);

    uart_puts(UART_ID, "\nArming trigger...\n");

    pio_sm_set_enabled(pio, sm, false);
    // Need to clear _input shift counter_, as well as FIFO, because there may be
    // partial ISR contents left over from a previous run. sm_restart does this.
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);

#if PICO_PIO_USE_GPIO_BASE==1
    uint gpio_base = pio_get_gpio_base(pio);

    int res;

    bool changed_base = false;
    if (gpio_base == 16) {
        // base is mapped to gpio 16-47
        if (trigger_pin < 16) {
            // need to use the other pio-state machine to trigger
            // pio_get_gpio_base(pio, 0);

            logic_analyser_trigger_init(false);
            res = pio_set_gpio_base(la_trigger_pio, 0);
            logic_analyser_trigger_init(true);

            trigger_pio = la_trigger_pio;
            trigger_sm = la_trigger_sm;

            uart_puts(UART_ID, "pio_set_gpio_base(pio, 0) for trigger\n\n");
            uart_putuif(UART_ID, "res: %d\n", res);
            changed_base = true;
        } else {
            trigger_pin -= 16;
        }

    } else {
        if (trigger_pin > 16) {
            // pio_get_gpio_base(pio, 16);
            logic_analyser_trigger_init(false);
            res = pio_set_gpio_base(la_trigger_pio, 16);
            logic_analyser_trigger_init(true);
            trigger_pio = la_trigger_pio;
            trigger_sm = la_trigger_sm;

            uart_puts(UART_ID, "pio_set_gpio_base(pio, 16) for trigger\n\n");
            uart_putuif(UART_ID, "res: %d\n", res);
            changed_base = true;
            trigger_pin -= 16;
        }
    }

#endif

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

    // Wait for a rising edge on VSYNC, which involves:
    // 1. Waiting until VSYNC is at the opposite level of the trigger level.

    bool triggered = false; // assume fail


// enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE, TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_VGA_CSYNC, TT_COUNT};

    switch (trigger_type) {

        case TT_NONE:
            triggered = true;
            break;

        case TT_LOW_LEVEL:
            triggered = pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US);
            break;

        case TT_HIGH_LEVEL:
            triggered = pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US);
            break;

        case TT_RISING_EDGE:
            if (pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US)) {
                triggered = pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US);
            }
            break;

        case TT_FALLING_EDGE:
            if (pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), TRIGGER_TIMEOUT_US)) {
                triggered = pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), TRIGGER_TIMEOUT_US);
            }
            break;

        case TT_ANY_EDGE:
            // read the state of the trigger pin
            bool trigger_pin_state = gpio_get(trigger_pin);

            // wait for it to change to the opposite state
            triggered = pio_sm_exec_timeout_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(!trigger_pin_state, trigger_pin), TRIGGER_TIMEOUT_US);
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

            if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin + 1), expiry_time)) {
                // VSYNC is high
                if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin + 1), expiry_time)) {
                    // VSYNC is low
                    triggered = true; // assume pass
                    // Wait for x HSYNC pulses before starting the capture
                    for (int i = 0; i < x; i++) {
                        if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), expiry_time)) {
                            if (!pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), expiry_time)) {
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

        case TT_VGA_CSYNC:
        case TT_VGA_CRGB:
        case TT_VGA_CVFRONT_PORCH:

            // int x = 1; // number of pulses 
            // switch (trigger_type) {
            //     case TT_VGA_VSYNC:
            //         x = 524; // capture just before the next VSYNC pulse
            //         break;
            //     case TT_VGA_RGB:
            //         x = 34; // capture just before the active phase
            //         break;
            //     case TT_VGA_VFRONT_PORCH:
            //         x = 480 + 33 + 2 - 1; // capture just before the vertical front porch phase
            //         break;
            // }

            // int x = 524; // number of pulses 

            // uint32_t expiry_time = time_us_32() + TRIGGER_TIMEOUT_US;

            x = 1; // number of pulses 

            switch (trigger_type) {
                case TT_VGA_CSYNC:
                    x = 524; // capture just before the next VSYNC pulse
                    break;
                case TT_VGA_CRGB:
                    x = 34; // capture just before the active phase
                    break;
                case TT_VGA_CVFRONT_PORCH:
                    x = 480 + 33 + 2 - 1; // capture just before the vertical front porch phase
                    break;
            }

            expiry_time = time_us_32() + TRIGGER_TIMEOUT_US;


            // if we get a low pulse on CSYNC that is greater than 96 clks / 25 MHz = 3.84 us then
            // chances are we have a "VSYNC" signal on CSYNC. It must also be less than 704 cls / 25 MHz = 28.16

            if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), expiry_time)) {
                // CSYNC is high
                // wait for CSYNC to go low
                while (1) {
                    if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), expiry_time)) {
                        // CSYNC is low
                        uint32_t csync_pulse_low_start_time = time_us_32();

                        // wait for CSYNC to go high again
                        if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), expiry_time)) {
                            // CSYNC is high again

                            uint32_t csync_pulse_low_duration = time_us_32() - csync_pulse_low_start_time;
                            
                            // csync_pulse_low_duration should be between 
                            
                            #define CSYNC_WIDTH_IN_MS 4
                            
                            // 3 fails - good
                            // 4, 5, 10, 25, 27, 28 works
                            // 29, 30, 35, 50, 100 fails - good
                            if (csync_pulse_low_duration > CSYNC_WIDTH_IN_MS) {
                                triggered = true; // assume pass
                                break;
                            }
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }

                if (triggered) {
                    // triggered = true; // assume pass
                    // Wait for x CSYNC (HSYNC) pulses before starting the capture
                    for (int i = 0; i < x; i++) {
                        if (pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(1, trigger_pin), expiry_time)) {
                            if (!pio_sm_exec_expiry_time_us(trigger_pio, trigger_sm, pio_encode_wait_gpio(0, trigger_pin), expiry_time)) {
                                triggered = false;
                                break;
                            }
                        } else {
                            triggered = false;
                            break;
                        }
                    }
                }
            }

            break;
    }

    if (!triggered) {
        // Failed to trigger, so restart the state machine otherwise it'll be stuck
        // in a latched EXEC instruction, and we'll never fill up the capture buffer.
        uart_puts(UART_ID, "failed to trigger, restarting the state machine...\n");
        pio_sm_restart(pio, sm);
    }

    // Let the capture state machine take over from here
    pio_sm_set_enabled(capture_pio, capture_sm, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    pio_sm_set_enabled(capture_pio, capture_sm, false); // Disable the state machine, which might save a bit of power? (todo - find out)

    g_no_of_captured_pins = g_no_of_pins_to_capture;

    clear_previous_edges();

    init_ir_rx(true);

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

// Colours for the filled rectangles

// #define LEFT_BOX_COLOR BLUE
// #define MIDDLE_BOX_COLOR RED
// #define RIGHT_BOX_COLOR MED_GREEN

#define LEFT_BOX_COLOR BLACK
#define MIDDLE_BOX_COLOR BLACK
#define RIGHT_BOX_COLOR BLACK

// Titlebar defines
#define TITLEBAR_TOP 1
#define TITLEBAR_LEFT 1
#define TITLEBAR_WIDTH SCREEN_WIDTH - (2 * TITLEBAR_LEFT)
#define TITLEBAR_HEIGHT 32


// Toolbar defines
// #define TOOLBAR_LEFT 1
// #define TOOLBAR_WIDTH SCREEN_WIDTH - (2 * TOOLBAR_LEFT)

#define TOOLBAR_LEFT 120
#define TOOLBAR_WIDTH SCREEN_WIDTH - TOOLBAR_LEFT - 1

#define TOOLBAR_HEIGHT 10
// #define TOOLBAR_TOP SCREEN_HEIGHT - TOOLBAR_HEIGHT - 1
// #define TOOLBAR_TOP TITLEBAR_TOP + TITLEBAR_HEIGHT
#define TOOLBAR_TOP (22 - TOOLBAR_HEIGHT)
#define TOOLBAR_TEXT_PADDING 1

// #define TOOLBAR_COLOR DARK_BLUE
#define TOOLBAR_COLOR BLACK
#define TOOLBAR_ITEM_PADDING 0

// #define TOOLBAR_HINT_WIDTH (TOOLBAR_WIDTH) / 3
// #define TOOLBAR_HINT_WIDTH 100
#define TOOLBAR_HINT_WIDTH 0


// #define CHANNEL_NO_LEFT (SCREEN_WIDTH / 2)

#define CHANNEL_NO_LEFT TOOLBAR_LEFT + TOOLBAR_TEXT_PADDING + TOOLBAR_HINT_WIDTH + TOOLBAR_TEXT_PADDING

#define CHANNEL_NO_WIDTH (FONT_WIDTH * (6 + 6)) + TOOLBAR_ITEM_PADDING

#define MAG_LEFT CHANNEL_NO_LEFT + CHANNEL_NO_WIDTH + TOOLBAR_ITEM_PADDING
#define MAG_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING


#define FREQ_LEFT MAG_LEFT + MAG_WIDTH + TOOLBAR_ITEM_PADDING
#define FREQ_WIDTH (FONT_WIDTH * 14) + TOOLBAR_ITEM_PADDING


// #define NO_OF_PINS_LEFT FREQ_LEFT + FREQ_WIDTH + TOOLBAR_ITEM_PADDING
// #define NO_OF_PINS_WIDTH (FONT_WIDTH * 8) + TOOLBAR_ITEM_PADDING

// #define PINS_BASE_LEFT NO_OF_PINS_LEFT + NO_OF_PINS_WIDTH + TOOLBAR_ITEM_PADDING
// #define PINS_BASE_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING



#define PINS_BASE_LEFT FREQ_LEFT + FREQ_WIDTH + TOOLBAR_ITEM_PADDING
#define PINS_BASE_WIDTH (FONT_WIDTH * 11) + TOOLBAR_ITEM_PADDING

#define NO_OF_PINS_LEFT PINS_BASE_LEFT + PINS_BASE_WIDTH + TOOLBAR_ITEM_PADDING
#define NO_OF_PINS_WIDTH (FONT_WIDTH * 9) + TOOLBAR_ITEM_PADDING

#define TRIGGER_BASE_LEFT NO_OF_PINS_LEFT + NO_OF_PINS_WIDTH + TOOLBAR_ITEM_PADDING
#define TRIGGER_BASE_WIDTH (FONT_WIDTH * (11 + 3)) + TOOLBAR_ITEM_PADDING

#define TRIGGER_TYPE_LEFT TRIGGER_BASE_LEFT + TRIGGER_BASE_WIDTH + TOOLBAR_ITEM_PADDING
#define TRIGGER_TYPE_WIDTH (FONT_WIDTH * 13) + TOOLBAR_ITEM_PADDING

#define PLOT_PADDING 4
#define PLOT_TOP (TOOLBAR_TOP + TOOLBAR_HEIGHT + 6)
// #define PLOT_HEIGHT 36

#define MAX_PLOT_HEIGHT 48

int plot_height;


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
#define MINIMAP_BOTTOM STATUSBAR_TOP - 1


#define STATUSBAR_HEIGHT 10
#define STATUSBAR_TOP (SCREEN_HEIGHT - STATUSBAR_HEIGHT - 1)
#define STATUSBAR_LEFT 1
#define STATUSBAR_WIDTH SCREEN_WIDTH - (2 * STATUSBAR_LEFT)

// #define STATUSBAR_TOP 55
#define STATUSBAR_TEXT_PADDING 1
// #define STATUSBAR_COLOR DARK_BLUE
#define STATUSBAR_COLOR BLACK
#define STATUSBAR_ITEM_PADDING 0

// #define STATUSBAR_HINT_WIDTH (TOOLBAR_WIDTH) / 3
#define STATUSBAR_HINT_LEFT STATUSBAR_LEFT + 2
// #define STATUSBAR_HINT_WIDTH STATUSBAR_WIDTH
#define STATUSBAR_HINT_WIDTH 400

#define STATUSBAR_HINT_COLOR STATUSBAR_COLOR

#define STATUSBAR_INFO_LEFT STATUSBAR_HINT_LEFT + STATUSBAR_HINT_WIDTH
#define STATUSBAR_INFO_WIDTH STATUSBAR_WIDTH - STATUSBAR_HINT_WIDTH - (2 * STATUSBAR_HINT_LEFT)
#define STATUSBAR_INFO_RIGHT STATUSBAR_INFO_LEFT + STATUSBAR_INFO_WIDTH
#define STATUSBAR_INFO_COLOR STATUSBAR_COLOR

char colours[] = {DARK_YELLOW, RED, ORANGE, YELLOW, GREEN, BLUE, MAGENTA, LIGHT_GREY_64, WHITE, DARK_GREY_64,
                  DARK_YELLOW, RED, ORANGE, YELLOW, GREEN, BLUE, /*MAGENTA,*/ LIGHT_GREY_64, /*WHITE,*/ DARK_GREY_64,
                  DARK_YELLOW, RED, ORANGE, YELLOW, GREEN, DARK_RED_64, DARK_RED_64, DARK_RED_64,BLUE, MAGENTA, LIGHT_GREY_64, WHITE, DARK_GREY_64,
                  DARK_YELLOW, RED, ORANGE, YELLOW, GREEN, BLUE, MAGENTA, LIGHT_GREY_64, WHITE, DARK_GREY_64};

#define MINIMAP_SCROLLBAR_HEIGHT 2
#define MINIMAP_SCROLLBAR_PADDING 1


int get_plot_padding() {
    if (g_no_of_captured_pins <= 16) {
        // return PLOT_PADDING;
        return 4;
    } else if (g_no_of_captured_pins <= 24) {
        return 3;
    } else {
        return 1;
    }
}


int get_minimap_height() {
    if (g_no_of_captured_pins <= 16) {
        // return MINIMAP_HEIGHT;
        return 6;
    } else if (g_no_of_captured_pins <= 24) {
        return 3;
    } else {
        return 2;
    }
}

int get_minimap_padding() {
    if (g_no_of_captured_pins <= 16) {
        // return MINIMAP_PADDING;
        return 1;
    } else if (g_no_of_captured_pins <= 24) {
        return 1;
    } else {
        return 1;
    }
}


int get_minimap_top(){
    // int minimap_height = MINIMAP_HEIGHT;
    // int minimap_padding = get_minimap_padding();
    // if (g_no_of_captured_pins > 16) {
    //     minimap_height = 3;
    // } else if (g_no_of_captured_pins > 24) {
    //     minimap_height = 2;
    // }
    // return TOOLBAR_TOP + ((get_minimap_height() + get_minimap_padding()) * g_no_of_captured_pins);
    return MINIMAP_BOTTOM - ((get_minimap_height() + get_minimap_padding()) * g_no_of_captured_pins);
}


int get_minimap_scrollbar_top() {
    return get_minimap_top() - (MINIMAP_SCROLLBAR_HEIGHT + MINIMAP_SCROLLBAR_PADDING);
}


/*
uint get_minimap_y() {
    int trace_height = MINIMAP_HEIGHT;
    int y_padding = MINIMAP_PADDING;
    if (g_no_of_captured_pins > 16) {
    // drawing the minimap
        trace_height = MINIMAP_HEIGHT >> 1;
        // y_padding = MINIMAP_PADDING;
    }
    // y = MINIMAP_TOP;
    // y = MINIMAP_BOTTOM - ((MINIMAP_HEIGHT + MINIMAP_PADDING) * pin_count);
    // y = MINIMAP_BOTTOM - ((trace_height + y_padding) * pin_count);

    return (MINIMAP_BOTTOM - ((trace_height + y_padding) * g_no_of_captured_pins) - (MINIMAP_SCROLLBAR_HEIGHT + MINIMAP_SCROLLBAR_PADDING));
}
*/

int get_plot_height(uint pin_count) {
    int avail_height = MINIMAP_BOTTOM - PLOT_TOP;
    // int fixed_height = ((PLOT_PADDING + MINIMAP_HEIGHT + MINIMAP_PADDING) * pin_count) + MINIMAP_SCROLLBAR_HEIGHT + (2 * MINIMAP_PADDING);

    // int fixed_height = ((PLOT_PADDING + MINIMAP_HEIGHT + MINIMAP_PADDING) * pin_count) + MINIMAP_SCROLLBAR_HEIGHT + (2 * MINIMAP_PADDING);

    // int minimap_height = MINIMAP_HEIGHT;
    // int minimap_padding = MINIMAP_PADDING;
    // if (g_no_of_captured_pins > 16) {
    // // drawing the minimap
    //     minimap_height = MINIMAP_HEIGHT >> 1;
    //     // minimap_padding = MINIMAP_PADDING;
    // }

    // int fixed_height = ((PLOT_PADDING + minimap_height + minimap_padding) * pin_count) + MINIMAP_SCROLLBAR_HEIGHT + (2 * MINIMAP_PADDING);
    // int fixed_height = (PLOT_PADDING * pin_count) + get_minimap_top(0) + MINIMAP_SCROLLBAR_HEIGHT + (2 * MINIMAP_PADDING);

    int fixed_height = ((get_plot_padding() + get_minimap_height() + get_minimap_padding()) * pin_count) + MINIMAP_SCROLLBAR_HEIGHT + (2 * MINIMAP_PADDING);

    plot_height = MIN((avail_height - fixed_height) / pin_count, MAX_PLOT_HEIGHT);
    return plot_height;
}


void set_plot_line_colors(uint pin_count) {
    int trace_height = get_plot_height(g_no_of_captured_pins);
    int y_padding = get_plot_padding();
    int y = PLOT_TOP;

    for (int pin = 0; pin < pin_count; ++pin) {

        char line_col = colours[pin];

        // char back_col = BLACK;
        // if (pin == g_channel) {
        //     back_col = DARK_GREY_64;
        // }

        for (uint16_t l = 0; l < trace_height; l++) {
            set_line_colors(y + l,  BLACK, line_col, 0, 0);
        }

        y += trace_height + y_padding;
    }


    y = get_minimap_scrollbar_top();
    for (int i = 0; i < MINIMAP_SCROLLBAR_HEIGHT; i++) {
        set_line_colors(y + i, BLACK, WHITE, 0, 0);
    }

    y = get_minimap_top();
    int minimap_height = get_minimap_height();
    int minimap_padding = get_minimap_padding();
    for (int pin = 0; pin < pin_count; ++pin) {
        char line_col = colours[pin];
        for (int i = 0; i < minimap_height; i++) {
            set_line_colors(y + i, BLACK, line_col, 0, 0);
        }
        y+= minimap_height + minimap_padding;
    }
}


// Returns the number of characters a uint would take if printed as ascii (with no separators).
uint8_t uint_width(uint32_t num) {
    uint8_t width = 0;
    do {
        width += 1;
        num = num / 10;
    } while (num);
    return width;
}


void plot_capture_buf(const uint32_t *buf, uint pin_base, uint pin_count, uint32_t n_samples, int magnification,
                        int scrollx, bool show_numbers) {
    // Display the capture buffer in graphical form, like this:
    // 00: __--__--__--__--__--__--
    // 01: ____----____----____----
    // ...only with lines
    // uart_puts(UART_ID, "Capture:\n");
    // Each FIFO record may be only partially filled with bits, depending on
    // whether pin_count is a factor of 32.

    // Define a structure for pulse width values
    struct PulseWidth {
        int16_t x;
        int32_t value;
    };

    struct PulseWidth pws[64];
    uint8_t no_of_pws;

    // 20
    #define PIXEL_WORDS_PER_LINE 640 / 32

    uint32_t top_pixels [PIXEL_WORDS_PER_LINE];
    uint32_t mid_pixels [PIXEL_WORDS_PER_LINE];
    uint32_t bot_pixels [PIXEL_WORDS_PER_LINE];

    uint record_size_bits = bits_packed_per_word(pin_count);

    int trace_height = plot_height;
    int y_padding = get_plot_padding();
    int y = PLOT_TOP;

    int max_screen_x = MIN(scrollx + mag_factor(SCREEN_WIDTH), n_samples);


    if (!show_numbers) {
        trace_height = get_minimap_height();
        y_padding = get_minimap_padding();
        y = get_minimap_top();

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

// #define DEBUG_PINS

#ifdef DEBUG_PINS

#define FIRST_PIN_TO_DEBUG 2
#define LAST_PIN_TO_DEBUG 2

    void print_debug(uint8_t pin) {
        char str[256];
        sprintf(str, "pin: %d scrollx: %d, prev start: %d prev_end: %d next start: %d next end: %d\n", 
            pin, scrollx, edges[pin].prev_start, edges[pin].prev_end, edges[pin].next_start, edges[pin].next_end);
        uart_puts(UART_ID, str);
    }

#endif

    char str[80];

    setTextSize(1);

#ifdef DEBUG_PINS
    uart_puts(UART_ID, "-\n");
#endif

    for (int pin = 0; pin < pin_count; ++pin) {

        no_of_pws = 0;

        int x = 0;

        uint magIndex = 0;

        char line_col = colours[pin];

        int last_sample = 1; // fix at high for now
        int last_x = 0;

        setTextColor(line_col);

        int last_i = 0;
        int cursor_x = 0;

        bool use_prev_start = false;

#ifdef DEBUG_PINS
        if ((pin >= FIRST_PIN_TO_DEBUG) && (pin <= LAST_PIN_TO_DEBUG)) {
            print_debug(pin);
        }
#endif

        int safe_scrollx = scrollx;

        if (safe_scrollx >= n_samples) {
            safe_scrollx = n_samples - 1;
        }

        if (scrollx >= edges[pin].prev_start) {
            if ((max_screen_x < edges[pin].next_end) || (edges[pin].next_end == n_samples)) {
                if (edges[pin].prev_end == -1) {
                    use_prev_start = true;
                } else if (scrollx < edges[pin].prev_end) {
                    use_prev_start = true;
                }
            }
        }

        last_sample = get_channel_sample(buf, pin, pin_count, safe_scrollx, record_size_bits);

        if (use_prev_start) {
            // we can use the previously searched-for prev_start edge
            last_i = edges[pin].prev_start;

        } else {
            // we can not use the previous searched-for edge, so search for it

#ifdef DEBUG_PINS
            if ((pin >= FIRST_PIN_TO_DEBUG) && (pin <= LAST_PIN_TO_DEBUG)) {
                sprintf(str, "finding prev edge on pin %d at scrollx: %d...\n", pin, scrollx);
                uart_puts(UART_ID, str);
            }
#endif

            last_i = 0;

            for (int i = safe_scrollx - 1; i >= 0; i--) {
                // go back from the current scroll position and find the first
                // sample that's different from the sample at the scroll position.

                uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);

                if (last_sample != sample) {
                    last_i = i + 1; // the "+ 1" is needed as we're going backwards
                    break;
                }
            }

            edges[pin].prev_start = last_i; // save the previous start edge for next time

#ifdef DEBUG_PINS
            if ((pin >= FIRST_PIN_TO_DEBUG) && (pin <= LAST_PIN_TO_DEBUG)) {
                print_debug(pin);
            }
#endif

        }

        // last_i is now the index of the previous off-screen edge, or the beginning of the samples

        // make any needed scale adjustments

        if (magnification < 0) {
            last_x = ((last_i - scrollx) / (abs(magnification) + 1));
        } else {
            last_x = ((last_i - scrollx) * (magnification + 1));
        }

        // get ready to paint the relevant samples on the screen

        int last_pixel_x = -1;
        int last_v_line_x = -1;

        x = 0;

        char debug1 = 0;

        int prev_index = MAX(0, scrollx - 1);
        last_sample = get_channel_sample(buf, pin, pin_count, prev_index, record_size_bits);

        bool found_prev_end = false;

        int prev_end = -1;

        int next_start = -1;

        // fillRect(0, y, SCREEN_WIDTH, (trace_height), BLACK); // clear plot area

        // clear the buffered top, middle and bottom lines of the plot
        memset(&top_pixels[0], 0, PIXEL_WORDS_PER_LINE * sizeof(uint32_t));
        memset(&mid_pixels[0], 0, PIXEL_WORDS_PER_LINE * sizeof(uint32_t));
        memset(&bot_pixels[0], 0, PIXEL_WORDS_PER_LINE * sizeof(uint32_t));

        // paint the windowed part of the plot

        for (int i = scrollx; i < max_screen_x; i++) {

            uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);

            if ((last_sample != sample))  {

                if ((!found_prev_end) && (i != scrollx)) {
                    prev_end = i;
                    found_prev_end = true;
                }

                next_start = i;

                if (x != last_v_line_x) {
                    // We've not yet drawn a vertical line at this x location, so let's draw one.
                    // drawVLine(x, y, trace_height, line_col);

                    // Or rather, let's buffer it now, and memcpy it later
                    // calculate the word index (0 - 19)
                    int wi = (x / 32);
                    
                    // calculate the bit index (0 - 31) and then the or mask 
                    int or_mask = 1 << (x & 0x1f);
                    
                    top_pixels[wi] |= (or_mask);
                    mid_pixels[wi] |= (or_mask);
                    bot_pixels[wi] |= (or_mask);

                    last_v_line_x = x;
                    last_pixel_x = x;

                    if (show_numbers) {
                        // sprintf(str,"%d", i - last_i);
                        // int str_width = strlen(str) * FONT_WIDTH;

                        int str_width =  uint_width(i - last_i) * FONT_WIDTH;

                        if (str_width < (x - last_x)) {
                            // the string fits between the two edges

                            int x_offset = ((x - last_x - str_width) / 2) + 1;
                            if (x_offset < 2) {
                                x_offset = 2;
                            }
                            cursor_x = last_x +  x_offset;

                            if (cursor_x < 0) {
                                cursor_x = 0;
                                if (cursor_x + str_width >= x) {
                                    cursor_x = x - str_width;
                                }
                            }

                            // if (cursor_x <= last_x) {
                            //     cursor_x = last_x + 1;
                            // }

                            if (cursor_x - str_width < SCREEN_WIDTH) {
                                // setCursor(cursor_x, y + (trace_height / 2) - (FONT_HEIGHT / 2));
                                // writeString(str);

                                // buffer the value and draw it later
                                pws[no_of_pws].x = cursor_x;
                                pws[no_of_pws].value = i - last_i;
                                no_of_pws++;
                            }
                        }
                    }
                }
                last_sample = sample;
                last_x = x;
                last_i = i;

            } else {
                if (x != last_pixel_x) {
                    // We've not yet drawn a pixel at this x location, so let's draw one.
                    // drawPixel(x, y + (sample ? 0 : trace_height - 1), line_col);

                    // calculate the word index 0 - 19
                    int wi = (x / 32);
                    
                    // calculate the bit index (0 - 31) and then the or mask 
                    int or_mask = 1 << (x & 0x1f);
                    
                    if (sample) {
                        top_pixels[wi] |= or_mask;
                    } else {
                        bot_pixels[wi] |= or_mask;
                    }

                    last_pixel_x = x;
                }
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

         if (show_numbers) {

            int i;

            bool use_next_edge = false;

            if ((use_prev_start == false) && (prev_end == -1)) {

            } else {
                if (max_screen_x <= edges[pin].next_end) {
                    if (next_start == edges[pin].next_start) {
                        use_next_edge = true;
                    }
                }
            }

            edges[pin].prev_end = prev_end;
            edges[pin].next_start = next_start;

            if (use_next_edge) {

                if (next_start == -1) {
                    // edges[pin].prev_start should also be -1
                    // we didn't find any edges in the windowed area 
                    last_i = edges[pin].prev_start;
                } else {
                    last_i = next_start;
                }

                i = edges[pin].next_end;
            } else {

#ifdef DEBUG_PINS
                if ((pin >= FIRST_PIN_TO_DEBUG) && (pin <= LAST_PIN_TO_DEBUG)) {
                    sprintf(str, "finding next edge on pin %d at scrollx: %d...\n", pin, scrollx);
                    uart_puts(UART_ID, str);
                }
#endif

                for (i = max_screen_x; i < n_samples; i++) {
                    uint sample = get_channel_sample(buf, pin, pin_count, i, record_size_bits);
                    if (last_sample != sample) {
                        if (!found_prev_end) {
                            edges[pin].prev_end = i;
                            found_prev_end = true;
                        }
                        edges[pin].next_end = i;
                        break;
                    }
                }

                edges[pin].next_end = i;
            }

            if (magnification < 0) {
                x = ((i - scrollx) / (abs(magnification) + 1));
            } else {
                x = ((i - scrollx) * (magnification + 1));
            }

            // sprintf(str,"%d", i - last_i);
            // int str_width = strlen(str) * FONT_WIDTH;

            int str_width =  uint_width(i - last_i) * FONT_WIDTH;

/*
            if (str_width < (x - last_x)) {
                // the string fits between the two edges
                cursor_x = last_x + ((x - last_x - str_width) / 2) + 1;

                if (cursor_x < 0) {
                    cursor_x = 0;
                    if (cursor_x + str_width >= x) {
                        cursor_x = x - str_width;
                    }
                }

                if (cursor_x - str_width < SCREEN_WIDTH) {
                    setCursor(cursor_x, y + (trace_height / 2) - (FONT_HEIGHT / 2));
                    writeString(str);
                }
            }
*/

            if (str_width < (x - last_x)) {

                cursor_x = last_x + ((x - last_x - str_width) / 2) + 1;
                if (cursor_x + str_width >= SCREEN_WIDTH) {
                    // some of the string is off screen and to the right
                    cursor_x = last_x + 2;
                    if (cursor_x + str_width < SCREEN_WIDTH) {
                        cursor_x = SCREEN_WIDTH - str_width - 1;
                    }

                } else if (cursor_x < 0) {
                    cursor_x = 0;
                }
                // else if (cursor_x <= last_x) {
                //     cursor_x = last_x + 1;
                // } 
                // setCursor(cursor_x, y + (trace_height / 2) - (FONT_HEIGHT / 2));
                // writeString(str);

                pws[no_of_pws].x = cursor_x;
                pws[no_of_pws].value = i - last_i;
                no_of_pws++;
            }
        }

        // copy the buffer top, all the middle and the bottom lines in quick succession 
        memcpy(&vga_1bit_data_array[(y * WORDS_PER_LINE) + 1], &top_pixels[0], PIXEL_WORDS_PER_LINE * sizeof(uint32_t));
        
        for (int i = 1; i < trace_height - 1; i++) {    
            memcpy(&vga_1bit_data_array[((y + i) * WORDS_PER_LINE) + 1], &mid_pixels[0], PIXEL_WORDS_PER_LINE * sizeof(uint32_t));
        }

        memcpy(&vga_1bit_data_array[((y + trace_height - 1) * WORDS_PER_LINE) + 1], &bot_pixels[0], PIXEL_WORDS_PER_LINE * sizeof(uint32_t));

        if (show_numbers) {
            // calculate the y position for the text
            uint16_t cursor_y = y + ((trace_height - FONT_HEIGHT) / 2);
            
            // draw all the buffered pulse widths
            for (int i = 0; i < no_of_pws; i++) {
                setCursor(pws[i].x, cursor_y);
                sprintf(str,"%d", pws[i].value);
                writeString(str);
            }
        }

        y += trace_height + y_padding;
    }

    g_prev_scrollx = scrollx;

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


uint32_t last_demo_time;

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
    UIC_ANY,
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
    UIC_A,

#if USE_DVI
    UIC_V,
    UIC_D,
    UIC_R,
    UIC_S,
#endif

    UIC_SPACEBAR,
    UIC_UP,
    UIC_DOWN,
    UIC_PAGE_UP,
    UIC_PAGE_DOWN,
    UIC_0,
    UIC_TAB,
    UIC_SHIFT_TAB,
    UIC_ESC
 };


// #define HINT_LEFT STATUSBAR_LEFT + 1

void draw_hint(char *s) {
    fillRect(STATUSBAR_HINT_LEFT, STATUSBAR_TOP, STATUSBAR_HINT_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_HINT_COLOR);
    setCursor(STATUSBAR_HINT_LEFT, STATUSBAR_TOP + STATUSBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
    writeString(s);
}


void draw_setting_helper(uint left, uint8_t label_len, uint8_t str_len) {
    fillRect(left + (FONT_WIDTH * label_len), TOOLBAR_TOP, (FONT_WIDTH * str_len), FONT_HEIGHT + 1, TOOLBAR_COLOR);
    setCursor(left + (FONT_WIDTH * label_len), TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}


void draw_channel_no() {
    draw_setting_helper(CHANNEL_NO_LEFT, 2 + 5, 4);
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
    draw_setting_helper(NO_OF_PINS_LEFT, 4, 5);
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
    draw_setting_helper(FREQ_LEFT, 8, 5);
    if (settings_state == SS_FREQ) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "fdiv: %d\n", g_sample_frequency);
    }
    write_intf(" %d ", g_sample_frequency);
}


void draw_trigger_pin_base() {
    draw_setting_helper(TRIGGER_BASE_LEFT, 7, 6);
    if (settings_state == SS_TRIGGER_PIN_BASE) {
        setTextColor2(TOOLBAR_COLOR, WHITE);
        uart_putcf(UART_ID, "tpin: %d\n", g_trigger_pin_base);
    }
    write_intf(" GP%d ", g_trigger_pin_base);
}


void draw_trigger_type() {

// enum TRIGGER_TYPES {TT_NONE, TT_LOW_LEVEL, TT_HIGH_LEVEL, TT_RISING_EDGE, TT_FALLING_EDGE, TT_ANY_EDGE, TT_VGA_VSYNC, TT_VGA_RGB, TT_VGA_VFRONT_PORCH, TT_COUNT};

    unsigned char tt_chars[TT_COUNT][8] = {" NONE ", " LOW ", " HIGH ", " RISE ", " FALL ", " EDGE ", " VSYNC ", " RGB ", " VFPOR ", " CSYNC ",  " CRGB ", " CFPOR "};
    draw_setting_helper(TRIGGER_TYPE_LEFT, 4, 7);
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


void draw_titlebar() {
    // fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_WIDTH, TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    fillRect(TITLEBAR_LEFT, TITLEBAR_TOP, TITLEBAR_WIDTH, TITLEBAR_HEIGHT, BLACK);
    setTextColor(WHITE);
    setCursor(TITLEBAR_LEFT + ((TITLEBAR_WIDTH - (6 * FONT_WIDTH)) / 2), TITLEBAR_TOP + 1);
    setTextSize(1);
    // writeString("palavo");
}


void draw_toolbar() {
    // fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_WIDTH, TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_WIDTH, TOOLBAR_HEIGHT, BLACK);
    setTextColor(WHITE);
    setCursor(CHANNEL_NO_LEFT, TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextSize(1);
    writeString("channel     zoom       freq.div      base       pins     trigger       type");
    draw_settings();
}


void set_toolbar_text() {
    setCursor(TOOLBAR_LEFT + TOOLBAR_TEXT_PADDING, TOOLBAR_TOP + TOOLBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}


void clear_toolbar_hint() {
    set_toolbar_text();
    // fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_HINT_WIDTH, TOOLBAR_HEIGHT, TOOLBAR_COLOR);
    fillRect(TOOLBAR_LEFT, TOOLBAR_TOP, TOOLBAR_HINT_WIDTH, TOOLBAR_HEIGHT, BLACK);
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
    // set_statusbar_text();
    fillRect(STATUSBAR_HINT_LEFT, STATUSBAR_TOP, STATUSBAR_HINT_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_HINT_COLOR);
    setCursor(STATUSBAR_HINT_LEFT, STATUSBAR_TOP + STATUSBAR_TEXT_PADDING);
    setTextColor(WHITE);
    setTextSize(1);
}

void clear_statusbar_info() {
    fillRect(STATUSBAR_INFO_LEFT, STATUSBAR_TOP, STATUSBAR_INFO_WIDTH, STATUSBAR_HEIGHT, STATUSBAR_INFO_COLOR);
    // draw_settings();
    // setCursor(STATUSBAR_INFO_LEFT + 1, SCREEN_HEIGHT - 1 - 8);
    setTextColor(WHITE);
    setTextSize(1);
}

uint check_keyboard() {

    uint ui_command = UIC_NONE;

    if ((last_uart_char == 27) || uart_is_readable(UART_ID)) {
        ui_command = UIC_ANY;

        clear_statusbar_hint();
        // set_statusbar_text();
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
                // writeString("esc");
                last_uart_char = 0;
                ui_command = UIC_ESC;
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

         } else {
            switch ((char)ch) {

                case ' ':
                    ui_command = UIC_SPACEBAR;
                    break;

                case '-':
                    ui_command = UIC_MINUS;
                    break;

                case '+' :
                    ui_command = UIC_PLUS;
                    break;

                case '=':
                    ui_command = UIC_EQUALS;
                    break;

                case 'c':
                // case 'C':
                    ui_command = UIC_C;
                    break;

                case 'z':
                // case 'Z':
                    ui_command = UIC_Z;
                    break;

                case 'm':
                // case 'M':
                    ui_command = UIC_M;
                    break;

                case '0':
                    ui_command = UIC_0;
                    break;

                case 'h':
                // case 'H':
                    ui_command = UIC_H;
                    break;

                case 'a':
                // case 'A':
                    ui_command = UIC_A;
                    break;

#if USE_DVI
                case 'v':
                    ui_command = UIC_V;
                    break;

                case 'd':
                    ui_command = UIC_D;
                    break;

                case 'r':
                    ui_command = UIC_R;
                    break;

                case 's':
                    ui_command = UIC_S;
                    break;
#endif

                case '\t':
                    ui_command = UIC_TAB;
                    break;
        
            }
         }
    }
    return ui_command;
}


void draw_minimap_indicator() {

    static int prev_mini_x = 0;
    static int prev_mini_w = 0;

    int mini_x = (g_scrollx * SCREEN_WIDTH) / g_capture_n_samples;
    int mini_w = (mag_factor(SCREEN_WIDTH * SCREEN_WIDTH) / g_capture_n_samples) + 1; // add one to round up and/or ensure a visible indicator

    uint y = get_minimap_scrollbar_top();

    fillRect(prev_mini_x, y, prev_mini_w, MINIMAP_SCROLLBAR_HEIGHT, BLACK);
    fillRect(mini_x, y, mini_w, MINIMAP_SCROLLBAR_HEIGHT, WHITE);

    prev_mini_x = mini_x;
    prev_mini_w = mini_w;

    // uart_putcf(UART_ID, "mini_x: %d; ", mini_x);
    // uart_putcf(UART_ID, "mini_w: %d\n", mini_w);
}


void draw_statusbar_info(){
    char str[80];
    clear_statusbar_info();
    // write_intf("x:%d ", g_scrollx);
    // write_intf("was:%d ", g_prev_scrollx);
    // write_intf("diff:%d", g_scrollx - g_prev_scrollx);

    // int diff = g_scrollx - g_prev_scrollx;

    sprintf(str, "x:%d prev:%d diff:%d", g_scrollx, g_prev_scrollx, g_scrollx - g_prev_scrollx);
    setCursor(STATUSBAR_INFO_RIGHT - (strlen(str) * FONT_WIDTH), STATUSBAR_TOP + STATUSBAR_TEXT_PADDING);
    writeString(str);
}


bool set_scroll_x(int x) {
    bool changed = x != g_scrollx;
    if (changed) {
        g_scrollx = x;
        draw_statusbar_info();
        // g_prev_scrollx = x;
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
    "Press...\n"
    "\n"
    "LEFT / RIGHT to scroll one sample period left / right\n"
    "CTRL-LEFT / CTRL-RIGHT to scroll to previous / next edge\n"
    "  on the selected channel (ch)\n"
    "PGUP / PGDN to scroll one page left / right\n"
    "HOME / END to scroll to the beginning / end\n"
    "\n"
    "TAB / SHIFT-TAB to select the next / previous setting\n"
    "UP / DOWN to increase / decrease the selected setting\n"
    "c to capture a sample using the settings\n"
    "z to zoom to fit all the samples on one page\n"
    "+ / - to zoom in / out\n"
    "= to set zoom to 1:1\n"
    "m to measure VGA timings\n"
    "h to show this help window\n"
    "a to show the about window\n"
#if USE_DVI
    "v to cycle DVI modes: mirror VGA out -> test -> VGA in\n"
#endif
    "SPACE to play / pause graphics demo\n";

char* press_any_key_string = 
    "Press any key to close this window\n";


bool showing_window = false;

#define HELP_WINDOW_PADDING 2
#define HELP_WINDOW_WIDTH (56 + 2) * FONT_WIDTH
#if USE_DVI
#define HELP_WINDOW_HEIGHT (21 + 2) * (FONT_HEIGHT + HELP_WINDOW_PADDING)
#else
#define HELP_WINDOW_HEIGHT (20 + 2) * (FONT_HEIGHT + HELP_WINDOW_PADDING)
#endif
#define HELP_WINDOW_TOP (SCREEN_HEIGHT - HELP_WINDOW_HEIGHT) / 2
#define HELP_WINDOW_LEFT (SCREEN_WIDTH - HELP_WINDOW_WIDTH) / 2 


void show_help_window() {
    fillRect(HELP_WINDOW_LEFT, HELP_WINDOW_TOP, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT, LIGHT_BLUE);

    // for (int y = 0; y < HELP_WINDOW_HEIGHT; y++) {
    //     set_line_colors(HELP_WINDOW_TOP + y, BLACK, LIGHT_BLUE, 0, 0);
    // }

    for (int y = 0; y <= SCREEN_HEIGHT; y++) {
        // set_line_colors(y, BLACK, LIGHT_BLUE, 0, 0);
        set_line_colors(y, BLACK, WHITE, 0, 0);
    }

    setTextSize(1);
    setTextColor(BLACK);
    setCursor(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + FONT_HEIGHT + HELP_WINDOW_PADDING);
    set_text_padding(HELP_WINDOW_PADDING);
    writeString(help_strings);
    // writeString(press_any_key_string);

    setCursor(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + HELP_WINDOW_HEIGHT - (2 * (FONT_HEIGHT + HELP_WINDOW_PADDING)));
    writeString(press_any_key_string);

    uart_puts(UART_ID, "\nHELP\n\n");
    uart_puts(UART_ID, help_strings);
    uart_puts(UART_ID, press_any_key_string);
    uart_puts(UART_ID, "\n");

    showing_window = true;
}


void logo(int x, int y, bool use_fore_col) {

    #define LOGO_THICKNESS 4
    #define LOGO_WIDTH 16
    // #define LOGO_HEIGHT

    // bool back_col = colour 

    void logo_o(int x, int y) {
        fillCircle(x + (LOGO_WIDTH / 2), y + (LOGO_WIDTH / 2) + 4 - 1, 8, use_fore_col);
        fillCircle(x + (LOGO_WIDTH / 2), y + (LOGO_WIDTH / 2) + 4 - 1, 8 - LOGO_THICKNESS, !use_fore_col);
    }

    void logo_a(int x, int y) {
        logo_o(x, y);
        fillRect(x + LOGO_WIDTH - LOGO_THICKNESS + 1, y + (LOGO_WIDTH / 2) + 4 - 1, LOGO_THICKNESS, 9, use_fore_col);
    }

    // p

    logo_o(x, y);
    fillRect(x, y + (LOGO_WIDTH / 2) + 4 - 1, LOGO_THICKNESS, 12, use_fore_col);

    // x is 61

    x += 16;

    // x is 77


    // a
    logo_a(x + 3, y);
    x += 3 + 16;

    // x is 96

    // l
    // fillRect(x + 4, y - 7 - 4, LOGO_THICKNESS, 20, use_fore_col);
    fillRect(x + 4, y, LOGO_THICKNESS, 20, use_fore_col);

    x += 4;

    // x is 100

    // t
    // fillRect(96 + 8 + 1, 20 - 7 - 4, 4, 20, WHITE);
    // fillRect(96 + 8 + 1 + 4, 20 - 7 , 4, 4, WHITE);

    // a

    logo_a(x + 6, y);

    x += 6 + 16;

    // x is 122

    // v

    for (int i = 0; i < LOGO_THICKNESS; i++) {
        // drawLine(O_X + V_SHIFT_X + 10 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + 1 + i, y + 3, x + 1 + 6 + i, y + 3 + 16, use_fore_col);
        // drawLine(O_X + V_SHIFT_X + 10 + 12 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + 12 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + 1 + 12 + i, y + 3, x + 1 + 6 + i, y + 3 + 16, use_fore_col);
    }

    x += 1 + 12; 

    // x is 135

    // o
    logo_o(x + 2 + 2, y);

}



void logo_small(int x, int y, bool use_fore_col) {

    #define LOGO_SMALL_THICKNESS 2
    #define LOGO_SMALL_WIDTH 8

    #define LOGO_SMALL_DIAMETER 4



    // #define LOGO_HEIGHT

    // bool back_col = colour 

    void logo_o(int x, int y) {
        fillCircle(x + (LOGO_SMALL_WIDTH / 2), y + (LOGO_SMALL_WIDTH / 2) + 2 - 1, LOGO_SMALL_DIAMETER, use_fore_col);
        fillCircle(x + (LOGO_SMALL_WIDTH / 2), y + (LOGO_SMALL_WIDTH / 2) + 2 - 1, LOGO_SMALL_DIAMETER - LOGO_SMALL_THICKNESS, !use_fore_col);
    }

    void logo_a(int x, int y) {
        logo_o(x, y);
        fillRect(x + LOGO_SMALL_WIDTH - LOGO_SMALL_THICKNESS + 1, y + (LOGO_SMALL_WIDTH / 2) + 2 - 1, LOGO_SMALL_THICKNESS, LOGO_SMALL_DIAMETER + 1, use_fore_col);
    }

    // p

    logo_o(x, y);
    fillRect(x, y + (LOGO_SMALL_WIDTH / 2) + 2 - 1, LOGO_SMALL_THICKNESS, LOGO_SMALL_DIAMETER + 1 + 2, use_fore_col);

    // x is 61

    x += 7;

    // x is 77


    // a
    logo_a(x + 3, y);
    x += 2 + 8;

    // x is 96

    // l
    // fillRect(x + 4, y - 7 - 4, LOGO_SMALL_THICKNESS, 20, use_fore_col);
    fillRect(x + 4, y, LOGO_SMALL_THICKNESS, LOGO_SMALL_WIDTH + 2, use_fore_col);

    x += 7;

    // x is 100

    // t
    // fillRect(96 + 8 + 1, 20 - 7 - 4, 4, 20, WHITE);
    // fillRect(96 + 8 + 1 + 4, 20 - 7 , 4, 4, WHITE);

    // a

    logo_a(x, y);

    x += 8;

    // x is 122

    // v

    for (int i = 0; i < LOGO_SMALL_THICKNESS; i++) {
        // drawLine(O_X + V_SHIFT_X + 10 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + 1 + i, y + 1, x + 1 + 3 + i, y + 1 + 8, use_fore_col);
        // drawLine(O_X + V_SHIFT_X + 10 + 12 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + 12 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + 1 + 6 + i, y + 1, x + 1 + 3 + i, y + 1 + 8, use_fore_col);
    }

    x += 6 - 1; 

    // x is 135

    // o
    logo_o(x + 2 + 2, y);

}



void logo_med(int x, int y, bool use_fore_col) {

    
    #define LOGO_MED_ASC 3
    #define LOGO_MED_DES 3
    
    #define LOGO_MED_THICKNESS 4
    #define LOGO_MED_WIDTH 14

    #define LOGO_MED_RADIUS 7



    // #define LOGO_HEIGHT

    // bool back_col = colour 

    void logo_o(int x, int y) {
        fillCircle(x + LOGO_MED_RADIUS, y + LOGO_MED_ASC + LOGO_MED_RADIUS - 1, LOGO_MED_RADIUS, use_fore_col);
        fillCircle(x + LOGO_MED_RADIUS, y + LOGO_MED_ASC + LOGO_MED_RADIUS - 1, LOGO_MED_RADIUS - LOGO_MED_THICKNESS, !use_fore_col);
    }

    void logo_a(int x, int y) {
        logo_o(x, y);
        fillRect(x + LOGO_MED_WIDTH - LOGO_MED_THICKNESS + 1, y + LOGO_MED_ASC + LOGO_MED_RADIUS, LOGO_MED_THICKNESS, LOGO_MED_RADIUS, use_fore_col);
    }

    // p

    logo_o(x, y);
    fillRect(x, y + LOGO_MED_ASC + LOGO_MED_RADIUS, LOGO_MED_THICKNESS, LOGO_MED_RADIUS + LOGO_MED_DES, use_fore_col);

    // x is 61

    x += 7 + 7 + 3;

    // x is 77


    // a
    logo_a(x, y);
    x += LOGO_MED_WIDTH + 4;

    // x is 96

    // l
    // fillRect(x + 4, y - 7 - 4, LOGO_MED_THICKNESS, 20, use_fore_col);
    fillRect(x, y, LOGO_MED_THICKNESS, LOGO_MED_ASC + LOGO_MED_WIDTH, use_fore_col);

    x += LOGO_MED_THICKNESS + 2;

    // x is 100

    // t
    // fillRect(96 + 8 + 1, 20 - 7 - 4, 4, 20, WHITE);
    // fillRect(96 + 8 + 1 + 4, 20 - 7 , 4, 4, WHITE);

    // a

    logo_a(x, y);

    x += (LOGO_MED_WIDTH) + 1;

    // x is 122

    // v

    for (int i = 0; i < LOGO_MED_THICKNESS; i++) {
        // drawLine(O_X + V_SHIFT_X + 10 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + i, y + LOGO_MED_ASC - 1, x + 5 + i, y + LOGO_MED_ASC - 1 + (LOGO_MED_WIDTH), use_fore_col);
        // drawLine(O_X + V_SHIFT_X + 10 + 12 + i, LOGO_Y_CENTRE - 8, O_X + V_SHIFT_X + 16 + i, LOGO_Y_CENTRE + 8, WHITE);
        // drawLine(x + 1 + 12 + i, y - 8, x + 1 + 6 + i, y + 8, use_fore_col);
        drawLine(x + 10 + i, y + LOGO_MED_ASC - 1, x + 5 + i, y + LOGO_MED_ASC - 1 + (LOGO_MED_WIDTH), use_fore_col);
    }

    x += 6 + 1 + 7; 

    // x is 135

    // o
    logo_o(x, y);

}



char* name_string = "PALAVO";

char* about_strings =
    "PIO-Assisted Logic Analyser with VGA Output\n"
    "Version 0.0.1\n"
    "\n"
    "Developed by Peter Stansfeld.\n"
    "\n"
    "\n"
    // "Inspired by and using code from Hunter Adams's\n"
    // "Graphics Primitives demo with Bruce Land's 4-bit mod\n"
    // "and Raspberry Pi's Logic Analyser example for the Pico.\n";

    "Inspired by and using code from:\n"
    "\n"
    "* Raspberry Pi's Logic Analyser example for the Pico.\n"
    "\n"
    "* Hunter Adams's Graphics Primitives demo with Bruce\n"
    "  Land's 4-bit mod.\n";


    // "vha3@cornell.edu\n"
    // "4-bit mod by Bruce Land\n";

void show_about_window() {
    for (int y = 0; y <= SCREEN_HEIGHT; y++) {
        // set_line_colors(y, BLACK, LIGHT_BLUE, 0, 0);
        set_line_colors(y, BLACK, WHITE, 0, 0);
    }

    fillRect(HELP_WINDOW_LEFT, HELP_WINDOW_TOP, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT, LIGHT_BLUE);

    // logo(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + 20 - 11, false);
    logo_med(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + 20 - 11, false);

    setTextSize(1);
    setTextColor(BLACK);

    // setCursor(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + FONT_HEIGHT + HELP_WINDOW_PADDING + );
    setCursor(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + 20 + 8 + 4 + 4);
    set_text_padding(HELP_WINDOW_PADDING);
    writeString(about_strings);
    // writeString(press_any_key_string);

    setCursor(HELP_WINDOW_LEFT + FONT_WIDTH, HELP_WINDOW_TOP + HELP_WINDOW_HEIGHT - (2 * (FONT_HEIGHT + HELP_WINDOW_PADDING)));
    writeString(press_any_key_string);

    uart_puts(UART_ID, "\nABOUT\n\nPALAVO\n");
    uart_puts(UART_ID, about_strings);
    uart_puts(UART_ID, press_any_key_string);
    uart_puts(UART_ID, "\n");

    showing_window = true;
}


void init_line_colours() {
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        // vga_1bit_data_array[y * WORDS_PER_LINE] = (((BLUE << 4) | (y & 0x0f)) << 16) | (639);
        char fore_colour;
        char back_colour;
        
        if (((y >= 0) && (y <= TOOLBAR_TOP + TOOLBAR_HEIGHT)) || (y >= STATUSBAR_TOP)) {
            fore_colour = WHITE;
            back_colour = BLACK;
            
        } else {
            fore_colour = WHITE;
            back_colour = BLACK;
        }

        // vga_1bit_data_array[y * WORDS_PER_LINE] = (((fore_colour << 4) | (back_colour)) << 16) | (639);
        set_line_colors(y, back_colour, fore_colour, WHITE, LIGHT_BLUE);
    }
}

#if USE_DVI

void mirror_VGA_data_to_DVI() {

    uint32_t *vga_framebuf_ptr = (uint32_t*) &vga_1bit_data_array[0];
    uint32_t *dvi_framebuf_ptr = (uint32_t*) &dvi_framebuf[0];

    uint32_t sr = 0;
    uint8_t shifts = 0;


    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {
        // The first uint32 of each line contains colour information as well as other stuff.
        uint32_t colours = *vga_framebuf_ptr++;

        uint8_t fore_col = get_four_bit_col((colours >> 20) & 0x0f);  
        uint8_t back_col = get_four_bit_col((colours >> 16) & 0x0f);  
        uint8_t pixcol;

        // get the 32-bit word
        for (int x = 0; x < WORDS_PER_LINE - 1; x++) {
            uint32_t vga_bit_word = *vga_framebuf_ptr;   
            vga_framebuf_ptr++;    

            uint32_t bitmask = 0x01;
            for (uint32_t b = 0; b < 32; b++) {
                if (vga_bit_word & bitmask) {
                    pixcol = fore_col;
                } else {
                    pixcol = back_col;
                }
                // Shift the shift register and put the 6-bit colour into the most significant bits
                sr = (sr >> 6) | (pixcol << 26);
                if (++shifts == 5) {
                    // Every 5 pixels the shift register consists of 5 6-bit values,
                    // which we pack into four bytes (a uin32_t).
                    *dvi_framebuf_ptr++ = sr;
                    shifts = 0;
                }
                bitmask <<= 1;
            }
        }
    }
}


void test_DVI_framebuf() {
    dvi_testbars();
}
#endif

void close_window() {
    fillRect(HELP_WINDOW_LEFT, HELP_WINDOW_TOP, HELP_WINDOW_WIDTH, HELP_WINDOW_HEIGHT, BLACK);
    uart_puts(UART_ID, "Window closed\n");
    init_line_colours();
    set_plot_line_colors(g_no_of_captured_pins);
    showing_window = false;
}


// char* left_rect_text =
//     // "Raspberry Pi Pico Test\n"
//     // "Graphics primitives demo\n"
//     // "Hunter Adams\n"
//     // "vha3@cornell.edu\n"
//     // "4-bit mod by Bruce Land";

//     "PALAVO\n"
//     "PIO Accelerated\n"
//     "Logic Analyser with\n"
//     "VGA Output\n"
//     "Peter Stansfeld\n";

    // "PLATYPI\n"
    // "Pico Logic Analyser\n"
    // "for Testing Your\n"
    // "PIO Ideas\n"
    // "Peter Stansfeld";

// char* right_rect_text =

//     "Inspired by and using\n"
//     "Graphics primitives demo\n"
//     "Hunter Adams\n"
//     "vha3@cornell.edu\n"
//     "4-bit mod by Bruce Land\n";

char * start_help_text = "Press h for help.\n";

#if USE_VGA_CAPTURE

enum vc_modes {VC_NONE, VC_VSYNC_AND_VSYNC_ON_CSYNC, VC_VSYNC_ON_CSYNC} ;
uint8_t vga_capture_mode = VC_NONE;

PIO vga_capture_pio = pio0;
uint vga_capture_sm = 0;
uint vga_capture_offset;

// PIO vga_detect_vsync_pio = pio0;
uint vga_detect_vsync_sm = 1;
uint vga_detect_vsync_offset;

// PIO vga_detect_csync_on_vsync_pio = pio0;
uint vga_detect_vsync_on_csync_sm = 2;
uint vga_detect_vsync_on_csync_offset;

int rgb_test_chan_0;
int rgb_test_chan_1;


void init_vga_capture_dma_and_start_capturing() {
    // DMA stuff

    dma_channel_config vga_capture_dma_ch0;
    dma_channel_config c1;
    
    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0
    rgb_test_chan_0 = dma_claim_unused_channel(true);
    rgb_test_chan_1 = dma_claim_unused_channel(true);

    // Channel Zero (receives color data to PIO VGA machine)
    vga_capture_dma_ch0 = dma_channel_get_default_config(rgb_test_chan_0);  // default configs
    channel_config_set_transfer_data_size(&vga_capture_dma_ch0, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&vga_capture_dma_ch0, false);                        // no read incrementing
    channel_config_set_write_increment(&vga_capture_dma_ch0, true);                      // yes write incrementing

    // channel_config_set_dreq(&c0, DREQ_PIO0_RX0) ;                        // DREQ_PIO0_RX0 pacing (FIFO)
    channel_config_set_dreq(&vga_capture_dma_ch0, pio_get_dreq(vga_capture_pio, vga_capture_sm, false));     // vga_detect_vsync rx FIFO pacing

    channel_config_set_chain_to(&vga_capture_dma_ch0, rgb_test_chan_1);                        // chain to other channel

    #define DVI_LINE_LENGTH_BYTES (640 * 4 / 5)
    
    #define DVI_BUF_TRANSFER_SIZE ((((640 * 4) / 5) * 480) / 4)

    static uint32_t testReadValue = /* 0xffffffff */((RED_BITS) | (GREEN_BITS << 6) | (BLUE_BITS << 12)) << 2;

     dma_channel_configure(
        rgb_test_chan_0,            // Channel to be configured
        &vga_capture_dma_ch0,                        // The configuration we just created
        &dvi_framebuf[0],              // The initial write address (pixel color array)
        &vga_capture_pio->rxf[vga_capture_sm],       // read address (RGB PIO RX FIFO)
        (0x0 << 28) | (DVI_LINE_LENGTH_BYTES * 480 / 4),                  // Number of transfers in words; in this case each word is 4 byte. this doesn't crash
        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    c1 = dma_channel_get_default_config(rgb_test_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, rgb_test_chan_0);                    // chain to other channel

    static char * dvi_framebuf_ptr = &dvi_framebuf[0];

    dma_channel_configure(
        rgb_test_chan_1,                        // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[rgb_test_chan_0].write_addr,  // Write address (channel 0 write address)
        &dvi_framebuf_ptr,                 // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

    dma_start_channel_mask((1u << rgb_test_chan_0)); 
}


// capture the RGB of pins 0 to 5 using HSYNC and VSYNC of pins 26 & 27, or CSYNC on pin 26.
void init_pio_vga_capture_with_vsync_and_vsync_on_csync() {
    if (vga_capture_mode == VC_NONE) {
        vga_capture_offset = pio_add_program(vga_capture_pio, &vga_capture_program);

        vga_capture_program_init(vga_capture_pio, vga_capture_sm, vga_capture_offset, RGB_IN_FIRST_PIN);

        vga_detect_vsync_offset = pio_add_program(vga_capture_pio, &vga_detect_vsync_program);
        vga_detect_vsync_program_init(vga_capture_pio, vga_detect_vsync_sm, vga_detect_vsync_offset, HSYNC_IN);
        // vga_detect_vsync_program_init(vga_capture_pio, vga_detect_vsync_sm, vga_detect_vsync_offset, 0);

        vga_detect_vsync_on_csync_offset = pio_add_program(vga_capture_pio, &vga_detect_vsync_on_csync_program);
        vga_detect_vsync_on_csync_program_init(vga_capture_pio, vga_detect_vsync_on_csync_sm, vga_detect_vsync_on_csync_offset, HSYNC_IN);
        // vga_detect_vsync_on_csync_program_init(vga_capture_pio, vga_detect_vsync_on_csync_sm, vga_detect_vsync_on_csync_offset, 0);

        init_vga_capture_dma_and_start_capturing();

        pio_enable_sm_mask_in_sync(vga_capture_pio, (1u << vga_capture_sm) | (1u << vga_detect_vsync_sm) | (1u << vga_detect_vsync_on_csync_sm));

        vga_capture_mode = VC_VSYNC_AND_VSYNC_ON_CSYNC;
    }
}


// capture the RGB of pins 6 to 11 using CSYNC of pins 22.
// or the RGB of pins 33 to 38 using CSYNC of pin 32
void init_pio_vga_detect_vsync_on_csync() {

    #ifndef PIMORONI_PICO_LIPO2XL_W_RP2350
    #define CSYNC_IN_PIN 22
    #define RGB_OUT_FIRST_PIN 6
    #else
    #define CSYNC_IN_PIN 31
    #define RGB_OUT_FIRST_PIN 32
    #endif

    if (vga_capture_mode == VC_NONE) {

        #if PICO_PIO_USE_GPIO_BASE==1
        pio_set_gpio_base(vga_capture_pio, 16);
        // #warning pio_set_gpio_base(vga_capture_pio, 16)
        #endif

        vga_capture_offset = pio_add_program(vga_capture_pio, &vga_capture_program);
        vga_capture_program_init(vga_capture_pio, vga_capture_sm, vga_capture_offset, RGB_OUT_FIRST_PIN);

        vga_detect_vsync_on_csync_offset = pio_add_program(vga_capture_pio, &vga_detect_vsync_on_csync_program);
        vga_detect_vsync_on_csync_program_init(vga_capture_pio, vga_detect_vsync_on_csync_sm, vga_detect_vsync_on_csync_offset, CSYNC_IN_PIN);

        init_vga_capture_dma_and_start_capturing();

        pio_enable_sm_mask_in_sync(vga_capture_pio, (1u << vga_capture_sm) | (1u << vga_detect_vsync_on_csync_sm));

        vga_capture_mode = VC_VSYNC_ON_CSYNC;
    }
}


void deinit_vga_capture() {
    if ((vga_capture_mode == VC_VSYNC_AND_VSYNC_ON_CSYNC) || (vga_capture_mode == VC_VSYNC_ON_CSYNC)) {
        // stop the state machine
        pio_sm_set_enabled(vga_capture_pio, vga_capture_sm, false);
        pio_sm_set_enabled(vga_capture_pio, vga_detect_vsync_on_csync_sm, false);

        if (vga_capture_mode == VC_VSYNC_AND_VSYNC_ON_CSYNC) {
            // stop the other state machine
            pio_sm_set_enabled(vga_capture_pio, vga_detect_vsync_sm, false);

            // and free it
            pio_remove_program(vga_capture_pio, &vga_detect_vsync_program, vga_detect_vsync_offset);
        }

        // free the remaining state machines
        pio_remove_program(vga_capture_pio, &vga_capture_program, vga_capture_offset);
        pio_remove_program(vga_capture_pio, &vga_detect_vsync_on_csync_program, vga_detect_vsync_on_csync_offset);

        // stop and free the dma channels
        dma_channel_cleanup(rgb_test_chan_1);
        dma_channel_cleanup(rgb_test_chan_0);

        // and unclaim them
        dma_channel_unclaim(rgb_test_chan_1);
        dma_channel_unclaim(rgb_test_chan_0);

        #if PICO_PIO_USE_GPIO_BASE==1
        pio_set_gpio_base(vga_capture_pio, 0);
        #endif

        vga_capture_mode = VC_NONE;
    }
}


void set_vga_capture(uint8_t new_capture_mode) {

    deinit_vga_capture();

    switch (new_capture_mode) {
        case VC_NONE:
            break;

        case VC_VSYNC_AND_VSYNC_ON_CSYNC:
            init_pio_vga_capture_with_vsync_and_vsync_on_csync();
            break;

        case VC_VSYNC_ON_CSYNC:
            init_pio_vga_detect_vsync_on_csync();
            break;
    }
}


#endif



uint32_t time_ms() {
    return time_us_64() / 1000;
}

uint check_ir() {
    static uint32_t last_ir_command = 0;
    static uint32_t last_ir_button_time;
    static uint32_t last_ir_button_start_time;

    uint ui_command = UIC_NONE;
    if (!pio_sm_is_rx_fifo_empty(ir_rx_pio, ir_rx_sm)) {

        uint32_t ir_command = pio_sm_get_blocking(ir_rx_pio, ir_rx_sm);

        uart_putuif(UART_ID, "ir: %#x\n", ir_command);

        // uart_putuif(UART_ID, "  ir: %#x\n", ir_command & 0xffff);
        char str[80];
        clear_statusbar_hint();

        uint32_t time_now = time_ms();
        if (ir_command) {
            // ir_command != 0, ie not a repeat code
            last_ir_button_time = time_now;
            last_ir_button_start_time = time_now;
            last_ir_command = ir_command;
        } else {
            // ir_command == 0, ie repeat code
           if (last_ir_button_start_time) {
                // Haven't timed out between repeat codes
                if (time_now - last_ir_button_start_time > 500) {
                    // Had enough time to allow a repeat code
                    if (time_now - last_ir_button_time < 500) {
                        // Still haven't timed out between repeat codes
                        ir_command = last_ir_command;                
                        last_ir_button_time = time_now;
                        sprintf(str, "repeat %#x ", ir_command);
                        writeString(str);
                        // uart_putuif(UART_ID, "ir2: %#x\n", ir_command);
                    } else {
                        // Timed out between repeat codes
                        last_ir_button_start_time = 0;
                    }
                }
                // Save this in case we're still allowed repeat codes
                last_ir_button_time = time_now;
            }
        }

        if (ir_command) {

            uint8_t ir_button = (ir_command >> 16) & 0xff;

            if ((ir_command & 0xffff) == 0xbf00) {
                // Adafruit Mini Remote Control (https://www.adafruit.com/product/389)



                uart_putcf(UART_ID, "  btn: %d\n", ir_button);

                switch (ir_button) {

                    case 1:
                        // 'play/pause'
                        ui_command = UIC_C; // capture
                        last_ir_command = 0;
                        break;

                    case 8:
                        ui_command = UIC_LEFT;
                        break;

                    case 10:
                        ui_command = UIC_RIGHT;
                        break;

    #if USE_DVI
                    case 14:
                        ui_command = UIC_R; // reset DVI
                        break;
    #endif

                    case 9:
                        // 'enter/save'
                        break;

    #if USE_DVI

                    case 16:
                        // '1'
                        set_vga_capture(VC_NONE);
                        test_DVI_framebuf();
                        break;

                    case 17:
                        set_vga_capture(VC_VSYNC_AND_VSYNC_ON_CSYNC);
                        // '2'
                        break;

                    case 18:
                        // '3'
                        set_vga_capture(VC_VSYNC_ON_CSYNC);
                        break;

    #endif

                }

                last_ir_command = ir_command;

            } else {
                // from a different ir remote

                if ((ir_command & 0xffff) == 0xff00) {
                    // Argon IR Remote (https://argon40.com/products/argon-remote?_pos=1&_psq=remote&_ss=e&_v=1.0)

                    switch (ir_button) {
                        case 0xce:
                            // 'play/pause'
                            ui_command = UIC_C; // capture
                            last_ir_command = 0;
                            break;

                        case 0xca:
                            ui_command = UIC_UP;
                            break;

                        case 0xd2:
                            ui_command = UIC_DOWN;
                            break;

                        case 0x99:
                            ui_command = UIC_LEFT;
                            break;

                        case 0xc1:
                            ui_command = UIC_RIGHT;
                            break;

                        case 0x9d:
                            ui_command = UIC_TAB;
                            break;

                        case 0x80:
                            ui_command = UIC_PLUS;
                            break;

                        case 0x81:
                            ui_command = UIC_MINUS;
                            break;

                    }
                    last_ir_command = ir_command;
                }
            }
        }
    }
    
    return ui_command;
}


void ir_flush() {
    pio_sm_clear_fifos(ir_rx_pio, ir_rx_sm);
}


#if USE_DVI
void print_dvi_regs() {
    uart_putcf(UART_ID, "expand_tmds: %#x\n", dvi_get_expand_tmds());
    uart_putcf(UART_ID, "expand_shift: %#x\n", dvi_get_expand_shift());
    uart_putcf(UART_ID, "csr: %#x\n", dvi_get_csr());
    uart_putcf(UART_ID, "v_scanline: %d\n", dvi_get_v_scanline());
}
#endif

int main() {

// hw_write_masked(
// 		&clocks_hw->clk[clk_hstx].div,
// 		2 << CLOCKS_CLK_HSTX_DIV_INT_LSB,
// 		CLOCKS_CLK_HSTX_DIV_INT_BITS
// 	);


    // this does not work
    // CLK_HSTX = clk_sys. Transmit bit clock for the HSTX peripheral.


    // clock_configure_undivided(clk_hstx,
    //                 0,
    //                 CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
    //                 125000000); // reports as changed to this value, but makes no difference to hstx dvi refresh rate - it's still 72 Hz and trying for 60 Hz

    // Initialize stdio
    
    stdio_init_all();

    #ifndef PIMORONI_PICO_LIPO2XL_W_RP2350

    // todo - we can probably and should use the code below (in the #else), which is from the SDK docs

    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    // gpio_set_function(LED_PIN, GPIO_FUNC_UART);

    #else

    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_init(UART_ID, BAUD_RATE);

    #endif

#if USE_LED_AS_IR_DEBUG == 0

#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT); // set LED_PIN GPIO to an output
    gpio_put(LED_PIN, 1); // set LED_PIN

#endif

#endif

    gpio_init_mask(GPIO_INPUT_MASK); // init all GPIO in the above mask

    for (int i = 0; i < 32; i++) {
        if (GPIO_INPUT_MASK & (1 << i)) {
            gpio_pull_up(i);
        }
    }

    uart_putcf(UART_ID, "GPIO Inputs: %d\n", GPIO_INPUT_MASK);

    uart_puts(UART_ID, "\n\n");
    // uart_puts(UART_ID, left_rect_text);
    // uart_puts(UART_ID, "\n");
    // uart_puts(UART_ID, right_rect_text);

    uart_puts(UART_ID, name_string);
    uart_puts(UART_ID, "\n");

    uart_puts(UART_ID, about_strings);
    uart_puts(UART_ID, "\n");
    // uart_puts(UART_ID, right_rect_text);


#ifdef SYS_CLK_KHZ
    int sys_clk_freq_khz = SYS_CLK_KHZ;
    uart_putcf(UART_ID, "SYS_CLOCK_KHZ: %d\n", sys_clk_freq_khz);
#endif

#ifdef PICO_SDK_VERSION_STRING
    uart_puts(UART_ID, "SDK Version: ");
    uart_puts(UART_ID, PICO_SDK_VERSION_STRING);
    uart_puts(UART_ID, "\n");
#endif

#ifdef PICO_BOARD
    uart_puts(UART_ID, "Board: ");
    uart_puts(UART_ID, PICO_BOARD);
    uart_puts(UART_ID, "\n");
#endif


#ifdef PICO_PLATFORM
    uart_puts(UART_ID, "Platform: ");
    uart_puts(UART_ID, PICO_PLATFORM);
    uart_puts(UART_ID, "\n");
#endif


#if USE_DVI
    // Initialize the VGA screen
    uart_puts(UART_ID, "Initialising DVI...\n");

    uart_putcf(UART_ID, "clk_hstx: %d\n", clock_get_hz (clk_hstx));
    // uart_putcf(UART_ID, "HSTX Frequency: %d\n", clock_get_hz (CLK_DEST_HSTX));


    // clock_stop(clk_hstx);

/*
        clock_configure(
            clk_peri,                                   //clock_handle
            0,                                          //
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, //
            150000000,                                  // src_freq
            150000000                                   // freq
        );

        clock_configure(
            clk_hstx,                                   //clock_handle
            0,                                          //
            CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, //
            150000000,                                  // src_freq
            125000000                                   // freq
        );
*/

    // this does not work
    // CLK_HSTX = clk_sys. Transmit bit clock for the HSTX peripheral.
    // clock_configure_undivided(clk_hstx,
    //                 0,
    //                 CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
    //                 125000000); // reports as changed to this value, but makes no difference to hstx dvi refresh rate - it's still 72Hz and tring for 60 Hz


// this does not work

        // clock_configure(
        //     clk_hstx,                                   //clock_handle
        //     0,                                          //
        //     CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, //
        //     150000000,                                  // src_freq
        //     125000000                                   // freq
        // );


// this does not work

    // clock_configure(
    //     clk_hstx,
    //     0,                                                // No glitchless mux
    //     // CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux

    //     CLOCKS_CLK_HSTX_CTRL_AUXSRC_VALUE_CLK_SYS,
    //     // Option.CPU_Speed * 1000,                               // Input frequency
    //     // Option.CPU_Speed * 500                                // Output (must be same as no divider)
    //     250000000,                               // Input frequency
    //     125000000                                // Output (must be same as no divider)
    // );
    uart_putcf(UART_ID, "clk_peri: %d\n", clock_get_hz(clk_peri));

    uart_putcf(UART_ID, "clk_hstx: %d\n", clock_get_hz(clk_hstx));

    sleep_ms(100) ;

    dvi_init();

    print_dvi_regs();

    dvi_testbars();

#endif

    // Initialize the VGA screen

    #if PICO_PIO_USE_GPIO_BASE==1

    // When connected to another Pico 2 (for VGA to DVI conversion) CSYNC seems
    // to suffer electrically. Increasing the drive strength improves it.
    // Also putting a buffer IC between the Pico Lipo's output and the VGA CSYNC
    // resistor fixed it. Signal relection from the monitor, perhaps? todo 
    // gpio_set_drive_strength(CSYNC /*CSYNC*/, GPIO_DRIVE_STRENGTH_8MA); // works when Pico2 CSYNC has not pull resistance - unless a debug probe is attached & powered(???)

    // enum gpio_drive_strength gds = gpio_get_drive_strength(CSYNC /*CSYNC*/);
    // uart_putcf(UART_ID, "CSYNC drive strength: %d\n", gds);

    gpio_set_function(CSYNC, GPIO_OUT); // set CSYNC to output
    // gpio_set_drive_strength(CSYNC /*CSYNC*/, GPIO_DRIVE_STRENGTH_12MA);

    enum gpio_drive_strength gds = gpio_get_drive_strength(CSYNC /*CSYNC*/);
    uart_putcf(UART_ID, "CSYNC drive strength: %d\n", gds);

    #endif

    uart_puts(UART_ID, "Initialising VGA...\n");
    initVGA() ;
    init_line_colours();

    // We're going to capture into a u32 buffer, for best DMA efficiency. Need
    // to be careful of rounding in case the number of pins being sampled
    // isn't a power of 2.

    // uint total_sample_bits = g_capture_n_samples * g_no_of_captured_pins;
    // total_sample_bits += bits_packed_per_word(g_no_of_captured_pins) - 1;
    
    // uint buf_size_words = total_sample_bits / bits_packed_per_word(g_no_of_captured_pins);

    // As we know that BUF_SIZE_WORDS is a power of 2 we don't need to bother with bits_packed_per_word()
    uint buf_size_words = BUF_SIZE_WORDS; // todo - replace buf_size_words with BUF_SIZE_WORDS

    // equivalent to: uint8_t array[210,000] 
    uint32_t *capture_buf = malloc(buf_size_words * sizeof(uint32_t));
    hard_assert(capture_buf);

    uint total_sample_bits = buf_size_words * bits_packed_per_word(g_no_of_captured_pins);

    g_capture_n_samples = total_sample_bits / g_no_of_captured_pins;    
    
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
    
    #ifndef PIMORONI_PICO_LIPO2XL_W_RP2350
        PIO pio = pio1;
    #else
        PIO pio = pio2;
    #endif

    uint sm = 3;
    uint dma_chan = dma_claim_unused_channel(true);
    // uint dma_chan = 5;

    // logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 125000000 / (115200 * 4) /*271.267*/);
    logic_analyser_init(pio, sm, g_pins_base, g_no_of_pins_to_capture, g_sample_frequency);

    #ifdef PIMORONI_PICO_LIPO2XL_W_RP2350

    logic_analyser_trigger_init(true); // for a trigger pin that's on a different PIO_BASE

    #endif

    // logic_analyser_init(pio, sm, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, 5); // this works

    // animation pause
    bool demo_paused = true;


    // Draw some filled rectangles
    fillRect(64, 0, 176, 50, LEFT_BOX_COLOR); // blue box
    fillRect(250, 0, 176, 50, MIDDLE_BOX_COLOR); // red box:
    fillRect(435, 0, 176, 50, RIGHT_BOX_COLOR); // green box

//    drawVLine(Vline_x, 300, (Vline_x>>2), color_index);


    #define CORNER_LEN 2

    drawHLine(0, 0, CORNER_LEN, WHITE);
    drawVLine(0, 0, CORNER_LEN, WHITE);

    drawHLine(SCREEN_WIDTH - CORNER_LEN, 0, CORNER_LEN, WHITE);
    drawVLine(639, 0, CORNER_LEN, WHITE);

    // drawRect(0, 0, SCREEN_WIDTH, TOOLBAR_TOP + TOOLBAR_HEIGHT + 1, WHITE);

    // drawHLine(0, TOOLBAR_TOP + TOOLBAR_HEIGHT + 3, SCREEN_WIDTH, WHITE);





    // drawHLine(10, TOOLBAR_TOP + TOOLBAR_HEIGHT, SCREEN_WIDTH - 10, WHITE);
    // drawVLine(102, 0, TOOLBAR_TOP + TOOLBAR_HEIGHT - 1, WHITE);


    // for (int i = 0; i < SCREEN_WIDTH - 10; i+= 3){
    //     drawPixel(10 + i , TOOLBAR_TOP + TOOLBAR_HEIGHT, WHITE); // test all colours in first horizontal line following sync
    // }

    drawHLine(0, SCREEN_HEIGHT - 1, CORNER_LEN, WHITE);
    drawVLine(0, SCREEN_HEIGHT - CORNER_LEN, CORNER_LEN, WHITE);

    drawVLine(639, SCREEN_HEIGHT - CORNER_LEN, CORNER_LEN, WHITE);
    drawHLine(SCREEN_WIDTH - CORNER_LEN, SCREEN_HEIGHT - 1, CORNER_LEN, WHITE);







    // drawRect(0, SCREEN_HEIGHT - (STATUSBAR_HEIGHT + 3), SCREEN_WIDTH, STATUSBAR_HEIGHT + 3, WHITE);

    // drawHLine(0, SCREEN_HEIGHT - (STATUSBAR_HEIGHT + 3), SCREEN_WIDTH, WHITE);

    // draw_titlebar();
    
    draw_toolbar();

    draw_statusbar();

    // drawHLine(630, 480 - 1, 1, WHITE);


    // for (int i = 0; i < 16; i++){
    //     drawPixel(16 + i , 0, i); // test all colours in first horizontal line following sync
    // }

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

    // setTextColor(WHITE) ;
    // set_text_padding(2);
    // setCursor(65, 0) ;
    // logo(3, 0, true);

    // logo_small(180, 0, true);

    logo_med(3, 3, true);

    // logo(200, 0);

    // logo(440, 30);


    // setCursor(435, 0);
    // writeString(right_rect_text);

    // setCursor(250, 0) ;
    // setTextSize(2) ;
    // writeString("Time Elapsed:") ;


    clear_statusbar_hint(); // set the cursor etc.
    writeString(start_help_text);
    uart_puts(UART_ID, start_help_text);

    // Setup a 1Hz timer
    struct repeating_timer timer;
    add_repeating_timer_ms(-1000, repeating_timer_callback, NULL, &timer);

    // Wait for the pios to get warmed up. Probably not necessary.
    sleep_ms(10);

    // no need to initialise the ir PIO state machine here as it's done in `logic_analyser_arm()`
    // init_ir_rx(); 

    logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, g_trigger_pin_base, g_trigger_type);

    get_plot_height(g_no_of_captured_pins);
    set_plot_line_colors(g_no_of_captured_pins);


    // print_capture_buf(capture_buf, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, CAPTURE_N_SAMPLES);
    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, true);

    // plot a fixed minimap of the above capture
    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);

    draw_minimap_indicator();

#if USE_VGA_CAPTURE
    sleep_ms(500);
    set_vga_capture(VC_VSYNC_AND_VSYNC_ON_CSYNC);
    // set_vga_capture(VC_VSYNC_ON_CSYNC);
    // set_vga_capture(VC_NONE);
#endif

    // mainloop

    uint ins = 0;

    for (int i = 0; i < 32; i++) {

        // if (gpio_is_dir_out(i) == 0) {
        // if (gpio_is_pulled_up(i) == 0) {
        if (gpio_is_dir_out(i) == 0) {

            ins |= (1 << i);
        }

    }

    uart_putcf(UART_ID, "Inputs: %x\n", ins);


    while(true) {

        // Timing text
        
        // if (time_accum != time_accum_old) {
        //     setTextColor(WHITE) ;
        //     time_accum_old = time_accum ;
        //     fillRect(250, 20, 176, 30, MIDDLE_BOX_COLOR); // red box
        //     sprintf(timetext, "%d", time_accum) ;
        //     setCursor(250, 20) ;
        //     setTextSize(2) ;
        //     writeString(timetext) ;
        //     // gpio_xor_mask(1 << LED_PIN);
        // }

        uint ui_command = check_keyboard();

        if (!ui_command) {
            ui_command = check_ir();
        }

        if (ui_command) {

            bool plot_required = false;
            bool mini_map_redraw_required = false;
            if (showing_window) {
                // if (ui_command == UIC_ESC) {
                    // writeString("esc");
                    close_window();
                    plot_required = true;
                // }

            } else {


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
                        int scroll_inc = 1;
                        if (g_mag < 0) {
                            scroll_inc = abs(g_mag) + 1;
                        }
                        plot_required = set_scroll_x(g_scrollx + scroll_inc);
                        break;

                    case UIC_LEFT:
                        writeString("scroll left");
                        int scroll_dec = 1;
                        if (g_mag < 0) {
                            scroll_dec = abs(g_mag) + 1;
                        }
                        plot_required = set_scroll_x(MAX(g_scrollx - scroll_dec, 0));
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
                        // fillRect(0, PLOT_TOP, SCREEN_WIDTH, MINIMAP_BOTTOM - PLOT_TOP, BLACK);

                        init_ir_rx(false);

                        logic_analyser_init(pio, sm, g_pins_base, g_no_of_pins_to_capture, g_sample_frequency);

                        if (!logic_analyser_arm(pio, sm, dma_chan, capture_buf, buf_size_words, g_trigger_pin_base, g_trigger_type)) {
                            writeString(" - failed to trigger");
                        }

                        fillRect(0, PLOT_TOP, SCREEN_WIDTH, MINIMAP_BOTTOM - PLOT_TOP, BLACK);


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


                        // plot_capture_buf(capture_buf, CAPTURE_PIN_BASE, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);

                        set_scroll_x(0);

                        get_plot_height(g_no_of_captured_pins);

                        set_plot_line_colors(g_no_of_captured_pins);

                        plot_required = true;

                        mini_map_redraw_required = true;

                        ir_flush();
                        // last_ir_command = 0;

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
                                set_no_of_pins(MIN(g_no_of_pins_to_capture + 1, MAX_NO_OF_CHANNELS));
                                break;

                            case SS_PINS_BASE:
                                set_pins_base(MIN(g_pins_base + 1, MAX_BASE_PIN_NO));
                                break;

                            case SS_TRIGGER_PIN_BASE:
                                set_trigger_pin_base(MIN(g_trigger_pin_base + 1, MAX_BASE_PIN_NO));
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
                        show_help_window();
                        break;

                    case UIC_A:
                        writeString("about");
                        show_about_window();
                        break;

#if USE_DVI

#if USE_VGA_CAPTURE

                    case UIC_V:
                        switch (vga_capture_mode) {
                            case VC_NONE:
                                set_vga_capture(VC_VSYNC_AND_VSYNC_ON_CSYNC);
                                writeString("capture VGA IN to DVI");
                                break;

                            case VC_VSYNC_AND_VSYNC_ON_CSYNC:
                                set_vga_capture(VC_VSYNC_ON_CSYNC);
                                writeString("mirror VGA OUT to DVI");
                                break;

                            case VC_VSYNC_ON_CSYNC:
                                set_vga_capture(VC_NONE);
                                writeString("display DVI test pattern");
                                test_DVI_framebuf();
                                break;
                        }
                        break;
#endif

                    case UIC_D:
                        writeString("test DVI");
                        test_DVI_framebuf();
                        break;

                    case UIC_R:

                        print_dvi_regs();

                        // writeString("reset DVI");
                        // dvi_deinit();
                        // sleep_ms(100) ;
                        // dvi_init();
                        // print_dvi_regs();

                        writeString("reinit DVI");
                        dvi_reinit();
                        // sleep_ms(100) ;
                        // dvi_init();
                        print_dvi_regs();
                        break;

                    case UIC_S:
                        writeString("deinit DVI");
                        dvi_deinit();
                        break;
#endif

                }
            }

            if ((plot_required) || (mini_map_redraw_required)) {

                if (plot_required) {
                    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, true);
                }

                if (mini_map_redraw_required){
                    plot_capture_buf(capture_buf, g_pins_base, g_no_of_captured_pins, g_capture_n_samples, g_mag, g_scrollx, false);
                }

                // A brief nap
                // sleep_ms(10);
                // busy_wait_us(10000);
            }

        } else {

            if (!demo_paused && !showing_window) {
                // uint32_t time_us_now = 
                if (time_us_32() - last_demo_time >= 10000) {
                    // {

                    demo();
                    last_demo_time = time_us_32();

                    // sleep_ms(1); // this crashes the hstx-dvi
                    // sleep_ms(10); // this seems not to crash the hstx-dvi
                    // sleep_ms(1); // any sleep seems to crash the hstx-dvi

                // A brief nap
                    // sleep_ms(10);
                    // reduce the sleep time to test the hstx-dvi lockup issue
                    // sleep_ms(2);

                    // sleep_ms(10);

                    // busy_wait_us(10000);
                }
            }
            // NB sleep_ms, which trys to use the arm's wfe instruction, seems to be the thing
            // that causes the hstx-dvi to fall over.

            // uart_putc(UART_ID, 'B');
        }

   }

}