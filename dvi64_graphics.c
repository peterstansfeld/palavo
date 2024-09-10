// Copyright (c) 2024 Raspberry Pi (Trading) Ltd.

// Generate DVI output using the command expander and TMDS encoder in HSTX.

// This example requires an external digital video connector connected to
// GPIOs 12 through 19 (the HSTX-capable GPIOs) with appropriate
// current-limiting resistors, e.g. 270 ohms. The pinout used in this example
// matches the Pico DVI Sock board, which can be soldered onto a Pico 2:
// https://github.com/Wren6991/Pico-DVI-Sock

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/sio.h"
// #include "pico/multicore.h"
// #include "pico/sem.h"

// #include "mountains_640x480_rgb332.h"
// #include "my_mountains_640x480_rgb332.h"

#include <string.h>

// #define framebuf mountains_640x480

#include "dvi64_graphics.h"


// ----------------------------------------------------------------------------

// My constants

// #define COLOUR_MODE 332 // RGB332
// #define COLOUR_MODE 121    // RBG121

// #define COLOUR_MODE 222    // RBG222

// #define COLOUR_MODE 111    // RBG111

#define COLOUR_MODE 2226


#if COLOUR_MODE == 332

static char __attribute__((aligned(4))) framebuf[640 * 480];

#elif COLOUR_MODE == 121

static char __attribute__((aligned(4))) framebuf[(640 / 2) * 480];

#elif COLOUR_MODE == 222

// static char __attribute__((aligned(4))) framebuf[((640 * 6) / 8) * 480];
static char __attribute__((aligned(4))) framebuf[640 * 480];


#elif COLOUR_MODE == 111

// static char __attribute__((aligned(4))) framebuf[((640 * 6) / 8) * 480];
static char __attribute__((aligned(4))) framebuf[(640 / 2) * 480];


#elif COLOUR_MODE == 2226

char __attribute__((aligned(4))) dvi_framebuf[((640 * 4) / 5) * 480];

#endif


// DVI constants

#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

// #define MODE_H_SYNC_POLARITY 0
// #define MODE_H_FRONT_PORCH   16
// #define MODE_H_SYNC_WIDTH    96
// #define MODE_H_BACK_PORCH    48
// #define MODE_H_ACTIVE_PIXELS 640

// #define MODE_V_SYNC_POLARITY 0
// #define MODE_V_FRONT_PORCH   10
// #define MODE_V_SYNC_WIDTH    2
// #define MODE_V_BACK_PORCH    33
// #define MODE_V_ACTIVE_LINES  480

#define MODE_H_TOTAL_PIXELS ( \
    MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + \
    MODE_H_BACK_PORCH  + MODE_H_ACTIVE_PIXELS \
)
#define MODE_V_TOTAL_LINES  ( \
    MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + \
    MODE_V_BACK_PORCH  + MODE_V_ACTIVE_LINES \
)

#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

// ----------------------------------------------------------------------------
// HSTX command lists

// Lists are padded with NOPs to be >= HSTX FIFO size, to avoid DMA rapidly
// pingponging and tripping up the IRQs.

static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V1_H1,
    HSTX_CMD_NOP
};

static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS),
    SYNC_V0_H1,
    HSTX_CMD_NOP
};

static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS       | MODE_H_ACTIVE_PIXELS
};

// ----------------------------------------------------------------------------
// DMA logic

#define DMACH_PING 0
#define DMACH_PONG 1

// dma_channel_claim(0); // todo - why isn't this ok?
// dma_channel_claim(1); // todo - why isn't this ok?

// const int DMACH_PING = dma_claim_unused_channel(true);
// int DMACH_PONG = dma_claim_unused_channel(true);


// First we ping. Then we pong. Then... we ping again.
static bool dma_pong = false;

// A ping and a pong are cued up initially, so the first time we enter this
// handler it is to cue up the second ping after the first ping has completed.
// This is the third scanline overall (-> =2 because zero-based).
static uint v_scanline = 2;

// During the vertical active period, we take two IRQs per scanline: one to
// post the command list, and another to post the pixels.
static bool vactive_cmdlist_posted = false;

