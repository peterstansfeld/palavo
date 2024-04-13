
// VGA_USE_PIO_PROG defines which VGA driver should be used to drive the VGA port

// If it's 1 Hunter Adams' VGA driver is used on HSYNC, VSYNC, LO_GRN, etc.
// otherwise its output will be on HSYNC2, VSYNC2, LO_GRN2, etc. and another
// VGA driver will be used on HSYNC, VSYNC, LO_GRN, etc.

#define VGA_USE_PIO_PROG 1


// VGA_TEST_PIO_PROG defines which VGA driver should be used to drive the VGA
// test pins on HSYNC2, VSYNC2, LO_GRN2, etc.

#define VGA_TEST_PIO_PROG 4


#if (VGA_USE_PIO_PROG != 1) && (VGA_TEST_PIO_PROG != 1) 

    #error Either VGA_USE_PIO_PROG or VGA_TEST_PIO_PROG must be 1.

#elif (VGA_USE_PIO_PROG == 1) && (VGA_TEST_PIO_PROG == 1)

    #error VGA_USE_PIO_PROG and VGA_TEST_PIO_PROG cannot both be 1.

#endif

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
// Our assembled programs:
// Each gets the name <pio_filename.pio.h>
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"

#if (VGA_USE_PIO_PROG == 2) || (VGA_TEST_PIO_PROG == 2)

#include "hsync2.pio.h"
#include "vsync2.pio.h"
#include "rgb2.pio.h"

#elif (VGA_USE_PIO_PROG == 3) || (VGA_TEST_PIO_PROG == 3)

#include "hsync3.pio.h"
#include "rgb3.pio.h"

#elif (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)

#include "hsync4.pio.h"
#include "rgb4.pio.h"

#endif

// Header file
#include "vga16_graphics.h"
// Font file
#include "glcdfont.c"
#include "font_rom_brl4.h"

// VGA timing constants
#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
#define V_ACTIVE   479    // (active - 1)
#define RGB_ACTIVE 319    // (horizontal active)/2 - 1
// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte

// Length of the pixel array, and number of DMA transfers
#define TXCOUNT 153600 // Total pixels/2 (since we have 2 pixels per byte)

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)
unsigned char vga_data_array[TXCOUNT];
char * address_pointer = &vga_data_array[0] ;

#if (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)

// #define SYNC_BUFFER_COUNT 14
#define SYNC_BUFFER_COUNT 7

uint32_t sync_buffer[SYNC_BUFFER_COUNT];
uint32_t * sync_buffer_address_pointer = &sync_buffer[0] ;
#endif

// Bit masks for drawPixel routine
#define TOPMASK 0b00001111
#define BOTTOMMASK 0b11110000

// For drawLine
#define swap(a, b) { short t = a; a = b; b = t; }

// For writing text
#define tabspace 4 // number of spaces for a tab

// For accessing the font library
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

// For drawing characters
unsigned short cursor_y, cursor_x, textsize ;
char textcolor, textbgcolor, wrap;

unsigned char str_cursor_x;

// Screen width/height
#define _width 640
#define _height 480

void initVGA() {
        // Choose which PIO instance to use (there are two instances, each with 4 state machines)
    PIO pio = pio0;

    PIO pio_2 = pio1;


    // Our assembled program needs to be loaded into this PIO's instruction
    // memory. This SDK function will find a location (offset) in the
    // instruction memory where there is enough space for our program. We need
    // to remember these locations!
    //
    // We only have 32 instructions to spend! If the PIO programs contain more than
    // 32 instructions, then an error message will get thrown at these lines of code.
    //
    // The program name comes from the .program part of the pio file
    // and is of the form <program name_program>
    uint hsync_offset = pio_add_program(pio, &hsync_program);
    uint vsync_offset = pio_add_program(pio, &vsync_program);
    uint rgb_offset = pio_add_program(pio, &rgb_program);

    // uint hsync2_offset = pio_add_program(pio_2, &hsync2_program);
    // uint vsync2_offset = pio_add_program(pio_2, &vsync2_program);
    // uint rgb3_offset = pio_add_program(pio_2, &rgb3_program);
    // uint hsync3_offset = pio_add_program(pio_2, &hsync3_program);

    // Manually select a few state machines from pio instance pio0.
    uint hsync_sm = 0;
    uint vsync_sm = 1;
    uint rgb_sm = 2;

    // uint hsync2_sm = 0;
    // uint vsync2_sm = 1;
    // uint rgb2_sm = 2;
    // uint hsync3_sm = 3;

    // Manually select a couple of state machines from pio instance pio1

#if (VGA_USE_PIO_PROG == 2) || (VGA_TEST_PIO_PROG == 2)

    uint hsync2_offset = pio_add_program(pio_2, &hsync2_program);
    uint vsync2_offset = pio_add_program(pio_2, &vsync2_program);
    uint rgb2_offset = pio_add_program(pio_2, &rgb2_program);

    uint hsync2_sm = 0;
    uint vsync2_sm = 1;
    uint rgb2_sm = 2;
    
#elif (VGA_USE_PIO_PROG == 3) || (VGA_TEST_PIO_PROG == 3)
    
    uint hsync3_sm = 0;
    uint rgb3_sm = 1;

    uint rgb3_offset = pio_add_program(pio_2, &rgb3_program);
    uint hsync3_offset = pio_add_program(pio_2, &hsync3_program);

#elif (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)
    
    uint hsync4_sm = 0;
    uint rgb4_sm = 1;

    uint rgb4_offset = pio_add_program(pio_2, &rgb4_program);
    uint hsync4_offset = pio_add_program(pio_2, &hsync4_program);

#endif
    
    // Call the initialization functions that are defined within each PIO file.
    // Why not create these programs here? By putting the initialization function in
    // the pio file, then all information about how to use/setup that state machine
    // is consolidated in one place. Here in the C, we then just import and use it.
    
#if VGA_USE_PIO_PROG == 1
    
    hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC);
    vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC);
    rgb_program_init(pio, rgb_sm, rgb_offset, LO_GRN);

    #if VGA_TEST_PIO_PROG == 2

        hsync2_program_init(pio_2, hsync2_sm, hsync2_offset, HSYNC2);
        vsync2_program_init(pio_2, vsync2_sm, vsync2_offset, VSYNC2);
        rgb2_program_init(pio_2, rgb2_sm, rgb2_offset, LO_GRN2);

    #elif VGA_TEST_PIO_PROG == 3
    
        hsync3_program_init(pio_2, hsync3_sm, hsync3_offset, HSYNC2, VSYNC2); // this has to be the first (not true, it must have .org 0)
        rgb3_program_init(pio_2, rgb3_sm, rgb3_offset, LO_GRN2 /*, HI_GRN2*/); // 

    #elif VGA_TEST_PIO_PROG == 4

        hsync4_program_init(pio_2, hsync4_sm, hsync4_offset, HSYNC2, VSYNC2); // this has to be the first (not true, it must have .org 0)
        rgb4_program_init(pio_2, rgb4_sm, rgb4_offset, LO_GRN2 /*, HI_GRN2*/); // 

    #endif

