// VGA_USE_PIO_PROG defines which VGA driver should be used to drive the VGA port

// If it's 1 Hunter Adams' VGA driver is used on HSYNC, VSYNC, LO_GRN, etc.
// otherwise its output will be on HSYNC2, VSYNC2, LO_GRN2, etc. and another
// VGA driver will be used on HSYNC, VSYNC, LO_GRN, etc.

// #define SYS_CLOCK_FREQ_KHZ 125000u



// VGA_TEST_PIO_PROG defines which VGA driver should be used to drive the VGA
// test pins on HSYNC2, VSYNC2, LO_GRN2, etc.

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "hardware/clocks.h"

#include "pico/binary_info.h"

// Header file - move this to before the *.pio.h files so that they can access
// SYS_CLOCK_FREQ_KHZ

#include "vga2_graphics.h"

// #include "hardware/irq.h"

// Our assembled programs:
// Each gets the name <pio_filename.pio.h>

#include "hsync5.pio.h"
#include "rgb5.pio.h"

#define USE_HSYNC_AND_VSYNC 0

#define USE_CSYNC 1


#if SYS_CLOCK_FREQ_KHZ == 250000u
// #define USE_4_BIT_RGB5_PIO

#ifdef USE_4_BIT_RGB5_PIO 

#if rgb5_250_mhz_BITS_PER_COLOUR != 2
    #error In rgb5.pio BITS_PER_COLOUR needs to be 2.
#endif

#elif rgb5_250_mhz_BITS_PER_COLOUR != 1
    #error In rgb5.pio BITS_PER_COLOUR needs to be 1.
#endif

#endif


// Font file
#include "glcdfont.c"
#include "font_rom_brl4.h"


#include <string.h>

// #define RGB_ACTIVE 639 // change to this if 1 pixel/byte

// Screen width/height
#define _width 640





// Length of the pixel array, and number of DMA transfers

// Pixel color array that is DMA's to the PIO machines and
// a pointer to the ADDRESS of this color array.
// Note that this array is automatically initialized to all 0's (black)

// #define SYNC_BUFFER_COUNT 14
#define SYNC_BUFFER_COUNT 8

// Maximum should last about 0xffffffff / (SYNC_BUFFER_COUNT * 24 * 60 * 60 * 60) = 103.56 days.
#define SYNC_DMA_TRANSFER_COUNT 0xffffffff

// #define SYNC_DMA_TRANSFER_COUNT SYNC_BUFFER_COUNT * 60 // one second (ish)
// #define SYNC_DMA_TRANSFER_COUNT SYNC_BUFFER_COUNT * 60 * 10 // ten seconds (ish)

// uint32_t sync_buffer [SYNC_BUFFER_COUNT];
uint32_t sync_buffer [SYNC_BUFFER_COUNT] __attribute__ ((aligned(SYNC_BUFFER_COUNT * sizeof(uint32_t))));

uint32_t * sync_buffer_address_pointer = &sync_buffer[0] ;

#ifdef USE_CSYNC

uint32_t csync_buffer [SYNC_BUFFER_COUNT] __attribute__ ((aligned(SYNC_BUFFER_COUNT * sizeof(uint32_t))));

uint32_t * csync_buffer_address_pointer = &csync_buffer[0];

#endif

 // (16 bits (639) + 16 bits (2, 4-bit colors) + (20 * 32 = 640 bits)

// any more than 48 and we get a weird vertical scrolling side-effet
// suggest that this is ram overflow 

uint32_t vga_1bit_data_array[TXCOUNT_2];
uint32_t * address_pointer_2 = &vga_1bit_data_array[0] ;


// vga_1bit_data_array[0] = 12;

// (639 << 16) | (RED << 4) | GREEN;

// vga_1bit_data_array[1] = 0xff00ff; //0b10101010110011001111000011111111;


// For drawLine
#define swap(a, b) { short t = a; a = b; b = t; }

// For writing text
#define tabspace 4 // number of spaces for a tab

// For accessing the font library
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

// For drawing characters
unsigned short cursor_y, cursor_x, textsize ;
char textcolor, textbgcolor, wrap;

