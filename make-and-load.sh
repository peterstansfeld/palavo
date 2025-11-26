#! /bin/bash

# This script attempts to make the palavo firmware and, if succesful, flash
# it onto a raspberry Pi RP2350 using a Raspberry Pi Debug Probe using
# Raspberry Pi's `picotool` application.  

make -j4

if [ $? -ne 0 ]; then
    echo "An error occured compiling the project."
else
    # Attempt to load palavo.uf2 into an MCU that's in boot load mode. If
    # no MCU is found it will attempt to force any MCU that has USB stdio
    # enabled. Once successfully loaded the MCU is returned to an
    # executing state.  
    picotool load palavo.uf2 -f -x
fi