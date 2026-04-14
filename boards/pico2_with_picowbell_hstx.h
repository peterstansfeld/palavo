/*
 * Copyright (c) 2024 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_PICO2_WITH_PICOWBELL_HSTX
#define _BOARDS_PICO2_WITH_PICOWBELL_HSTX

pico_board_cmake_set(PICO_PLATFORM, rp2350)

#include "boards/pico2.h"

// For board detection
#define PICO2_WITH_PICOWBELL_HSTX

#define VGA_IN_VSYNC_PIN 0
#define VGA_IN_HSYNC_CSYNC_PIN 1
#define VGA_IN_RGB_BASE_PIN 6

#if (PALAVO_CONFIG & (1 << 2))
// #if USE_VGA_IN_TO_DVI // VGA to DVI converter
// VGA Out uses CSYNC and a monochrome pin

#define VGA_OUT_HSYNC_CSYNC_PIN 20
#define VGA_OUT_RGB_BASE_PIN 21
#define VGA_OUT_RGB_PIN_COUNT 1

#else
// VGA Out uses CSYNC and 6 colour pins (RRGGBB)

#define VGA_OUT_HSYNC_CSYNC_PIN 1
#define VGA_OUT_RGB_BASE_PIN 6
#define VGA_OUT_RGB_PIN_COUNT 6

#endif

// Move the IR
#define IR_RX_PIN 22

// Move the UART
#define PICO_DEFAULT_UART_TX_PIN 26
#define PICO_DEFAULT_UART_RX_PIN 27

#endif
