cmake_minimum_required(VERSION 3.13)

# add `./boards` directory to the list of directories in which to search for the <PICO_BOARD>.h file.
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/boards")

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(palavo)

pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync_vsync.pio)
pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)
pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/nec_ir_rx.pio)
if (PICO_RP2350)
    # vga_capture.pio currently uses some of the new PIO instructions in the RP235x
    pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/vga_capture.pio)
endif()

# must match with executable name and source file names
target_sources(palavo PRIVATE palavo.c vga2_graphics.c)
#if (PICO_BOARD STREQUAL "pico2")
if (PICO_RP2350)
    target_sources(palavo PRIVATE dvi64_graphics.c)
    # add_compile_definitions(USE_DVI=0)
endif()

if (DEFINED PALAVO_CONFIG)
    # without the following line we can't pass the `PALAVO_CONFIG` definition 
    # `-DPALAVO_CONFIG=x` as an argument when running `make`.
    add_compile_definitions(PALAVO_CONFIG=${PALAVO_CONFIG})

    math(EXPR PC_BIT0 "${PALAVO_CONFIG} & (1<<0)")
    math(EXPR PC_BIT1 "${PALAVO_CONFIG} & (1<<1)")
    math(EXPR PC_BIT2 "${PALAVO_CONFIG} & (1<<2)")
    math(EXPR PC_BIT3 "${PALAVO_CONFIG} & (1<<3)")
    math(EXPR PC_BIT4 "${PALAVO_CONFIG} & (1<<4)")
    math(EXPR PC_BIT5 "${PALAVO_CONFIG} & (1<<5)")
    # if ((PALAVO_CONFIG EQUAL 29))
    if (PC_BIT0)
        # PC_BIT_USE_DVI
        message(STATUS "PALAVO_CONFIG.0 is 1")
    endif()
    if (PC_BIT1)
        # PC_BIT_USE_GPIO_31_47 
        message(STATUS "PALAVO_CONFIG.1 is 1")
    endif()
    if (PC_BIT2)
        # PC_BIT_USE_VGA_IN_TO_DVI
        message(STATUS "PALAVO_CONFIG.2 is 1")
    endif()
    if (PC_BIT3)
        # PC_BIT_USE_IR
        message(STATUS "PALAVO_CONFIG.3 is 1")
    endif()
    if (PC_BIT4)
        # PC_BIT_USE_CSYNC
        message(STATUS "PALAVO_CONFIG.4 is 1")
    endif()
    if (PC_BIT5)
        # PC_BIT_USE_UART_STDIO
        message(STATUS "PALAVO_CONFIG.5 is 1")
    endif()
endif()

# specify preprocessor definitions for the target palavo
target_compile_definitions(palavo PRIVATE
    PICO_DEFAULT_UART=1)

if (PC_BIT1)
    target_compile_definitions(palavo PRIVATE
        PICO_DEFAULT_UART_TX_PIN=38
        PICO_DEFAULT_UART_RX_PIN=39)
else()
    target_compile_definitions(palavo PRIVATE
        PICO_DEFAULT_UART_TX_PIN=8
        PICO_DEFAULT_UART_RX_PIN=9)
endif()

if (DEFINED VGA_TIMEOUT)
    # without the following line we can't pass the `VGA_TIMEOUT` definition 
    # `-DVGA_TIMEOUT=x` as an argument when running `make`.
    add_compile_definitions(VGA_TIMEOUT=${VGA_TIMEOUT})
endif()

if (DEFINED SYS_CLK_HZ)
    if ((SYS_CLK_HZ EQUAL 125000000))
        message(STATUS "SYS_CLK set to 125 MHz")
        add_compile_definitions(SYS_CLK_HZ=${SYS_CLK_HZ})
    elseif ((SYS_CLK_HZ EQUAL 150000000))
        message(STATUS "SYS_CLK set to 150 MHz")
        add_compile_definitions(SYS_CLK_HZ=${SYS_CLK_HZ})
    else()
        message(STATUS "SYS_CLK left at default")
    endif()
endif()

# Set the maximum number of milliseconds to wait during USB stdio 
# initialization. Without this a number of transmissions are lost
# because the host (PC) isn't ready to receive. 
target_compile_definitions(palavo PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=5000)

# must match with executable name
target_link_libraries(palavo PRIVATE pico_stdlib pico_stdio_uart hardware_pio hardware_dma hardware_irq hardware_clocks)

if (PICO_RP2350)
    # If we're using the RP235x we can use its HSTX peripheral to output DVI.
    # Seems we need to run it on the second core, otherwise it can fall over.
    target_link_libraries(palavo PRIVATE pico_multicore)
endif()

# must match with executable name
pico_add_extra_outputs(palavo)

# Select stdio peripheral(s)
message(STATUS "Use USB stdio")
pico_enable_stdio_usb(palavo 1)