#else
    // VGA_USE_PIO_PROG != 1, output it on test pins HSYNC2, HSYNC2, VSYNC2 & LO_GRN2 etc.
    hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC2);
    vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC2);
    rgb_program_init(pio, rgb_sm, rgb_offset, LO_GRN2);

    // and use other pio programs to drive the VGA monitor
    #if VGA_USE_PIO_PROG == 2
      hsync2_program_init(pio_2, hsync2_sm, hsync2_offset, HSYNC);
      vsync2_program_init(pio_2, vsync2_sm, vsync2_offset, VSYNC);
      rgb2_program_init(pio_2, rgb2_sm, rgb2_offset, LO_GRN);
    
    #elif VGA_USE_PIO_PROG == 3
      hsync3_program_init(pio_2, hsync3_sm, hsync3_offset, HSYNC, VSYNC);
      rgb3_program_init(pio_2, rgb3_sm, rgb3_offset, LO_GRN);

    #elif VGA_USE_PIO_PROG == 4
      hsync4_program_init(pio_2, hsync4_sm, hsync4_offset, HSYNC, VSYNC);
      rgb4_program_init(pio_2, rgb4_sm, rgb4_offset, LO_GRN);

    #endif


#endif


    // hsync3_program_init(pio_2, hsync3_sm, hsync3_offset, HI_GRN2);


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ============================== PIO DMA Channels =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // DMA channels - 0 sends color data, 1 reconfigures and restarts 0
    int rgb_chan_0 = dma_claim_unused_channel(true);
    int rgb_chan_1 = dma_claim_unused_channel(true);

    // Channel Zero (sends color data to PIO VGA machine)
    dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              // 8-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing
    channel_config_set_dreq(&c0, DREQ_PIO0_TX2) ;                        // DREQ_PIO0_TX2 pacing (FIFO)
    channel_config_set_chain_to(&c0, rgb_chan_1);                        // chain to other channel

     dma_channel_configure(
        rgb_chan_0,                 // Channel to be configured
        &c0,                        // The configuration we just created
        &pio->txf[rgb_sm],          // write address (RGB PIO TX FIFO)
        &vga_data_array,            // The initial read address (pixel color array)
        TXCOUNT,                    // Number of transfers; in this case each is 1 byte.
        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, rgb_chan_0);                         // chain to other channel

    dma_channel_configure(
        rgb_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[rgb_chan_0].read_addr,  // Write address (channel 0 read address)
        &address_pointer,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );


    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0
    int rgb_test_chan_0 = dma_claim_unused_channel(true);
    int rgb_test_chan_1 = dma_claim_unused_channel(true);

    // Channel Zero (sends color data to PIO VGA machine)
    c0 = dma_channel_get_default_config(rgb_test_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);              // 8-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing

#if (VGA_USE_PIO_PROG == 2) || (VGA_TEST_PIO_PROG == 2)
    channel_config_set_dreq(&c0, DREQ_PIO1_TX2) ;                        // DREQ_PIO1_TX2 pacing (FIFO)
#elif (VGA_USE_PIO_PROG == 3) || (VGA_TEST_PIO_PROG == 3)
    channel_config_set_dreq(&c0, DREQ_PIO1_TX1) ;                        // DREQ_PIO1_TX1 pacing (FIFO)