void __scratch_x("") dma_irq_handler() {
    // dma_pong indicates the channel that just finished, which is the one
    // we're about to reload.
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;

    if (v_scanline >= MODE_V_FRONT_PORCH && v_scanline < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
    } else if (v_scanline < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
    } else if (!vactive_cmdlist_posted) {
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        vactive_cmdlist_posted = true;
    } else {
#if COLOUR_MODE == 332
        ch->read_addr = (uintptr_t)&framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * (MODE_H_ACTIVE_PIXELS)];
        ch->transfer_count = (MODE_H_ACTIVE_PIXELS) / sizeof(uint32_t);
#elif COLOUR_MODE == 121 
        ch->read_addr = (uintptr_t)&framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * (MODE_H_ACTIVE_PIXELS / 2)];
        ch->transfer_count = (MODE_H_ACTIVE_PIXELS / 2) / sizeof(uint32_t);

#elif COLOUR_MODE == 222 
        // ch->read_addr = (uintptr_t)&framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * ((MODE_H_ACTIVE_PIXELS * 6) / 8)];
        // ch->transfer_count = ((MODE_H_ACTIVE_PIXELS * 6) / 8) / sizeof(uint32_t);

        ch->read_addr = (uintptr_t)&framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * (MODE_H_ACTIVE_PIXELS)];
        ch->transfer_count = (MODE_H_ACTIVE_PIXELS) / sizeof(uint32_t);

#elif COLOUR_MODE == 111 
        ch->read_addr = (uintptr_t)&framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * (MODE_H_ACTIVE_PIXELS / 2)];
        ch->transfer_count = (MODE_H_ACTIVE_PIXELS / 2) / sizeof(uint32_t);

#elif COLOUR_MODE == 2226
        ch->read_addr = (uintptr_t)&dvi_framebuf[(v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES)) * ((MODE_H_ACTIVE_PIXELS * 4) / 5)];
        ch->transfer_count = ((MODE_H_ACTIVE_PIXELS * 4) / 5) / sizeof(uint32_t);

#endif
        vactive_cmdlist_posted = false;
    }

    if (!vactive_cmdlist_posted) {
        v_scanline = (v_scanline + 1) % MODE_V_TOTAL_LINES;
    }
}

// ----------------------------------------------------------------------------
// Main program

static __force_inline uint16_t colour_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)r & 0xf8) >> 3 | ((uint16_t)g & 0xfc) << 3 | ((uint16_t)b & 0xf8) << 8;
}

static __force_inline uint8_t colour_rgb332(uint8_t r, uint8_t g, uint8_t b) {
    return (r & 0xc0) >> 6 | (g & 0xe0) >> 3 | (b & 0xe0) >> 0;
}

void scroll_framebuffer(void);

// dark green,light green, blue, red

// enum colors {BLACK, DARK_GREEN, MED_GREEN, GREEN,
//             DARK_BLUE, BLUE, LIGHT_BLUE, CYAN,
//             RED, DARK_ORANGE, ORANGE, YELLOW, 
//             MAGENTA, PINK, LIGHT_PINK, WHITE} ;

#if COLOUR_MODE == 332

#define RED_BITS        0b11100000
#define DARK_GREEN_BITS 0b00000100
#define MED_GREEN_BITS  0b00011000
#define BLUE_BITS       0b0000011

#elif COLOUR_MODE == 222

// #define RED_BITS        0b00110000
// #define DARK_GREEN_BITS 0b00000100
// #define MED_GREEN_BITS  0b00001000
// #define BLUE_BITS       0b00000011


#define RED_BITS        0b11000000
#define DARK_GREEN_BITS 0b00010000
#define MED_GREEN_BITS  0b00100000
#define BLUE_BITS       0b00001100


#elif COLOUR_MODE == 2226

// #define RED_BITS        0b11000000
// #define DARK_GREEN_BITS 0b00010000
// #define MED_GREEN_BITS  0b00100000
// #define BLUE_BITS       0b00001100

#endif