uint16_t str_cursor_x;


// #define LED_PIN PICO_DEFAULT_LED_PIN

static int sync_test_chan_0 = 0;

#define USE_RING_BUF


void __not_in_flash_func(dma_handler)() {
// void dma_handler() {
    // gpio_put(LED_PIN, 0); // clear LED_PIN


    // if (dma_hw->ints0 & (1u << sync_test_chan_0)) {
    //     dma_hw->ints0 = 1u << sync_test_chan_0;

    // } else {

    //     dma_hw->ints0 = dma_hw->ints0;
    // }


    // Clear the interrupt request.
    dma_hw->ints0 = 1u << sync_test_chan_0;

    // Reload the transfer count
    dma_hw->ch[sync_test_chan_0].transfer_count = SYNC_DMA_TRANSFER_COUNT;

    // dma_hw->ch[sync_test_chan_0].transfer_count = 1; // this works - don't quite understand why
    // maybe it's because we're using ring and not resetting the read address each time

    dma_channel_start(sync_test_chan_0);
    // dma_channel_set_read_addr(sync_test_chan_0, &sync_buffer, true);

    // gpio_xor_mask(1 << LED_PIN);


}


void set_line_colors(uint16_t line, uint8_t back_colour, uint8_t fore_colour, uint8_t colour_2, uint8_t colour_3) {
    if ((line < 0) || (line >= NO_OF_LINES)) 
        return;
     vga_1bit_data_array[line * WORDS_PER_LINE] = (((fore_colour << 6) | (back_colour)) << 16) | 639;
}

