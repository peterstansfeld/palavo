
# Compresses a 640 x 480 `.rgb` image into a 1-bit palavo VGA framebuffer

# To convert a `.png` image (with a depth of 8) into a `.rgb` image use 
# the following `imagemagick` command:

# magick SOURCE.png DEST.rgb

# (where DEST is the SOURCE for this program)

import os
import sys

def show_help():
    print("Usage: python3 compress_rgb.py SOURCE DEST")


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
print("Compressing `" + src_filename + "` and saving as `" + dest_filename + "` ...")

length_vga = int(4 + (640 / 8))
length_vga = length_vga * 480

bytes_per_line_dvi = int((640 * 4) / 5)
length_dvi = bytes_per_line_dvi * 480

length_raw = 640 * 480 * 3

src_len = len(src_contents)

# if (src_len!= length_vga) and (src_len != (length_dvi)):
#     print("The file " + src_filename + " needs to be either " + str(length_vga) + " bytes in length for VGA or " + str(length_dvi) + " for DVI.")
if src_len!= length_raw:
    print("The file " + src_filename + " needs to be " + str(length_raw) + " bytes in length.")
    sys.exit(3)

length = length_vga

if src_len == length_dvi:
    length = length_dvi

# Global variables.

# bytearray to hold all the 8-bit RGB values for the output file.
# dest_contents = bytearray(640 * 480 * 3)

dest_contents = bytearray(length_vga)

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


def compress_raw_image():
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

    line = 0

    while (line < 480):
        # The first four bytes are a little-endian uint32_t consisting of
        # foreground and background colours and a constant (639). 

        back_col = 0    # black
        fore_col = 0x3f # white

        u32 = (((fore_col << 6) | back_col) << 16) | 639

        dest_contents[dc_i] = u32 & 0xff
        dest_contents[dc_i + 1] = (u32 >> 8) & 0xff
        dest_contents[dc_i + 2] = (u32 >> 16) & 0xff
        dest_contents[dc_i + 3] = (u32 >> 24) & 0xff
        dc_i += 4

        col = 0
        while (col < (640 / 8)):
            mono_byte = 0
            for j in range(0, 8):
                rgb24 = (src_contents[src_i] << 16) | (src_contents[src_i + 1] << 8) | src_contents[src_i + 2]
                src_i += 3

                if rgb24 != 0:
                    mono_byte = mono_byte | (1 << j)
            dest_contents[dc_i] = mono_byte
            dc_i += 1
            col += 1
        line += 1


compress_raw_image()

# Write the output file
f = open(dest_filename, "wb")
f.write(dest_contents)
f.close()

print("Done")