// #define RED_BITS 0b10000000
// #define DARK_GREEN_BITS 0b00001000
// #define MED_GREEN_BITS  0b00010000
// #define BLUE_BITS 0b0000010

#if (COLOUR_MODE == 332) | (COLOUR_MODE == 222) | (COLOUR_MODE == 2226)

const uint8_t FOUR_BIT_COLS[16] = {
    0x00, DARK_GREEN_BITS, MED_GREEN_BITS, DARK_GREEN_BITS | MED_GREEN_BITS,
    BLUE_BITS, BLUE_BITS | DARK_GREEN_BITS, BLUE_BITS | MED_GREEN_BITS, BLUE_BITS | DARK_GREEN_BITS | MED_GREEN_BITS,
    RED_BITS, RED_BITS | DARK_GREEN_BITS, RED_BITS | MED_GREEN_BITS, RED_BITS | DARK_GREEN_BITS | MED_GREEN_BITS,
    RED_BITS | BLUE_BITS, RED_BITS | BLUE_BITS | DARK_GREEN_BITS, RED_BITS | BLUE_BITS | MED_GREEN_BITS, RED_BITS | BLUE_BITS | DARK_GREEN_BITS | MED_GREEN_BITS };

#endif

uint8_t get_four_bit_col(uint8_t col) {
    return FOUR_BIT_COLS[col & 0x0f];
}

#define IGNORE_THIS 1


#if COLOUR_MODE == 2226

void dvi_testbars() {
    uint32_t *framebuf_ptr = (uint32_t*) &dvi_framebuf[0]; 

    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {
        uint32_t sr = 0;
        uint8_t shifts = 0;
        uint8_t pixcol;
        for (int x = 0; x < MODE_H_ACTIVE_PIXELS; x++) {
            pixcol = FOUR_BIT_COLS[x / 40];
            sr = (sr >> 6) | (pixcol << 26);
            if (++shifts == 5) {
                // Every 5 shifts (pixels) save the shift register into the frame buffer
                *framebuf_ptr++ = sr;
                shifts = 0;
            }
        }
    }    
}

#endif