void initVGA(uint csync_pin, uint rgb_base_pin) {
    // Choose which PIO instance to use (there are two instances (three for rp2350), each with 4 state machines)

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

    // uint hsync2_offset = pio_add_program(pio_2, &hsync2_program);
    // uint vsync2_offset = pio_add_program(pio_2, &vsync2_program);
    // uint rgb3_offset = pio_add_program(pio_2, &rgb3_program);
    // uint hsync3_offset = pio_add_program(pio_2, &hsync3_program);

    // Manually select a few state machines from pio instance pio0.

    // uint hsync2_sm = 0;
    // uint vsync2_sm = 1;
    // uint rgb2_sm = 2;
    // uint hsync3_sm = 3;

    // Manually select a couple of state machines from pio instance pio1

#if USE_HSYNC_AND_VSYNC

    uint hsync5_sm = 0;

#endif    
    
    uint rgb5_sm = 1;

#ifdef USE_CSYNC

    uint csync_sm = 2;

#endif

#if SYS_CLK_KHZ == 125000u
    uint rgb5_offset = pio_add_program(pio_2, &rgb5_program);
#elif SYS_CLK_KHZ == 150000u
    // uint rgb5_offset = pio_add_program(pio_2, &rgb5_150_mhz_program);
    uint rgb5_offset = pio_add_program(pio_2, &rgb5_150_mhz_rp235x_program);
#elif SYS_CLK_KHZ == 250000u
    uint rgb5_offset = pio_add_program(pio_2, &rgb5_250_mhz_program);
#endif

    uint hsync5_offset = pio_add_program(pio_2, &hsync5_program);

    // Call the initialization functions that are defined within each PIO file.
    // Why not create these programs here? By putting the initialization function in
    // the pio file, then all information about how to use/setup that state machine
    // is consolidated in one place. Here in the C, we then just import and use it.
    
#if USE_HSYNC_AND_VSYNC
      hsync5_program_init(pio_2, hsync5_sm, hsync5_offset, HSYNC2, 2);
#endif

      // bi_decl(bi_program_feature("This is a feature?"));

      // bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));
      // bi_decl(bi_2pins_with_func(12, 13, GPIO_FUNC_I2C));
      // bi_program_description("This is a program description");

      // bi_decl(bi_2pins_with_func(12, 13, GPIO_FUNC_HSTX));
      // bi_decl(bi_1pin_with_func(14, GPIO_FUNC_HSTX));

      // bi_decl(bi_pin_mask_with_name((1 << 27) | (1 << 26) | (1 << 5)| (1 << 4)| (1 << 3)| (1 << 2)| (1 << 1)| (1 << 0), "VGA In"));
      
      bi_decl(bi_1pin_with_name(0, "VGA In - Dark Blue"));
      bi_decl(bi_1pin_with_name(1, "VGA In - Light Blue"));
      bi_decl(bi_1pin_with_name(2, "VGA In - Dark Green"));
      bi_decl(bi_1pin_with_name(3, "VGA In - Light Green"));
      bi_decl(bi_1pin_with_name(4, "VGA In - Dark Red"));
      bi_decl(bi_1pin_with_name(5, "VGA In - Light Red"));
      bi_decl(bi_1pin_with_name(26, "VGA In - HSYNC / CSYNC"));
      bi_decl(bi_1pin_with_name(27, "VGA In - VSYNC"));

      // bi_decl(bi_pin_mask_with_name((1 << 22)| (1 << 11)| (1 << 10)| (1 << 9)| (1 << 8)| (1 << 7)| (1 << 6), "VGA Out"));
#if USE_HSYNC_AND_VSYNC
      bi_decl(bi_1pin_with_name(6, "HSYNC"));
      bi_decl(bi_1pin_with_name(7, "VSYNC"));
#endif
      // bi_decl(bi_1pin_with_name(8, "Dark Green"));
      // bi_decl(bi_1pin_with_name(9, "Light Green"));
      // bi_decl(bi_1pin_with_name(10, "Blue"));
      // bi_decl(bi_1pin_with_name(11, "Red"));
      // bi_decl(bi_1pin_with_name(11, "CSYNC"));

      bi_decl(bi_1pin_with_name(6, "VGA Out - Dark Blue"));
      bi_decl(bi_1pin_with_name(7, "VGA Out - Light Blue"));
      bi_decl(bi_1pin_with_name(8, "VGA Out - Dark Green"));
      bi_decl(bi_1pin_with_name(9, "VGA Out - Light Green"));
      bi_decl(bi_1pin_with_name(10, "VGA Out - Dark Red"));
      bi_decl(bi_1pin_with_name(11, "VGA Out - Light Red"));
      bi_decl(bi_1pin_with_name(22, "VGA Out - CSYNC"));


      // bi_decl(bi_pin_range_with_func(12, 19, GPIO_FUNC_HSTX));

      bi_decl(bi_1pin_with_name(12, "DVI Out - D0+"));
      bi_decl(bi_1pin_with_name(13, "DVI Out - D0-"));
      bi_decl(bi_1pin_with_name(14, "DVI Out - CK+"));
      bi_decl(bi_1pin_with_name(15, "DVI Out - CK-"));
      bi_decl(bi_1pin_with_name(16, "DVI Out - D1-"));
      bi_decl(bi_1pin_with_name(17, "DVI Out - D1+"));
      bi_decl(bi_1pin_with_name(18, "DVI Out - D2-"));
      bi_decl(bi_1pin_with_name(19, "DVI Out - D2+"));

      bi_decl(bi_2pins_with_func(20, 21, GPIO_FUNC_UART));

      bi_decl(bi_1pin_with_name(28, "IR RX"));
#ifdef USE_CSYNC


    // Initialise a second copy of hvsync which we'll call csync and use to test csync
    // uint csync_offset = pio_add_program(pio_2, &hsync5_program);
    hsync5_program_init(pio_2, csync_sm, hsync5_offset, csync_pin, 1);

#endif
// todo - tidy these GPIO pin definitions below

#if SYS_CLK_KHZ == 125000u
      rgb5_program_init(pio_2, rgb5_sm, rgb5_offset, LO_GRN);
#elif SYS_CLK_KHZ == 150000u
    rgb5_150_mhz_rp235x_program_init(pio_2, rgb5_sm, rgb5_offset, rgb_base_pin, 6);
#elif SYS_CLK_KHZ == 250000u
      rgb5_250_mhz_program_init(pio_2, rgb5_250_mhz_sm, rgb5_offset, LO_GRN);
#endif

#ifdef PICO_PLATFORM

#else
    // #warning no PICO_PLATFORM defined
#endif

#ifdef PICO_BOARD

#else
    #warning no PICO_BOARD defined
#endif




    // hsync3_program_init(pio_2, hsync3_sm, hsync3_offset, HI_GRN2);


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    // ============================== PIO DMA Channels =================================================
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    // DMA channels - 0 sends color data, 1 reconfigures and restarts 0

    dma_channel_config c0;
    dma_channel_config c1;

    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0
    int rgb_test_chan_0 = dma_claim_unused_channel(true);
    int rgb_test_chan_1 = dma_claim_unused_channel(true);

    // Channel Zero (sends color data to PIO VGA machine)
    c0 = dma_channel_get_default_config(rgb_test_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing

    channel_config_set_dreq(&c0, pio_get_dreq(pio_2, rgb5_sm, true));     // rgb5_sm tx FIFO pacing

    channel_config_set_chain_to(&c0, rgb_test_chan_1);                        // chain to other channel

     dma_channel_configure(
        rgb_test_chan_0,                 // Channel to be configured
        &c0,                        // The configuration we just created
        &pio_2->txf[rgb5_sm],          // write address (RGB PIO TX FIFO)
        &vga_1bit_data_array,       // The initial read address (pixel color array)
        TXCOUNT_2,                  // Number of transfers; in this case each is 4 byte.
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
        &address_pointer_2,                 // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

#if USE_HSYNC_AND_VSYNC

    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0
    int sync_test_chan_0 = dma_claim_unused_channel(true);

#ifndef USE_RING_BUF
    int sync_test_chan_1 = dma_claim_unused_channel(true);
#endif

    // Channel Zero (sends color data to PIO VGA machine)
    c0 = dma_channel_get_default_config(sync_test_chan_0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);             // 32-bit txfers

    //  Prevent this channel from generating IRQs at the end of every transfer block
    // channel_config_set_irq_quiet(&c0, true); 

    // don't actually need the following two as they are the default values
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing

    // Wrap read address on 4 word boundary
    // channel_config_set_ring(&c0, false, 3); // 2 stops it working. 3 gives us only HSYNC working  as expected

#ifdef USE_RING_BUF
    channel_config_set_ring(&c0, false, 5); // Set read address to wrap at (1<<5) = 32 byte boundary
#endif

    // 2 stops it working.
    // 3 gives us only HSYNC working as expected
    // 4 seems to work but doesn't wrap by itself - it still seems to need chaining, it does
    // as the ring only repeats until SYNC_BUFFER_COUNT is decremented to 0

    // channel_config_set_dreq(&c0, DREQ_PIO1_TX0) ;                        // DREQ_PIO1_TX0 pacing (FIFO)

    channel_config_set_dreq(&c0, pio_get_dreq(pio_2, hsync5_sm, true));     // hsync5_sm tx FIFO pacing

#ifndef USE_RING_BUF
    channel_config_set_chain_to(&c0, sync_test_chan_1);                  // chain to other channel
#endif

     dma_channel_configure(
        sync_test_chan_0,           // Channel to be configured
        &c0,                        // The configuration we just created
        &pio_2->txf[hsync5_sm],     // write address (RGB PIO TX FIFO)
        &sync_buffer,               // The initial read address (pixel color array)

#ifdef USE_RING_BUF
        SYNC_DMA_TRANSFER_COUNT,    // Number of transfers to perform
#else
        SYNC_BUFFER_COUNT,          // Number of transfers to perform
#endif
        false                       // Don't start immediately.
    );

#ifndef USE_RING_BUF
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

#endif

#endif 

#ifdef USE_CSYNC

    // More DMA channels - test ones - 0 sends color data, 1 reconfigures and restarts 0

    int csync_dma_chan0 = dma_claim_unused_channel(true);

#ifndef USE_RING_BUF
    int csync_dms_chan_1 = dma_claim_unused_channel(true);
    #warning 'were not using RING_BUF'
#endif

    // Channel Zero (sends color data to PIO VGA machine)
    c0 = dma_channel_get_default_config(csync_dma_chan0);  // default configs
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_32);             // 32-bit txfers

    //  Prevent this channel from generating IRQs at the end of every transfer block
    // channel_config_set_irq_quiet(&c0, true); 

    // don't actually need the following two as they are the default values
    channel_config_set_read_increment(&c0, true);                        // yes read incrementing
    channel_config_set_write_increment(&c0, false);                      // no write incrementing

    // Wrap read address on 4 word boundary
    // channel_config_set_ring(&c0, false, 3); // 2 stops it working. 3 gives us only HSYNC working  as expected

#ifdef USE_RING_BUF
    channel_config_set_ring(&c0, false, 5); // Set read address to wrap at (1<<5) = 32 byte boundary
#endif

    // 2 stops it working.
    // 3 gives us only HSYNC working as expected
    // 4 seems to work but doesn't wrap by itself - it still seems to need chaining, it does
    // as the ring only repeats until SYNC_BUFFER_COUNT is decremented to 0

    // channel_config_set_dreq(&c0, DREQ_PIO1_TX0) ;                        // DREQ_PIO1_TX0 pacing (FIFO)

    channel_config_set_dreq(&c0, pio_get_dreq(pio_2, csync_sm, true));     // hsync5_sm tx FIFO pacing

#ifndef USE_RING_BUF
    channel_config_set_chain_to(&c0, csync_dms_chan_1);                  // chain to other channel
#endif

     dma_channel_configure(
        csync_dma_chan0,           // Channel to be configured
        &c0,                        // The configuration we just created
        &pio_2->txf[csync_sm],      // write address (RGB PIO TX FIFO)
        &csync_buffer,               // The initial read address (pixel color array)

#ifdef USE_RING_BUF
        SYNC_DMA_TRANSFER_COUNT,    // Number of transfers to perform
#else
        SYNC_BUFFER_COUNT,          // Number of transfers to perform
#endif
        false                       // Don't start immediately.
    );

#ifndef USE_RING_BUF
    // Channel One (reconfigures the first channel)
    c1 = dma_channel_get_default_config(csync_dms_chan_1);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, csync_dma_chan0);                         // chain to other channel

    dma_channel_configure(
        csync_dms_chan_1,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[csync_dma_chan0].read_addr,  // Write address (channel 0 read address)
        &csync_buffer_address_pointer,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );

#endif

#endif

    // put some data in the buffer
/*
    for (int i = 0; i < SYNC_BUFFER_COUNT; i++) {
        // sync_buffer[i] = 0x55aa55aa;
        // sync_buffer[i] = 0x55555555;
        // sync_buffer[i] = 0x12345678;
        sync_buffer[i] = i;
    }
*/

    // Setup interrupt handler for sync_test_chan_0 DMA channels

    // dma_channel_set_irq0_enabled(sync_test_chan_0, true);
    // irq_set_exclusive_handler(DMA_IRQ_0, dma_irh);
    // irq_set_enabled(DMA_IRQ_0, true);






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
        return ((delay1 - 8 - irq) << 3 | vsync1 << 2 | hsync1 << 1 | irq) << 20 | (delay0 - 4) << 11 | vsync0 << 10 | hsync0 << 9 | repeat;
     }