#elif (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)
    channel_config_set_dreq(&c0, DREQ_PIO1_TX1) ;                        // DREQ_PIO1_TX1 pacing (FIFO)
#endif

    channel_config_set_chain_to(&c0, rgb_test_chan_1);                        // chain to other channel

     dma_channel_configure(
        rgb_test_chan_0,                 // Channel to be configured
        &c0,                        // The configuration we just created

#if (VGA_USE_PIO_PROG == 2) || (VGA_TEST_PIO_PROG == 2)
        &pio_2->txf[rgb2_sm],          // write address (RGB PIO TX FIFO)
#elif (VGA_USE_PIO_PROG == 3) || (VGA_TEST_PIO_PROG == 3)
        &pio_2->txf[rgb3_sm],          // write address (RGB PIO TX FIFO)
#elif (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)
        &pio_2->txf[rgb4_sm],          // write address (RGB PIO TX FIFO)
#endif

// #endif
        &vga_data_array,            // The initial read address (pixel color array)
        TXCOUNT,                    // Number of transfers; in this case each is 1 byte.
        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    c1 = dma_channel_get_default_config(rgb_test_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, rgb_test_chan_0);                         // chain to other channel

    dma_channel_configure(
        rgb_test_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[rgb_test_chan_0].read_addr,  // Write address (channel 0 read address)
        &address_pointer,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );



#if (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)

    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0
    int sync_test_chan_0 = dma_claim_unused_channel(true);
    int sync_test_chan_1 = dma_claim_unused_channel(true);

    // Channel Zero (sends color data to PIO VGA machine)
    c0 = dma_channel_get_default_config(sync_test_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);             // 32-bit txfers

    // don'tactually need the following two as they are the default values
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing

    // Wrap read address on 4 word boundary
    // channel_config_set_ring(&c0, false, 3); // 2 stops it working. 3 gives us only HSYNC working  as expected

    // channel_config_set_ring(&c0, false, 5); // 0 (default) ignores it. 
    // 2 stops it working.
    // 3 gives us only HSYNC working as expected
    // 4 seems to work but doesn't wrap by itself - it still seems to need chaining, it does
    //   as the ring only repeats until SYNC_BUFFER_COUNT is decremented to 0

    // channel_config_set_dreq(&c0, DREQ_PIO1_TX0) ;                        // DREQ_PIO1_TX0 pacing (FIFO)
    channel_config_set_dreq(&c0, pio_get_dreq(pio_2, hsync4_sm, true));     // hsync4_sm tx FIFO pacing

    channel_config_set_chain_to(&c0, sync_test_chan_1);                  // chain to other channel

     dma_channel_configure(
        sync_test_chan_0,           // Channel to be configured
        &c0,                        // The configuration we just created
        &pio_2->txf[hsync4_sm],     // write address (RGB PIO TX FIFO)
        &sync_buffer,            // The initial read address (pixel color array)
        // SYNC_BUFFER_COUNT * 200 * 60 * 5,                    // Number of transfers; in this case each is 1 32-bit word.
        SYNC_BUFFER_COUNT,                    // Number of transfers; in this case each is 1 32-bit word.

        false                       // Don't start immediately.
    );

    // Channel One (reconfigures the first channel)
    c1 = dma_channel_get_default_config(sync_test_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, sync_test_chan_0);                         // chain to other channel

    dma_channel_configure(
        sync_test_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[sync_test_chan_0].read_addr,  // Write address (channel 0 read address)
        &sync_buffer_address_pointer,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

    // put some data in the buffer
/*
    for (int i = 0; i < SYNC_BUFFER_COUNT; i++) {
        // sync_buffer[i] = 0x55aa55aa;
        // sync_buffer[i] = 0x55555555;
        // sync_buffer[i] = 0x12345678;
        sync_buffer[i] = i;
    }
*/



/*

    sync_buffer[0] = ((88 - 4) << 2) | 0b11;
    sync_buffer[1] = ((12 - 4) << 2) | 0b10;
    sync_buffer[2] = ((88 - 4) << 2) | 0b11;
    sync_buffer[3] = ((12 - 4) << 2) | 0b10;

    // these should be ignored if we set the ring to (1 << 4) = 16 (bytes)) - they are
    sync_buffer[4] = ((88 - 4) << 2) | 0b11;
    sync_buffer[5] = ((12 - 4) << 2) | 0b10;
    sync_buffer[6] = ((6 - 4) << 2)  | 0b11;

    sync_buffer[7] = ((88 - 6 - 4) << 2) | 0b01;
    sync_buffer[8] = ((12 - 4) << 2) | 0b00;

    sync_buffer[9] = ((88 - 4) << 2) | 0b01;
    sync_buffer[10] = ((12 - 4) << 2) | 0b00;

    sync_buffer[11] = ((6 - 4) << 2) | 0b01;
    sync_buffer[12] = ((88 - 6 - 4) << 2) | 0b11;
    sync_buffer[13] = ((12 - 4) << 2) | 0b10;

*/


    // uint32_t encode(char delay1, bool vsync1, bool hsync1, char delay0,  bool vsync0, bool hsync0, int repeat) {
        // return ((delay1 - 6) << 2 | vsync1 << 1 | hsync1) << 16 | (delay0 - 5) << 2 | vsync0 << 1 | hsync0;
        // return ((delay1 - 5) << 2 | vsync1 << 1 | hsync1) << 16 | (delay0 - 5) << 2 | vsync0 << 1 | hsync0;
        // return ((delay1 - 6) << 2 | vsync1 << 1 | hsync1) << 16 | repeat << 9 | (delay0 - 5) << 2 | vsync0 << 1 | hsync0;
        // return ((delay1 - 6) << 2 | vsync1 << 1 | hsync1) << 16 | (delay0 - 5) << 9 | vsync0 << 8 | hsync0 << 7 | repeat;
        // return ((delay1 - 9) << 3 | vsync1 << 1 | hsync1) << 19 | repeat << 10 | (delay0 - 6) << 2 | vsync0 << 1 | hsync0;
    //  }

    uint32_t encode(int repeat, bool vsync0, bool hsync0, char delay0, bool irq, bool vsync1, bool hsync1, char delay1) {
        return ((delay1 - 8 - irq) << 3 | irq << 2 | vsync1 << 1 | hsync1) << 19 | repeat << 10 | (delay0 - 6) << 2 | vsync0 << 1 | hsync0;
        // return ((delay0 - 9) << 3 | irq << 2 | vsync0 << 1 | hsync0) << 19 | repeat << 10 | (delay1 - 6) << 2 | vsync1 << 1 | hsync1;
     
     }

    // sync_buffer[0] = (((12 - 4 - 2) << 2) | 0b10) << 16 | (88 - 4 - 1) << 2 | 0b11;
    // sync_buffer[1] = (((12 - 4 - 2) << 2) | 0b10) << 16 | (88 - 4 - 1) << 2 | 0b11;
    // sync_buffer[2] = (((12 - 4 - 2) << 2) | 0b10) << 16 | (88 - 4 - 1) << 2 | 0b11;
    // sync_buffer[3] = (((88 - 6 - 4 - 2) << 2) | 0b01) << 16 | (6 - 4 - 1) << 2 | 0b11;
    // sync_buffer[4] = (((88 - 4 - 2) << 2) | 0b01) << 16 | (12 - 4 - 1) << 2 | 0b00;
    // sync_buffer[5] = (((6 - 4 - 2) << 2) | 0b01) << 16 | (12 - 4 - 1) << 2 | 0b00;
    // sync_buffer[6] = (((12 - 4 - 2) << 2) | 0b10) << 16 | (88 - 6 - 4 - 1) << 2 | 0b11;

    // sync_buffer[0] = encode(12, 1, 0, 88, 1, 1, 127);
    // sync_buffer[1] = encode(12, 1, 0, 88, 1, 1, 127);
    // sync_buffer[2] = encode(12, 1, 0, 88, 1, 1, 127);
    // sync_buffer[3] = encode(12, 1, 0, 88, 1, 1, 127);
    // sync_buffer[4] = encode(12, 1, 0, 88, 1, 1, 9);
    // sync_buffer[5] = encode(88 - 6, 0, 1, 6, 1, 1, 0);
    // sync_buffer[6] = encode(88, 0, 1, 12, 0, 0, 0);
    // sync_buffer[7] = encode(6, 0, 1, 12, 0, 0, 0);
    // sync_buffer[8] = encode(12, 1, 0, 88 - 6, 1, 1, 0);

    // sync_buffer[0] = encode(12, 1, 0,       88,     1, 1, 511);
    // sync_buffer[1] = encode(12, 1, 0,       88,     1, 1, 9);
    // sync_buffer[2] = encode(88 - 6, 0, 1,    6,     1, 1, 0);
    // sync_buffer[3] = encode(88, 0, 1,       12,     0, 0, 0);
    // sync_buffer[4] = encode(6, 0, 1,        12,     0, 0, 0);
    // sync_buffer[5] = encode(12, 1, 0,       88 - 6, 1, 1, 0);

    // sync_buffer[0] = encode(24, 1, 1, 0,       176,     1, 1, 479);
    // sync_buffer[1] = encode(24, 0, 1, 0,       176,     1, 1, 39);
    // sync_buffer[2] = encode(24, 0, 1, 0,       176,     1, 1, 0);
    // sync_buffer[3] = encode(176 - 12, 0, 0, 1,  12,     1, 1, 0);
    // sync_buffer[4] = encode(176, 0, 0, 1,       24,     0, 0, 0);
    // sync_buffer[5] = encode(12, 0, 0, 1,        24,     0, 0, 0);
    // sync_buffer[6] = encode(24, 0, 1, 0,       176 - 12, 1, 1, 0);

    // sync_buffer[0] = encode(24,       1, 1, 0,       176,       1, 1, 479);
    // sync_buffer[1] = encode(24,       0, 1, 0,       176,       1, 1, 39 );
    // sync_buffer[2] = encode(24,       0, 1, 0,       176,       1, 1,   0);
    // sync_buffer[3] = encode(176 - 12, 0, 0, 1,       12,        1, 1,   0);
    // sync_buffer[4] = encode(176,      0, 0, 1,       24,        0, 0,   0);
    // sync_buffer[5] = encode(12,       0, 0, 1,       24,        0, 0,   0);
    // sync_buffer[6] = encode(24,       0, 1, 0,       176 - 12,  1, 1,   0);

    // sync_buffer[0] = encode(176,      1, 1, 1,       24,        1, 0, 479);
    // sync_buffer[1] = encode(176,      0, 1, 1,       24,        1, 0, 39 );
    // sync_buffer[2] = encode(176,      0, 1, 1,       24,        1, 0, 0  );
    // sync_buffer[3] = encode(12,       0, 1, 0,       176 - 12,  0, 1, 0  );
    // sync_buffer[4] = encode(24,       0, 0, 0,       176,       0, 1, 0  );
    // sync_buffer[5] = encode(24,       0, 0, 0,       12,        0, 1, 0  );
    // sync_buffer[6] = encode(176 - 12, 0, 1, 1,       24,        1, 0, 0  );

    // sync_buffer[0] = encode(24,       1, 1, 0,       176,       1, 1, 479);
    // sync_buffer[1] = encode(24,       0, 1, 0,       176,       1, 1, 10  );
    // sync_buffer[2] = encode(176 - 12, 0, 0, 1,       12,        1, 1, 0  );
    // sync_buffer[3] = encode(176,      0, 0, 1,       24,        0, 0, 0  );
    // sync_buffer[4] = encode(12,       0, 0, 1,       24,        0, 0, 0  );
    // sync_buffer[5] = encode(24,       0, 1, 0,       176 - 12,  1, 1, 0  );
    // sync_buffer[6] = encode(24,       0, 1, 0,       176,       1, 1, 30  );



//                          delay     iq V  H        delay      V  H  rpt 
    // sync_buffer[0] = encode(176,      1, 1, 1,       24,        1, 0, 479);
    // sync_buffer[1] = encode(176,      0, 1, 1,       24,        1, 0, 9  );
    // sync_buffer[2] = encode(12,       0, 1, 1,       24,        1, 0, 0  );
    // sync_buffer[3] = encode(24,       0, 0, 0,       176 - 12,  0, 1, 0  );
    // sync_buffer[4] = encode(24,       0, 0, 0,       176,       0, 1, 0  );
    // sync_buffer[5] = encode(176 - 12, 0, 1, 1,       12,        0, 1, 0  );
    // sync_buffer[6] = encode(176,      0, 1, 1,       24,        1, 0, 31 );

//                          rpt    V  H, delay         irq V  H, delay
    sync_buffer[0] = encode(479,   1, 0, 24,           1,  1, 1, 176     );
    sync_buffer[1] = encode(9,     1, 0, 24,           0,  1, 1, 176     );
    sync_buffer[2] = encode(0,     1, 0, 24,           0,  1, 1, 12      );
    sync_buffer[3] = encode(0,     0, 1, 176 - 12,     0,  0, 0, 24      );
    sync_buffer[4] = encode(0,     0, 1, 176,          0,  0, 0, 24      );
    sync_buffer[5] = encode(0,     0, 1, 12,           0,  1, 1, 176 - 12);
    sync_buffer[6] = encode(31,    1, 0, 24,           0,  1, 1, 176     );


#endif

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // Initialize PIO state machine counters. This passes the information to the state machines
    // that they retrieve in the first 'pull' instructions, before the .wrap_target directive
    // in the assembly. Each uses these values to initialize some counting registers.
    
    // no longer need this as we've slowed the clock down to 1/16 of what it was before, and
    // we can achieve the required delays with 'set x (or y), (0..31)' instructions and instruction
    // '[delays]'
    
    
    // don't need this for the current, optimised h_sync, but will do when restoring 
    // Hunter's original driver. Let's try... yep, that still works. 
    
    // pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
    
    // shouldn't need these for the current, optimised  v_sync and rgb, but will do when restoring 
    // Hunter's original driver. Let's try... oops, that's not true. 
    
    // pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
    // pio_sm_put_blocking(pio, rgb_sm, RGB_ACTIVE);

    // pio_sm_put_blocking(pio_2, hsync2_sm, H_ACTIVE);

    // pio_sm_put_blocking(pio_2, vsync2_sm, V_ACTIVE);

    // tempted to move this lot to their respective .pio files in the initialisation function 
    // try one at a time to see if that works... It does, now try all of them (for hsync3)...
    
    /*
    pio_sm_put_blocking(pio_2, hsync3_sm, V_ACTIVE);

    pio_sm_exec(pio_2, hsync3_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)
    // pio_sm_exec(pio_2, hsync3_sm, pio_encode_mov(pio_isr, pio_osr)); // this fails for some reason!!!
    pio_sm_exec(pio_2, hsync3_sm, pio_encode_mov(pio_y, pio_osr)); // isr = V_ACTIVE
    pio_sm_exec(pio_2, hsync3_sm, pio_encode_mov(pio_isr, pio_y)); // isr = V_ACTIVE
    pio_sm_exec(pio_2, hsync3_sm, pio_encode_out(pio_null, 32)); // isr = V_ACTIVE
  */

  // great. that all worked, now try rgb2...
   

   /*
    pio_sm_put_blocking(pio_2, rgb2_sm, RGB_ACTIVE); // value to store in isr as a horizontal pixel pair counter
    pio_sm_put_blocking(pio_2, rgb2_sm, 480 - 1); //value to store in y as line counter
    */


    // this pio_sm_exec instruction saves one pio instruction
    // pio_sm_exec(pio, vsync_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)


    // pio_sm_exec(pio, hsync2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)


   //pio_sm_exec(pio_2, vsync2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)

    // these pio_sm_exec instructions save three pio instructions
    // pio_sm_exec(pio, rgb_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)

    // pio_sm_exec(pio_2, rgb2_sm, pio_encode_out(pio_isr, 32)); // store osr in isr a a loop counter
  
   /*
    pio_sm_exec(pio_2, rgb2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)
    pio_sm_exec(pio_2, rgb2_sm, pio_encode_out(pio_isr, 32)); // trigger auto-pull

    pio_sm_exec(pio_2, rgb2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)
    pio_sm_exec(pio_2, rgb2_sm, pio_encode_out(pio_y, 32)); // trigger auto-pull
    */

     // great. that all worked too.

    //pio_sm_exec(pio, rgb_sm, pio_encode_mov(pio_y, pio_osr)); // (IfE = 0, Blk = 1)
    //pio_sm_exec(pio, rgb_sm, pio_encode_jmp(rgb_offset + 0)); // (jmp 0)

    // Shift 32 bits of the osr (RGB_ACTIVE) into y, which also triggers
    // auto-pull once the dma is enabled. 
    // pio_sm_exec(pio, rgb_sm, pio_encode_out(pio_y, 32)); // trigger auto-pull

#if VGA_TEST_PIO_PROG == 2
    // pio_sm_put_blocking(pio_2, hsync2_sm, H_ACTIVE);  // don't think we need this    
    // pio_sm_put_blocking(pio_2, vsync2_sm, V_ACTIVE);
    // pio_sm_put_blocking(pio_2, rgb2_sm, RGB_ACTIVE); // will need this. no, they've been moved in .pio


    // pio_sm_exec(pio_2, vsync2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)

    // pio_sm_exec(pio_2, rgb2_sm, pio_encode_pull(false, true)); // will need this.  no, they've been moved in .pio
    // pio_sm_exec(pio_2, rgb2_sm, pio_encode_out(pio_y, 32)); // // will need this. no, they've been moved in .pio
#endif

    // Start the two pio machine IN SYNC
    // Note that the RGB state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.
    pio_enable_sm_mask_in_sync(pio, ((1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm)));


#if (VGA_USE_PIO_PROG == 2) || (VGA_TEST_PIO_PROG == 2)

    pio_enable_sm_mask_in_sync(pio_2, ((1u << hsync2_sm) | (1u << vsync2_sm) | (1u << rgb2_sm)));

#elif (VGA_USE_PIO_PROG == 3) || (VGA_TEST_PIO_PROG == 3)

    pio_enable_sm_mask_in_sync(pio_2, ((1u << hsync3_sm) | (1u << rgb3_sm)));

#elif (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)

    pio_enable_sm_mask_in_sync(pio_2, ((1u << hsync4_sm) | (1u << rgb4_sm)));

#endif

    // Start DMA channel 0. Once started, the contents of the pixel color array
    // will be continously DMA's to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
    dma_start_channel_mask((1u << rgb_chan_0)) ;
    dma_start_channel_mask((1u << rgb_test_chan_0)) ;

#if (VGA_USE_PIO_PROG == 4) || (VGA_TEST_PIO_PROG == 4)
    dma_start_channel_mask((1u << sync_test_chan_0)) ;
#endif

}


// A function for drawing a pixel with a specified color.
// Note that because information is passed to the PIO state machines through
// a DMA channel, we only need to modify the contents of the array and the
// pixels will be automatically updated on the screen.
void drawPixel(short x, short y, char color) {
    // Range checks (640x480 display)
    
    // if (x > 639) x = 639 ;
    // if (x < 0) x = 0 ;
    // if (y < 0) y = 0 ;
    // if (y > 479) y = 479 ;

    if ((x > 639) || (x < 0) || (y > 479) || (y < 0))
        return;

    // Which pixel is it?
    int pixel = ((640 * y) + x) ;

    // Is this pixel stored in the first 4 bits
    // of the vga data array index, or the second
    // 4 bits? Check, then mask.
    if (pixel & 1) {
        vga_data_array[pixel>>1] = (vga_data_array[pixel>>1] & TOPMASK) | (color << 4) ;
    }
    else {
        vga_data_array[pixel>>1] = (vga_data_array[pixel>>1] & BOTTOMMASK) | (color) ;
    }
}

void drawVLine(short x, short y, short h, char color) {
    for (short i=y; i<(y+h); i++) {
        drawPixel(x, i, color) ;
    }
}

void drawHLine(short x, short y, short w, char color) {
    for (short i=x; i<(x+w); i++) {
        drawPixel(i, y, color) ;
    }
}

// Bresenham's algorithm - thx wikipedia and thx Bruce!
void drawLine(short x0, short y0, short x1, short y1, char color) {
/* Draw a straight line from (x0,y0) to (x1,y1) with given color
 * Parameters:
 *      x0: x-coordinate of starting point of line. The x-coordinate of
 *          the top-left of the screen is 0. It increases to the right.
 *      y0: y-coordinate of starting point of line. The y-coordinate of
 *          the top-left of the screen is 0. It increases to the bottom.
 *      x1: x-coordinate of ending point of line. The x-coordinate of
 *          the top-left of the screen is 0. It increases to the right.
 *      y1: y-coordinate of ending point of line. The y-coordinate of
 *          the top-left of the screen is 0. It increases to the bottom.
 *      color: 3-bit color value for line
 */
      short steep = abs(y1 - y0) > abs(x1 - x0);
      if (steep) {
        swap(x0, y0);
        swap(x1, y1);
      }

      if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
      }

      short dx, dy;
      dx = x1 - x0;
      dy = abs(y1 - y0);

      short err = dx / 2;
      short ystep;

      if (y0 < y1) {
        ystep = 1;
      } else {
        ystep = -1;
      }

      for (; x0<=x1; x0++) {
        if (steep) {
          drawPixel(y0, x0, color);
        } else {
          drawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
          y0 += ystep;
          err += dx;
        }
      }
}

