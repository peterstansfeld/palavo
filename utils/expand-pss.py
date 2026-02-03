# Expands a palavo VGA framebuffer as a 24-bit colour 
# 640 x 480 file, which can be converted to a much smaller `.png` file 
# using the following `imagemagick` command:

# magick -size 640x480 -depth 8 rgb:SOURCE DEST

# (where SOURCE is the DEST from this program)

import os
import sys


def show_help():
    print("Usage: python3 expand-pss.py SOURCE DEST")


if len(sys.argv) != 3:
    print("Please specify a source and a destination file.")
    show_help()
    sys.exit(1)

src_filename = sys.argv[1]
dest_filename = sys.argv[2]

# Check if the file exists.
if os.path.isfile(src_filename) == False:
    print("The source file " + src_filename + " does not exist.")
    show_help()
    sys.exit(2)

# Read the input file.
f = open(src_filename, "rb")
src_contents = f.read()
f.close()

# Expand
print("Expanding `" + src_filename + "` and saving as `" + dest_filename + "` ...")

length_vga = int(4 + (640 / 8))
length_vga = length_vga * 480

bytes_per_line_dvi = int((640 * 4) / 5)
length_dvi = bytes_per_line_dvi * 480

src_len = len(src_contents)

if (src_len!= length_vga) and (src_len != (length_dvi)):
    print("The file " + src_filename + " needs to be either " + str(length_vga) + " bytes in length for VGA or " + str(length_dvi) + " for DVI.")
    sys.exit(3)

length = length_vga

if src_len == length_dvi:
    length = length_dvi

# Global variables.

# bytearray to hold all the 8-bit RGB values for the output file.
dest_contents = bytearray(640 * 480 * 3)


def expand_2_bit_color(two_bits):
    r = 0
    if two_bits & 0x02 != 0:
        # r = 0xc0
        r = 0x80
        # r = 170
    if two_bits & 0x01 != 0:
        # r = r | 0x3f
        r = r | 0x40
        # r = r + 85
    return r


def expand_vga_framebuffer():
    fore_col = 0
    back_col = 0

    fore_red = 0
    fore_green = 0
    fore_blue= 0

    back_red = 0
    back_green = 0
    back_blue= 0

    # dest_contents index
    dc_i = 0

    # src_contents index
    src_i = 0

    while (src_i < length):

        # The first four bytes are a little-endian uint32_t consisting of
        # foreground and background colours and a constant (639). 
        u16 = (src_contents[src_i + 3] << 8 ) | src_contents[src_i + 2]

        back_col = (u16 & 0x3f)
        fore_col = ((u16 >> 6) & 0x3f)

        # Expand the 2-bit colours into 8-bit ones.
        fore_red = expand_2_bit_color(fore_col >> 4)
        fore_green = expand_2_bit_color(fore_col >> 2)
        fore_blue = expand_2_bit_color(fore_col) 

        back_red = expand_2_bit_color(back_col >> 4)
        back_green = expand_2_bit_color(back_col >> 2)
        back_blue = expand_2_bit_color(back_col) 

        # Skip the line colours etc.
        src_i += 4

        for j in range(0, 80):

            # expand each bit in the byte rgb values
            byte = src_contents[src_i]
            for j in range(8):
                if (byte & (1 << j)):
                    dest_contents[dc_i] = fore_red
                    dest_contents[dc_i + 1] = fore_green
                    dest_contents[dc_i + 2] = fore_blue
                else:
                    dest_contents[dc_i] = back_red
                    dest_contents[dc_i + 1] = back_green
                    dest_contents[dc_i + 2] = back_blue

                # next 24-bit RGB value
                dc_i += 3

            # next byte
            src_i += 1


def expand_dvi_framebuffer():

    fore_col = 0
    back_col = 0

    fore_red = 0
    fore_green = 0
    fore_blue= 0

    back_red = 0
    back_green = 0
    back_blue= 0

    # dest_contents index
    dc_i = 0

    # src_contents index
    src_i = 0

    while (src_i < length):
        # read four bytes as a little-endian 32-bit word and trim the two unused bits
        u32 = ((src_contents[src_i + 3] << 24 ) | (src_contents[src_i + 2] << 16) | (src_contents[src_i + 1] << 8) | src_contents[src_i + 0]) >> 2
        
        for j in range(0, 5):

            fore_blue = expand_2_bit_color(u32 & 0b11)
            u32 >>= 2
            fore_green = expand_2_bit_color(u32 & 0b11)
            u32 >>= 2
            fore_red = expand_2_bit_color(u32 & 0b11)
            u32 >>= 2

            dest_contents[dc_i] = fore_red
            dest_contents[dc_i + 1] = fore_green
            dest_contents[dc_i + 2] = fore_blue

            # next 24-bit RGB value
            dc_i += 3

        # next u32 word
        src_i += 4


if src_len == length_vga:
    expand_vga_framebuffer()
else:
    expand_dvi_framebuffer()

# Write the output file
f = open(dest_filename, "wb")
f.write(dest_contents)
f.close()

print("Done")