//                          rpt    V  H, delay         irq V  H, delay
    sync_buffer[0] = encode(479,   1, 0, 24,           1,  1, 1, 176     );
    sync_buffer[1] = encode(9,     1, 0, 24,           0,  1, 1, 176     );
    sync_buffer[2] = encode(0,     1, 0, 24,           0,  1, 1, 12      );
    sync_buffer[3] = encode(0,     0, 1, 176 - 12,     0,  0, 0, 24      );
    sync_buffer[4] = encode(0,     0, 1, 176,          0,  0, 0, 24      );
    sync_buffer[5] = encode(0,     0, 1, 12,           0,  1, 1, 176 - 12);
    sync_buffer[6] = encode(30,    1, 0, 24,           0,  1, 1, 176     );
// add a buffer entry so we can use RING on the DMA channel
    sync_buffer[7] = encode(0,     1, 0, 24,           0,  1, 1, 176     );


#ifdef USE_CSYNC

    csync_buffer[0] = encode(479,   1, 0, 24,           1,  1, 1, 176     ); // 480 lines with irq triggering
    csync_buffer[1] = encode(9,     1, 0, 24,           0,  1, 1, 176     ); // 10 lines with no irq triggering
    csync_buffer[2] = encode(0,     1, 0, 24,           0,  1, 1, 12      ); // 1 short delay before start of vsync 
    csync_buffer[3] = encode(0,     0, 0, 176 - 12,     0,  0, 1, 24      ); // 1 start of vsync
    csync_buffer[4] = encode(0,     0, 0, 176,          0,  0, 1, 24      ); // 
    csync_buffer[5] = encode(0,     0, 0, 12,           0,  1, 1, 176 - 12);
    csync_buffer[6] = encode(30,    1, 0, 24,           0,  1, 1, 176     );