// Draw a rectangle
void drawRect(short x, short y, short w, short h, char color) {
/* Draw a rectangle outline with top left vertex (x,y), width w
 * and height h at given color
 * Parameters:
 *      x:  x-coordinate of top-left vertex. The x-coordinate of
 *          the top-left of the screen is 0. It increases to the right.
 *      y:  y-coordinate of top-left vertex. The y-coordinate of
 *          the top-left of the screen is 0. It increases to the bottom.
 *      w:  width of the rectangle
 *      h:  height of the rectangle
 *      color:  16-bit color of the rectangle outline
 * Returns: Nothing
 */
  drawHLine(x, y, w, color);
  drawHLine(x, y+h-1, w, color);
  drawVLine(x, y, h, color);
  drawVLine(x+w-1, y, h, color);
}

void drawCircle(short x0, short y0, short r, char color) {
/* Draw a circle outline with center (x0,y0) and radius r, with given color
 * Parameters:
 *      x0: x-coordinate of center of circle. The top-left of the screen
 *          has x-coordinate 0 and increases to the right
 *      y0: y-coordinate of center of circle. The top-left of the screen
 *          has y-coordinate 0 and increases to the bottom
 *      r:  radius of circle
 *      color: 16-bit color value for the circle. Note that the circle
 *          isn't filled. So, this is the color of the outline of the circle
 * Returns: Nothing
 */
  short f = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x = 0;
  short y = r;

  drawPixel(x0  , y0+r, color);
  drawPixel(x0  , y0-r, color);
  drawPixel(x0+r, y0  , color);
  drawPixel(x0-r, y0  , color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void drawCircleHelper( short x0, short y0, short r, unsigned char cornername, char color) {
// Helper function for drawing circles and circular objects
  short f     = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x     = 0;
  short y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void fillCircle(short x0, short y0, short r, char color) {
/* Draw a filled circle with center (x0,y0) and radius r, with given color
 * Parameters:
 *      x0: x-coordinate of center of circle. The top-left of the screen
 *          has x-coordinate 0 and increases to the right
 *      y0: y-coordinate of center of circle. The top-left of the screen
 *          has y-coordinate 0 and increases to the bottom
 *      r:  radius of circle
 *      color: 16-bit color value for the circle
 * Returns: Nothing
 */
  drawVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

void fillCircleHelper(short x0, short y0, short r, unsigned char cornername, short delta, char color) {
// Helper function for drawing filled circles
  short f     = 1 - r;
  short ddF_x = 1;
  short ddF_y = -2 * r;
  short x     = 0;
  short y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

// Draw a rounded rectangle
void drawRoundRect(short x, short y, short w, short h, short r, char color) {
/* Draw a rounded rectangle outline with top left vertex (x,y), width w,
 * height h and radius of curvature r at given color
 * Parameters:
 *      x:  x-coordinate of top-left vertex. The x-coordinate of
 *          the top-left of the screen is 0. It increases to the right.
 *      y:  y-coordinate of top-left vertex. The y-coordinate of
 *          the top-left of the screen is 0. It increases to the bottom.
 *      w:  width of the rectangle
 *      h:  height of the rectangle
 *      color:  16-bit color of the rectangle outline
 * Returns: Nothing
 */
  // smarter version
  drawHLine(x+r  , y    , w-2*r, color); // Top
  drawHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawVLine(x    , y+r  , h-2*r, color); // Left
  drawVLine(x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// Fill a rounded rectangle
void fillRoundRect(short x, short y, short w, short h, short r, char color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}


// fill a rectangle
void fillRect(short x, short y, short w, short h, char color) {
/* Draw a filled rectangle with starting top-left vertex (x,y),
 *  width w and height h with given color
 * Parameters:
 *      x:  x-coordinate of top-left vertex; top left of screen is x=0
 *              and x increases to the right
 *      y:  y-coordinate of top-left vertex; top left of screen is y=0
 *              and y increases to the bottom
 *      w:  width of rectangle
 *      h:  height of rectangle
 *      color:  3-bit color value
 * Returns:     Nothing
 */

  // rudimentary clipping (drawChar w/big text requires this)
  // if((x >= _width) || (y >= _height)) return;
  // if((x + w - 1) >= _width)  w = _width  - x;
  // if((y + h - 1) >= _height) h = _height - y;

  // tft_setAddrWindow(x, y, x+w-1, y+h-1);

  for(int i=x; i<(x+w); i++) {
    for(int j=y; j<(y+h); j++) {
        drawPixel(i, j, color);
    }
  }
}

// Draw a character
void drawChar(short x, short y, unsigned char c, char color, char bg, unsigned char size) {
    char i, j;
  if((x >= _width)            || // Clip right
     (y >= _height)           || // Clip bottom
     ((x + 6 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0))   // Clip top
    return;

  for (i=0; i<6; i++ ) {
    unsigned char line;
    if (i == 5)
      line = 0x0;
    else
      line = pgm_read_byte(font+(c*5)+i);
    for ( j = 0; j<8; j++) {
      if (line & 0x1) {
        if (size == 1) // default size
          drawPixel(x+i, y+j, color);
        else {  // big size
          fillRect(x+(i*size), y+(j*size), size, size, color);
        }
      } else if (bg != color) {
        if (size == 1) // default size
          drawPixel(x+i, y+j, bg);
        else {  // big size
          fillRect(x+i*size, y+j*size, size, size, bg);
        }
      }
      line >>= 1;
    }
  }
}


inline void setCursor(short x, short y) {
/* Set cursor for text to be printed
 * Parameters:
 *      x = x-coordinate of top-left of text starting
 *      y = y-coordinate of top-left of text starting
 * Returns: Nothing
 */
  cursor_x = x;
  cursor_y = y;
}

inline void setTextSize(unsigned char s) {
/*Set size of text to be displayed
 * Parameters:
 *      s = text size (1 being smallest)
 * Returns: nothing
 */
  textsize = (s > 0) ? s : 1;
}

inline void setTextColor(char c) {
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  textcolor = textbgcolor = c;
}

inline void setTextColor2(char c, char b) {
/* Set color of text to be displayed
 * Parameters:
 *      c = 16-bit color of text
 *      b = 16-bit color of text background
 */
  textcolor   = c;
  textbgcolor = b;
}

inline void setTextWrap(char w) {
  wrap = w;
}


void tft_write(unsigned char c){
  if (c == '\n') {
    cursor_y += textsize*8;
    cursor_x  = str_cursor_x;
  } else if (c == '\r') {
    // skip em
  } else if (c == '\t'){
      int new_x = cursor_x + tabspace;
      if (new_x < _width){
          cursor_x = new_x;
      }
  } else {
    drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
    cursor_x += textsize*6;
    if (wrap && (cursor_x > (_width - textsize*6))) {
      cursor_y += textsize*8;
      cursor_x = 0;
    }
  }
}

inline void writeString(char* str){
/* Print text onto screen
 * Call tft_setCursor(), tft_setTextColor(), tft_setTextSize()
 *  as necessary before printing
 */
    str_cursor_x = cursor_x;
    while (*str){
        tft_write(*str++);
    }
}

//=================================================
// added 10/16/2023 brl4
inline void setTextColorBig(char color, char background) {
/* Set color of text to be displayed
 * Parameters:
 *      color = 16-bit color of text
 *      b = 16-bit color of text background
 *      background ==-1 means trasnparten background
 */
  textcolor   = color;
  textbgcolor = background;
}
//=================================================
// added 10/11/2023 brl4
// Draw a character
void drawCharBig(short x, short y, unsigned char c, char color, char bg) {
  char i, j ;
  unsigned char line; 
  for (i=0; i<15; i++ ) {   
    line = pgm_read_byte(bigFont+((int)c*16)+i);
    for ( j = 0; j<8; j++) {
      if (line & 0x80) {
        drawPixel(x+j, y+i, color);
      } else if (bg!=color){
        drawPixel(x+j, y+i, bg);
      }
      line <<= 1;
    }
  }
}

inline void writeStringBig(char* str){
/* Print text onto screen
 * Call tft_setCursor(), tft_setTextColorBig()
 *  as necessary before printing
 */
    while (*str){
      char c = *str++;
        drawCharBig(cursor_x, cursor_y, c, textcolor, textbgcolor);
        cursor_x += 8 ;
    }
}

inline void writeStringBold(char* str){
/* Print text onto screen
 * Call tft_setCursor(), tft_setTextColorBig()
 *  as necessary before printing
 */
   /* Print text onto screen
 * Call tft_setCursor(), tft_setTextColor(), tft_setTextSize()
 *  as necessary before printing
 */
    char temp_bg ;
    temp_bg = textbgcolor;
    while (*str){
        char c = *str++;
        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
        drawChar(cursor_x+1, cursor_y, c, textcolor, textcolor, textsize);
        cursor_x += 7 * textsize ;
    }
    textbgcolor = temp_bg ;
}
