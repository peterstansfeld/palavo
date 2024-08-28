cmake_minimum_required(VERSION 3.13)

# add_compile_definitions(SYS_CLK_KHZ=125000u)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(graphics_test)

# must match with pio filename and executable name from above
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/hsync2.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/vsync2.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/rgb2.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/hsync3.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/rgb3.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/hsync4.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/rgb4.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/hsync5.pio)
pico_generate_pio_header(graphics_test ${CMAKE_CURRENT_LIST_DIR}/rgb5.pio)

# must match with executable name and source file names
target_sources(graphics_test PRIVATE vga_graphics_demo.c vga16_graphics.c)

# must match with executable name
target_link_libraries(graphics_test PRIVATE pico_stdlib hardware_pio hardware_dma hardware_adc hardware_irq hardware_uart hardware_clocks)

# must match with executable name
pico_add_extra_outputs(graphics_test)