// add a buffer entry so we can use RING on the DMA channel
    csync_buffer[7] = encode(0,     1, 0, 24,           0,  1, 1, 176     );


// HSYNC   24  176         24  176        24 12   
//        ____""""" * 480 ____""""" * 10 ____""  


// VSYNC



#endif

// vga_1bit_data_array[0] = (((WHITE << 4) | BLACK) << 16) | (639);

// vga_1bit_data_array[0] = (((YELLOW << 4) | MAGENTA) << 16) | (639);
// vga_1bit_data_array[WORDS_PER_LINE] = (((BLUE << 4) | WHITE) << 16) | (639);


for (int y = 0; y < NO_OF_LINES; y++) {
    // vga_1bit_data_array[y * WORDS_PER_LINE] = (((BLUE << 4) | (y & 0x0f)) << 16) | (639);
    // char fore_colour;
    // char back_colour;
      
    // if (((y >= 55) && (y < 65)) || (y >= 469)) {
    //     fore_colour = WHITE;
    //     back_colour = DARK_BLUE;
    // } else {
    //     fore_colour = WHITE;
    //     back_colour = BLACK;
    // }

    // vga_1bit_data_array[y * WORDS_PER_LINE] = (((fore_colour << 4) | (back_colour)) << 16) | (639);
    // set_line_colors(y, back_colour, fore_colour, WHITE, LIGHT_BLUE);
    set_line_colors(y, BLACK, WHITE, WHITE, LIGHT_BLUE);
}


