#include "pico/binary_info.h"
#include <string.h>
#include "binary_info_access.h"

uint32_t address_mapping_table;


uint32_t get_flash_address(uint32_t address) {
    uint32_t r = 0;
    if (address >= 0x20000000) {
        // address is in SRAM so use the address_mapping_table to
        // map it to an address in flash
        uint32_t* amt_ptr = (uint32_t*)address_mapping_table;
        while (*amt_ptr) {
            uint32_t source_addr_start = *amt_ptr;
            uint32_t dest_addr_start = *(amt_ptr + 1);
            uint32_t dest_addr_end = *(amt_ptr + 2);
            if ((address >= dest_addr_start) && address < dest_addr_end) {
                r = source_addr_start + (address - dest_addr_start);
                break;
            }
            amt_ptr += 3;
        }
    } else {
        r = address;
    }
    return r;
}


uint32_t* find_binary_info_start_ptr() {
    uint32_t* flash_ptr = (uint32_t*)XIP_BASE + (0x100 / sizeof(uint32_t));
    for (int i = 0; i < (256 / sizeof(uint32_t)); i++) {
        if ((*flash_ptr == BINARY_INFO_MARKER_START) && (*(flash_ptr + 4) == BINARY_INFO_MARKER_END)) {
            // binary_info_start = *(flash_ptr + 1);
            // binary_info_end = *(flash_ptr + 2);
            address_mapping_table = *(flash_ptr + 3);
            return flash_ptr + 1;
        }
        flash_ptr += 1;
    }
    return 0;
}


void print_binary_info(uint32_t print_types, uint16_t tag, uint32_t id) {
    static uint32_t* binary_info_start_ptr;

    if (!binary_info_start_ptr) {
        binary_info_start_ptr = find_binary_info_start_ptr();
    }

    uint32_t* bis_ptr = (uint32_t*)*(binary_info_start_ptr);
    uint32_t* bie_ptr = (uint32_t*)*(binary_info_start_ptr + 1);

    int no_of_items = (((uint32_t*)bie_ptr - (uint32_t*)bis_ptr));
    for (int i =  0; i < no_of_items; i++) {
        uint32_t * bi_ptr = (uint32_t*)*bis_ptr;
        binary_info_t bi;
        memcpy(&bi, bi_ptr, sizeof(binary_info_t));

        if ((print_types & (1 << bi.type)) && ((bi.tag == tag) | (tag == 0))) {
            switch (bi.type){

                case BINARY_INFO_TYPE_ID_AND_INT:
                    binary_info_id_and_int_t bi_id_and_int;
                    memcpy(&bi_id_and_int, bi_ptr, sizeof(binary_info_id_and_int_t));
                    // uint32_t mask_lo = bi_p64.pin_mask & 0xffffffff;
                    stdio_printf("0x%x: %d\n", bi_id_and_int.id, bi_id_and_int.value);
                    break;

                case BINARY_INFO_TYPE_PINS64_WITH_NAME:
                    binary_info_pins64_with_name_t bi_p64;
                    memcpy(&bi_p64, bi_ptr, sizeof(binary_info_pins64_with_name_t));

                    for (int i = 0; i < 64; i++) {
                        if (bi_p64.pin_mask & (1 << i)) {
                            stdio_printf(" %d", i);
                            break;
                        }
                    }

                    stdio_printf(": %s\n", (char*)get_flash_address((uint32_t)bi_p64.label));
                    break;

                case BINARY_INFO_TYPE_ID_AND_STRING:
                    binary_info_id_and_string_t bi_id_and_str;
                    memcpy(&bi_id_and_str, bi_ptr, sizeof(binary_info_id_and_string_t));
                    if ((bi.tag == tag) && ((bi_id_and_str.id == id) | (id == 0))){
                        stdio_printf(" ");
                        switch (bi_id_and_str.id) {
                            case BINARY_INFO_ID_RP_PROGRAM_URL:
                                stdio_printf("web site: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_DESCRIPTION:
                                stdio_printf("description: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_VERSION_STRING:
                                stdio_printf("version: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_BUILD_ATTRIBUTE:
                                stdio_printf("build attributes: ");
                                break;
                            case BINARY_INFO_ID_RP_BOOT2_NAME:
                                stdio_printf("boot2_name: ");
                                break;
                            case BINARY_INFO_ID_RP_SDK_VERSION:
                                stdio_printf("sdk version: ");
                                break;
                            case BINARY_INFO_ID_RP_PICO_BOARD:
                                stdio_printf("pico_board: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_NAME: 
                                stdio_printf("name: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_BUILD_DATE_STRING:
                                stdio_printf("build date: ");
                                break;
                            case BINARY_INFO_ID_RP_PROGRAM_FEATURE:
                                // stdio_printf("feature: ");
                                stdio_printf(" ");
                                break;
                        }
                        stdio_printf("%s\n", (char*)get_flash_address((uint32_t)bi_id_and_str.value));
                    } else {
                        // other tag or id
                        // stdio_printf("Other tag 0x%x:", bi.tag);
                    }
                    break;

                case BINARY_INFO_TYPE_PTR_INT32_WITH_NAME:
                    binary_info_ptr_int32_with_name_t bi_ptr_int32_with_name;
                    memcpy(&bi_ptr_int32_with_name, bi_ptr, sizeof(binary_info_ptr_int32_with_name_t));
                    stdio_printf(" %s = %d\n", (char*)get_flash_address((uint32_t)bi_ptr_int32_with_name.label),
                        *(int32_t*)get_flash_address((uint32_t)bi_ptr_int32_with_name.value));
                    break;

                case BINARY_INFO_TYPE_PTR_STRING_WITH_NAME:
                    binary_info_ptr_string_with_name_t bi_ptr_string_with_name;
                    memcpy(&bi_ptr_string_with_name, bi_ptr, sizeof(binary_info_ptr_string_with_name_t));
                    stdio_printf(" %s = \"%s\"\n", (char*)get_flash_address((uint32_t)bi_ptr_string_with_name.label),
                        (char*)get_flash_address((uint32_t)bi_ptr_string_with_name.value));
                    break;

                case BINARY_INFO_TYPE_NAMED_GROUP:
                    binary_info_named_group_t bi_named_group;
                    memcpy(&bi_named_group, bi_ptr, sizeof(binary_info_named_group_t));
                    stdio_printf("%s\n", (char*)get_flash_address((uint32_t)bi_named_group.label));
                    break;

                default: 
                    stdio_printf("unknown type: %d\n", bi.type);
                    break;
            }
        }
        bis_ptr += 1;
    }
}
