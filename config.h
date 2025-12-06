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


#if (defined RASPBERRYPI_PICO2)

    #pragma message "Building Palavo for RASPBERRYPI_PICO2"
    #define BOARD_TYPE 1

#elif (defined PIMORONI_PICO_LIPO2XL_W_RP2350)

    #pragma message "Building Palavo for PIMORONI_PICO_LIPO2XL_W_RP2350"
    #define BOARD_TYPE 2

#elif (defined SOLDERPARTY_RP2350_STAMP_XL)

    #pragma message "Building Palavo for SOLDERPARTY_RP2350_STAMP_XL"
    #define BOARD_TYPE 3

#elif (defined RASPBERRYPI_PICO)

    #pragma message "Building Palavo for RASPBERRYPI_PICO"
    #define BOARD_TYPE 4

#elif (defined PIMORONI_PICO_PLUS2_RP2350)

    #pragma message "Building Palavo for PIMORONI_PICO_PLUS2_RP2350"
    #define BOARD_TYPE 5

#else

    #pragma message "Building Palavo for an unknown board"
    #define BOARD_TYPE 0
    
#endif


#ifdef PALAVO_CONFIG

    #pragma message "PALAVO_CONFIG detected."
    // #pragma message STR(PALAVO_CONFIG)

#else

    #pragma message "PALAVO_CONFIG not detected."

    // PALAVO_CONFIG has not already been defined by CMAKE, so define one here.
    // This is mainly intended for getting colour syntax working whilst developing.

    #if (BOARD_TYPE == 1)

        // #pragma message "Building Palavo for RASPBERRYPI_PICO2"
        #define PALAVO_CONFIG 0
        // #define PALAVO_CONFIG ((1 << PC_BIT_USE_DVI) | (1 << PC_BIT_USE_VGA_IN_TO_DVI))

    #elif (BOARD_TYPE == 2)

        // #pragma message "Building Palavo for PIMORONI_PICO_LIPO2XL_W_RP2350"
        #define PALAVO_CONFIG (1 << PC_BIT_USE_GPIO_31_47)

    #elif (BOARD_TYPE == 3)

        // #pragma message "Building Palavo for SOLDERPARTY_RP2350_STAMP_XL"
        #define PALAVO_CONFIG (1 << PC_BIT_USE_DVI)
        // #define PALAVO_CONFIG (1 << PC_BIT_USE_GPIO_31_47)

    #elif (BOARD_TYPE == 4)

        // #pragma message "Building Palavo for RASPBERRYPI_PICO"
        #define PALAVO_CONFIG 0
        
    #else

        // This is a default palavo config for colour syntax whilst developing only.
        // #define PALAVO_CONFIG 0
        // #define PALAVO_CONFIG (1 << PC_BIT_USE_DVI)
        // #define PALAVO_CONFIG ((1 << PC_BIT_USE_DVI) | (1 << PC_BIT_USE_GPIO_31_47))
        #error "Please specify a supported board, or define a PALAVO_CONFIG (See README.md)."
        // #define PALAVO_CONFIG ((1 << PC_BIT_USE_DVI) | (1 << PC_BIT_USE_VGA_IN_TO_DVI))

        // #define PALAVO_CONFIG ((1 << PC_BIT_USE_DVI) | (1 << PC_BIT_USE_GPIO_31_47))

// config13
        // #define PALAVO_CONFIG ((1 << PC_BIT_USE_DVI) | (0 << PC_BIT_USE_GPIO_31_47) | (1 << PC_BIT_USE_VGA_IN_TO_DVI) | (1 << PC_BIT_USE_NEW_IO_MAPPING))
        // #define PICO_PIO_USE_GPIO_BASE 0

// config2
        // #define PALAVO_CONFIG (1 << PC_BIT_USE_GPIO_31_47)
        // #define PICO_PIO_USE_GPIO_BASE 1

// config0


// 33 = 32 + 1 = USE_USB & USE_DVI
        // #define PALAVO_CONFIG 33

// 49 = 32 + 16 + 1 = USE_USB & USE_DVI
        // #define PALAVO_CONFIG 49

// 1 = USE_DVI
        // #define PALAVO_CONFIG 1

// 17 = 16 + 1 = USE_CSYNC & USE_DVI
        // #define PALAVO_CONFIG 17

// 5 = 4 + 1 = USE_VGA_IN_TO_DVI & USE_DVI
        // #define PALAVO_CONFIG 5

// 3 = 2 + 1 = USE_GPIO_31_47 & USE_DVI
        // #define PALAVO_CONFIG 3

// 7 = 4 + 2 + 1 = USE_VGA_IN_TO_DVI USE_GPIO_31_47 & USE_DVI
        // #define PALAVO_CONFIG 7

// 15 = 8 + 4 + 2 + 1 = USE_VGA_IN_TO_DVI USE_GPIO_31_47 & USE_DVI
        // #define PALAVO_CONFIG 15

        #define PALAVO_CONFIG 34
        #define PICO_PIO_USE_GPIO_BASE 1

// #13 needs CSYNC to be set?

        #endif

#endif

#if (PALAVO_CONFIG & (1 << PC_BIT_USE_DVI))
    #define USE_DVI 1
#endif


#if (PALAVO_CONFIG & (1 << PC_BIT_USE_GPIO_31_47))
    #define USE_GPIO_31_47 1
#endif


#if (PALAVO_CONFIG & (1 << PC_BIT_USE_VGA_IN_TO_DVI))
    #define USE_VGA_IN_TO_DVI 1
#endif


#if (PALAVO_CONFIG & (1 << PC_BIT_USE_IR))
    #define USE_IR 1
#endif


#if (PALAVO_CONFIG & (1 << PC_BIT_USE_CSYNC))
    #define USE_CSYNC 1
#endif

#if (PALAVO_CONFIG & (1 << PC_BIT_USE_USB_STDIO))
    #define USE_USB_STDIO 1
#endif

#if USE_DVI
    // Using DVI - define what mode to show on startup.
    #pragma message "Using DVI"

    #define USE_VGA_CAPTURE 1

    #if USE_VGA_IN_TO_DVI

        #pragma message "Display VGA In on DVI on startup"

    #else

        #pragma message "Mirror VGA Out to DVI on startup"

    #endif

#else
    #pragma message "Not using DVI"
#endif


#if USE_GPIO_31_47


    #if (!PICO_PIO_USE_GPIO_BASE)
        #error "Can't use more than 32 pins on this microcontroller"
    #else
        #pragma message "Using GPIO 31-47 for user interface"
    #endif

    #define MUST_USE_CSYNC 1

#else
    // !USE_GPIO_31_47

    #pragma message "Using GPIO 0-31"

    #if USE_DVI && USE_VGA_IN_TO_DVI

        #define MUST_USE_CSYNC 1

    #endif

#endif


#if USE_IR

    #pragma message "Using Infra-red Remote Control"

#endif


#if USE_CSYNC

    #pragma message "Using CSYNC instead of VSYNC and HSYNC"

#else

    #if MUST_USE_CSYNC

    // #error "CSYNC must be used with this configuration"

    #pragma message "CSYNC must be set with this configuration"
    #define USE_CSYNC 1
    #pragma message "Added `#define USE_CSYNC 1`"

    #endif

#endif


#if USE_USB_STDIO

    #pragma message "Using USB STDIO instead of UART STDIO"

#endif


#endif