// vga_1bit_data_array[1] = 0b10101010110011001111000011111111;
// vga_1bit_data_array[1] = 0xffffffff;

// colour palette

// pixels
for (int y = 0; y < NO_OF_LINES; y++) {
    for (int x = 1; x < WORDS_PER_LINE; x ++) {
    // vga_1bit_data_array[i] = 0xf0f0f0f0;
        // vga_1bit_data_array[(y * WORDS_PER_LINE) + x] = 0b10101010110011001111000011111111;
        vga_1bit_data_array[(y * WORDS_PER_LINE) + x] = 0;
    }
}


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

    // pio_sm_put_blocking(pio_2, hsync2_sm, H_ACTIVE);  // don't think we need this    
    // pio_sm_put_blocking(pio_2, vsync2_sm, V_ACTIVE);
    // pio_sm_put_blocking(pio_2, rgb2_sm, RGB_ACTIVE); // will need this. no, they've been moved in .pio


    // pio_sm_exec(pio_2, vsync2_sm, pio_encode_pull(false, true)); // (IfE = 0, Blk = 1)

    // pio_sm_exec(pio_2, rgb2_sm, pio_encode_pull(false, true)); // will need this.  no, they've been moved in .pio
    // pio_sm_exec(pio_2, rgb2_sm, pio_encode_out(pio_y, 32)); // // will need this. no, they've been moved in .pio

    // Start the two pio machine IN SYNC
    // Note that the RGB state machine is running at full speed,
    // so synchronization doesn't matter for that one. But, we'll
    // start them all simultaneously anyway.


#ifndef USE_CSYNC
    pio_enable_sm_mask_in_sync(pio_2, ((1u << hsync5_sm) | (1u << rgb5_sm)));
