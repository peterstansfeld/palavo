#! /bin/bash

# Usage: ./expand-convert-and-display.sh [file name]

# This script attempts to expand a downloaded Palavo framebuffer to a raw format that imagemagick recognises,
# and if so converts it to a PNG called [file name].png and then display it.

# N.B. No error checking is performed, and:
# * [file name].png will be overwritten if it already exists!
# * [file name].raw will be overwritten if it already exists, and then deleted!

file_name=$1

python3 expand-pss.py ${file_name} ${file_name}.raw \
&& magick -size 640x480 -depth 8 rgb:${file_name}.raw ${file_name}.png \
&& rm ${file_name}.raw \
&& display ${file_name}.png