void dvi_init() {

    dma_channel_claim(DMACH_PING);
    dma_channel_claim(DMACH_PONG);

    // modify the first horizontal line to be red
    // memset(&framebuf, 0xe0, MODE_H_ACTIVE_PIXELS);

    // modify the third horizontal line to be yellow (red & green)
    // memset(&framebuf[2 * MODE_H_ACTIVE_PIXELS], 0xfc, MODE_H_ACTIVE_PIXELS);

#if COLOUR_MODE == 332
    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {
        int start = y * MODE_H_ACTIVE_PIXELS;
        for (int x = 0; x < MODE_H_ACTIVE_PIXELS; x++) {
            // uint8_t = 0; 
            // framebuf[start + x] = 0x80; 
            framebuf[start + x] = FOUR_BIT_COLS[x / 40]; 
        }
    }

#elif COLOUR_MODE == 121
    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {
        int start = y * MODE_H_ACTIVE_PIXELS / 2;
        for (int x = 0; x < MODE_H_ACTIVE_PIXELS / 2; x++) {
            // uint8_t = 0; 
            // framebuf[start + x] = 0x80;
            uint8_t pixcol = /* 0x0f - */ (x / 20); // 0 to 15 
            framebuf[start + x] = (pixcol << 4) | pixcol; 
        }
    }

#elif COLOUR_MODE == 222
    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {
        // int start = y * MODE_H_ACTIVE_PIXELS / 2;
        // for (int x = 0; x < (MODE_H_ACTIVE_PIXELS * 6) / 8; x++) {
        //     // uint8_t = 0; 
        //     // framebuf[start + x] = 0x80;
        //     // uint8_t pixcol = /* 0x0f - */ (x / 20); // 0 to 15 
        //     // framebuf[start + x] = (pixcol << 4) | pixcol; 
        //     framebuf[start + x] = 0xff; 
        // }

        int start = y * MODE_H_ACTIVE_PIXELS;
        for (int x = 0; x < MODE_H_ACTIVE_PIXELS; x++) {
            // uint8_t = 0; 
            // framebuf[start + x] = 0x80; 
            framebuf[start + x] = FOUR_BIT_COLS[x / 40]; 
        }
    }

#elif COLOUR_MODE == 111
    for (int y = 0; y < MODE_V_ACTIVE_LINES; y++) {

        int start = y * MODE_H_ACTIVE_PIXELS / 2;
        for (int x = 0; x < MODE_H_ACTIVE_PIXELS / 2; x++) {
            // uint8_t = 0; 
            // framebuf[start + x] = 0x80;
            uint8_t pixcol = /* 0x0f - */ ((x / 20) << 1) & 0x0e; // 0 to 15 
            framebuf[start + x] = (pixcol << 4) | pixcol; 
        }
    }

#elif COLOUR_MODE == 2226

    dvi_testbars();

#endif

#if COLOUR_MODE == 332

    // Configure HSTX's TMDS encoder for RGB332
    hstx_ctrl_hw->expand_tmds =
        2  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 3 valid red bits to use          
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        2  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 3 valid green bits to use        
        29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 29 - rol 32 - 29 = 3
        1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 2 valid blue bits to use         
        26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 26 - rol 32 - 26 = 6

#elif COLOUR_MODE == 121

    // Configure HSTX's TMDS encoder for RGB121
    // red, blue, med green, dark green 
    hstx_ctrl_hw->expand_tmds = // commenting out this line is fun.. all green, purple, grey and black
        0  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 1 valid red bits to use          
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
        30 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol (32 - 30) = 2
        0  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 1 valid blue bits to use         
        31 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 31 - rol (32 - 31) = 1
    // the above works but the colours are dimmer, which is the same thing that Chris
    // experienced on the Circuit Python Deep Dive. Basically, 0b1000000 should ideally
    // expand to 0xffff, ie bright red, but I suspect it's expanding to 0x8000.

#elif COLOUR_MODE == 222

    // Configure HSTX's TMDS encoder for RGB121
    // red, blue, med green, dark green 
    // hstx_ctrl_hw->expand_tmds = // commenting out this line is fun.. all green, purple, grey and black
    //     1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 1 valid red bits to use          
    //     0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
    //     1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
    //     30 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol (32 - 30) = 2
    //     1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 1 valid blue bits to use         
    //     28 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 28 - rol (32 - 28) = 4

    // hstx_ctrl_hw->expand_tmds =
    //     1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 2 valid red bits to use          
    //     30  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
    //     1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
    //     28 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol 32 - 30 = 2
    //     1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 2 valid blue bits to use         
    //     26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 28 - rol 32 - 28 = 4

    hstx_ctrl_hw->expand_tmds =
        1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 2 valid red bits to use          
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
        30 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol 32 - 30 = 2
        1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 2 valid blue bits to use         
        28 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 28 - rol 32 - 28 = 4

#elif COLOUR_MODE == 111

    // Configure HSTX's TMDS encoder for RGB121
    // red, blue, med green, dark green 
    hstx_ctrl_hw->expand_tmds = // commenting out this line is fun.. all green, purple, grey and black
        0  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 1 valid red bits to use          
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        0  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
        30 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol (32 - 30) = 2
        0  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 1 valid blue bits to use         
        31 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 31 - rol (32 - 31) = 1

#elif COLOUR_MODE == 2226

    // Configure HSTX's TMDS encoder for RGB222
    // red, blue, med green, dark green 
    hstx_ctrl_hw->expand_tmds =
        1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 2 valid red bits to use          
        0  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
        30 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol 32 - 30 = 2
        1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 2 valid blue bits to use         
        28 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 28 - rol 32 - 28 = 4


        // 1  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 2 valid red bits to use          
        // 2  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // don't rotate                     
        // 1  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 2 valid green bits to use        
        // 28 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 30 - rol 32 - 30 = 2
        // 1  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 2 valid blue bits to use         
        // 26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 28 - rol 32 - 28 = 4

#endif


    // Configure HSTX's TMDS encoder for RGBD
    // hstx_ctrl_hw->expand_tmds =
    //     0  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | // 1 valid red bit to use          
    //     28 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB   | // rotate right 28 = rol 32 - 28 = 4                     
    //     0  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB | // 1 valid green bit to use        
    //     27 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB   | // rotate right 27 - rol 32 - 27 = 5
    //     0  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB | // 1 valid blue bit to use         
    //     26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;    // rotate right 26 - rol 32 - 26 = 6

#if COLOUR_MODE == 332
    // Pixels (TMDS) come in 4 8-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

#elif COLOUR_MODE == 121
    // Pixels (TMDS) come in 8 4-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

#elif COLOUR_MODE == 222
    // Pixels (TMDS) come in 8 6-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    // hstx_ctrl_hw->expand_shift =
    //     6 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
    //     6 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
    //     1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
    //     0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    // Pixels (TMDS) come in 4 8-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

#elif COLOUR_MODE == 111
    // Pixels (TMDS) come in 8 4-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

#elif COLOUR_MODE == 2226

    // Pixels (TMDS) come in 5 6-bit chunks. Control symbols (RAW) are an
    // entire 32-bit word.
    hstx_ctrl_hw->expand_shift =
        5 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB | // 
        6 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

#endif

    // Serial output config: clock period of 5 cycles, pop from command
    // expander every 5 cycles, shift the output shiftreg by 2 every cycle.
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // Note we are leaving the HSTX clock at the SDK default of 125 MHz; since
    // we shift out two bits per HSTX clock cycle, this gives us an output of
    // 250 Mbps, which is very close to the bit clock for 480p 60Hz (252 MHz).
    // If we want the exact rate then we'll have to reconfigure PLLs.

    // HSTX outputs 0 through 7 appear on GPIO 12 through 19.
    // Pinout on Pico DVI sock:
    //
    //   GP12 D0+  GP13 D0-
    //   GP14 CK+  GP15 CK-
    //   GP16 D2+  GP17 D2-
    //   GP18 D1+  GP19 D1-

    // Assign clock pair to two neighbouring pins:
    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    for (uint lane = 0; lane < 3; ++lane) {
        // For each TMDS lane, assign it to the correct GPIO pair based on the
        // desired pinout:
        static const int lane_to_output_bit[3] = {0, 6, 4};
        int bit = lane_to_output_bit[lane];
        // Output even bits during first half of each HSTX cycle, and odd bits
        // during second half. The shifter advances by two bits each cycle.
        uint32_t lane_data_sel_bits =
            (lane * 10    ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        // The two halves of each pair get identical data, but one pin is inverted.
        hstx_ctrl_hw->bit[bit    ] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }


    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, 0); // HSTX
    }


    // Both channels are set up identically, to transfer a whole scanline and
    // then chain to the opposite channel. Each time a channel finishes, we
    // reconfigure the one that just finished, meanwhile the opposite channel
    // is already making progress.
    dma_channel_config c;
    c = dma_channel_get_default_config(DMACH_PING);
    channel_config_set_chain_to(&c, DMACH_PONG);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        DMACH_PING,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );
    c = dma_channel_get_default_config(DMACH_PONG);
    channel_config_set_chain_to(&c, DMACH_PING);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        DMACH_PONG,
        &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

// #ifndef IGNORE_THIS
// IGNORE_THIS works when here


    // Modify DMA Interrupt Status for IRQ 0 - clear any pending DMACH_P*NG interrupts, I think
    dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG);

// #ifndef IGNORE_THIS
// IGNORE_THIS works when here

    // Modify DMA Interrupt Enables for IRQ 0 -  pass interrupts from DMACH_P*NG channels to DMA IRQ 0, I think
    dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG);

// #ifndef IGNORE_THIS
// IGNORE_THIS starts to fail when here
// Some of the VGA screen is painted but not the samples, so maybe the sampling DMA stuff is messing with this DVI DMA stuff

    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);

// #ifndef IGNORE_THIS
// IGNORE_THIS completely fails when here

    irq_set_enabled(DMA_IRQ_0, true);

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    dma_channel_start(DMACH_PING);

// #endif

}