#else

#if USE_HSYNC_AND_VSYNC
    pio_enable_sm_mask_in_sync(pio_2, ((1u << hsync5_sm) | (1u << rgb5_sm) | (1u << csync_sm)));
#else
    pio_enable_sm_mask_in_sync(pio_2, ((1u << rgb5_sm) | (1u << csync_sm)));
#endif


#endif

    // Start DMA channel 0. Once started, the contents of the pixel color array
    // will be continously DMA's to the PIO machines that are driving the screen.
    // To change the contents of the screen, we need only change the contents
    // of that array.
    dma_start_channel_mask((1u << rgb_test_chan_0)) ;


#ifdef USE_RING_BUF

    // The DVI uses irq_set_exclusive_handler(), so stops this working.
    // By disabling it here will mean that eventually the VGA will stop
    // woking and require a reset.

#if PICO_RP2040 == 1

    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(sync_test_chan_0, true);

    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler); // just setting this causes problems

    irq_set_enabled(DMA_IRQ_0, true);

#endif


#endif

    // Manually call the handler once, to trigger the first transfer
    // dma_handler();

#ifndef USE_CSYNC

    dma_start_channel_mask((1u << sync_test_chan_0));
#else
    dma_start_channel_mask((1u << sync_test_chan_0) | (1u << csync_dma_chan0));
#endif


    // Manually call the handler once, to trigger the first transfer
    // dma_handler();

#ifdef USE_RING_BUF

//    dma_hw->ch[sync_test_chan_0].transfer_count = SYNC_BUFFER_COUNT * 60 * 30; this doesn't work

#endif

}


bool get_1bit_color(short y, char color) {
    return color != BLACK;
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

    if ((x >= _width) || (x < 0) || (y < 0))
        return;

    #define LINES_TOP 0

    if ((y < LINES_TOP) || (y >= LINES_TOP + NO_OF_LINES))
        return;

    // calculate the word index

    // y is between LINES_TOP and LINES_TOP + NO_OF_LINES - 1
    // convert it to 0 and NO_OF_LINES - 1
    
    int wi = (y - LINES_TOP) * WORDS_PER_LINE;

    // add the x offset plus the number of words before the data (currently one)

    #ifdef USE_4_BIT_RGB5_PIO
    
    wi = wi + (x / 16) + 1;

    #else

    wi = wi + (x / 32) + 1;

    #endif

    // we're now pointing at the correct word
    // calculate the bit index 0 - 31

    #ifdef USE_4_BIT_RGB5_PIO

    // calculate the bits index 0, 2, 4 - 26, 28, 30

    int bi = (x & 0x0f) << 1;
    
    #else

    // calculate the bit index 0 - 31

    int bi = x & 0x1f;

    #endif

    bool on = get_1bit_color(y, color);

    #ifdef USE_4_BIT_RGB5_PIO
    vga_1bit_data_array[wi] &= ~(3u << bi);
    if (on) {
        uint8_t col_index = 1;
        if (color == LIGHT_BLUE) {
            col_index = 3;
        }
        vga_1bit_data_array[wi] |= (col_index << bi);
    // } else {
    //     vga_1bit_data_array[wi] = col & ~(3u << bi);
    }

    #else

    if (on) {
        vga_1bit_data_array[wi] |= (1u << bi);
    } else {
        vga_1bit_data_array[wi] &= ~(1u << bi);
    }

    #endif

}

void drawVLine(short x, short y, short h, char color) {
    for (short i=y; i<(y+h); i++) {
        drawPixel(x, i, color) ;
    }
}

