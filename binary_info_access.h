#ifndef _BINARY_INFO_ACCESS_H
#define _BINARY_INFO_ACCESS_H

#include "pico/stdlib.h"

extern uint32_t address_mapping_table;

void print_binary_info(uint32_t print_types, uint16_t tag, uint32_t id);

#endif
