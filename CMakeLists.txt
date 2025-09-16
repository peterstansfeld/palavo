cmake_minimum_required(VERSION 3.13)

# add_compile_definitions(SYS_CLK_KHZ=125000u)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(palavo)

# must match with pio filename and executable name from above
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync2.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/vsync2.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb2.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync3.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb3.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync4.pio)
# pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb4.pio)
pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/hsync5.pio)
pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/rgb5.pio)
pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/nec_ir_rx.pio)
if (PICO_RP2350)
    # vga_capture.pio currently uses some of the new PIO instructions in the RP235x
    pico_generate_pio_header(palavo ${CMAKE_CURRENT_LIST_DIR}/vga_capture.pio)
endif()

# must match with executable name and source file names
target_sources(palavo PRIVATE vga_graphics_demo.c vga2_graphics.c)
#if (PICO_BOARD STREQUAL "pico2")
if (PICO_RP2350)
    target_sources(palavo PRIVATE dvi64_graphics.c)
    # add_compile_definitions(USE_DVI=0)
endif()

# without the following we can't pass the `PALAVO_CONFIG` definition 
# `-DPALAVO_CONFIG=x` as an argument when running `cmake`.

if (DEFINED PALAVO_CONFIG)
    # add_compile_definitions(PALAVO_CONFIG
    add_compile_definitions(PALAVO_CONFIG=${PALAVO_CONFIG})
endif()

# must match with executable name
target_link_libraries(palavo PRIVATE pico_stdlib hardware_pio hardware_dma hardware_adc hardware_irq hardware_uart hardware_clocks)

# must match with executable name
pico_add_extra_outputs(palavo)