void drawHLine(short x, short y, short w, char color) {

    if ((x < _width) && (y >= 0)) {

        if (x < 0) {
            w += x; // decrease w
            x = 0;
        }

        if (x + w > _width) {
            w = _width - x;
        }

        if (y < NO_OF_LINES) {

            bool on = get_1bit_color(y, color);

            int bufIndex = (y * WORDS_PER_LINE) + 1 + (x / 32);

            int pixels_to_preserve = x % 32;

                // eg if x = 325, pixels_to_preserve = 5
                // we need to preserve the first 5 pixels as they are and modify the remaining 32 - 5 = 27 pixels
                // we need to point our

                // however if the width, say 2, is less than the pixels to modify we need preserve the remaining pixels
                // 27 - 2 = 25 of them

            
            if (pixels_to_preserve) {

                x = (x + 32) & ~0x1f; // increment x to the next word boundary

                uint32_t mask = UINT32_MAX << (pixels_to_preserve);

                int pixels_to_modify = 32 - pixels_to_preserve;

                if (pixels_to_modify > w) {
                    uint32_t leave_mask = UINT32_MAX << (pixels_to_preserve + w);
                    mask ^= leave_mask;
                    w = 0;
                } else {
                    w -= pixels_to_modify;
                }

                if (on) {
                    vga_1bit_data_array[bufIndex] |= mask;
                } else {
                    vga_1bit_data_array[bufIndex] &= ~mask;
                }

                bufIndex++;
            }

            if (w > 0) {
                int words_remaining = w / 32;
                if (words_remaining > 0) {
                    memset(&vga_1bit_data_array[bufIndex], on ? 0xff : 0, words_remaining * sizeof(uint32_t));
                    bufIndex += words_remaining;
                }

            }
            int bits_remaining = w % 32;

            if (bits_remaining > 0) {
                uint32_t mask = UINT32_MAX >> (32 - bits_remaining);
                if (on) {
                    vga_1bit_data_array[bufIndex] |= mask;
                } else {
                    vga_1bit_data_array[bufIndex] &= ~mask;
                }
            }
        }

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

    // if (y >= _height) {
    //     return;
    // }


      if (x < _width) {

          // The commented out stuff below is checked during drawHLine
          if (x < 0) {
              w += x; // decrease w
              x = 0;
          }

          if (x + w > _width) {
              w = _width - x;
          }

          if (y < NO_OF_LINES) {

              if (y < 0) {
                  h += y; // decrease h
                  y = 0;
              }

              if (y + h > NO_OF_LINES) {
                  h = NO_OF_LINES - y;
              }

              if ((x == 0) && (w == _width)) {

                  uint8_t bits = 0;
                  if (get_1bit_color(y, color)) {
                      bits = 0xff;
                  }

                  for (int i = 0; i < h; i++) {
                      memset(&vga_1bit_data_array[((y + i) * WORDS_PER_LINE) + 1], bits, (WORDS_PER_LINE - 1) * sizeof(uint32_t));
                  }
                  // drawHLine(x, y + i, w, color);
              } else {
                  for (int i = 0; i < h; i++) {
                      drawHLine(x, y + i, w, color);

                  }
              }
          }

            //  memset(&vga_1bit_data_array[(479 * WORDS_PER_LINE) + 1], 0xff, (WORDS_PER_LINE - 1) * sizeof(uint32_t));


// #if (VGA_USE_PIO_PROG == 5) || (VGA_TEST_PIO_PROG == 5)
//         memset(&vga_1bit_data_array[(y * WORDS_PER_LINE) + 1], 0, h * (_width / 2));

//             for (int x = 1; x < WORDS_PER_LINE; x ++) {
//     // vga_1bit_data_array[i] = 0xf0f0f0f0;
//         // vga_1bit_data_array[(y * WORDS_PER_LINE) + x] = 0b10101010110011001111000011111111;
//         vga_1bit_data_array[(y * WORDS_PER_LINE) + x] = 0;
//     }

// #endif

      }

}

// Draw a character
void drawChar(short x, short y, unsigned char c, char color, char bg, unsigned char size) {
    char i, j;
  // if((x >= _width)            || // Clip right
  //    (y >= _height)           || // Clip bottom
  //    ((x + 6 * size - 1) < 0) || // Clip left
  //    ((y + 8 * size - 1) < 0))   // Clip top
  //   return;

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

uint16_t text_padding;

void set_text_padding(uint16_t padding) {
    text_padding = padding;
}


void tft_write(unsigned char c){
  if (c == '\n') {
    cursor_y += textsize * (8 + text_padding);
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
