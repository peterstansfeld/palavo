#! /bin/bash

# This script attempts to compile (`make`) the palavo firmware and,
# if succesful, flash it onto a Raspberry Pi RP2xxx MCU by using
# Raspberry Pi's `picotool` application.

make -j4

if [ $? -ne 0 ]; then
    echo "An error occured compiling the project."
else
    # Attempt to load palavo.uf2 into an MCU that's in boot load mode. If
    # no MCU is found it will attempt to force any MCU that has USB stdio
    # enabled. Once successfully loaded the MCU is returned to an
    # executing state.:
    # picotool load palavo.uf2 -f -x

    # If `picotool` detects multiple MCUs that have USB stdio enabled,
    # it will refuse to program any of them - and rightly so. In which
    # case the serial number of the target MCU can be specified.:
    
    # picotool load palavo.uf2 --ser  -f -x

    picotool load palavo.uf2 --ser E66044304368642B -f -x

    # To find the serial number of a device:
    # 1. Unplug the device.
    # 2. run `lsusb` to list all usb devices.
    # 3. Plug the device back in.
    # 4. run `lsusb` and note the new device's Bus and Device numbers.
    # 5. run `lsusb -s[[bus]]:[devnum] -v | grep iSerial
    # 6. The serial number is the long number.

    # Here's a current list of picos' serial numbers:

    # E66044304368642B - rpi pico (config 0)
    # E2CD6B985F314CA0 - rpi pico2 (config53 @150 MHz)
    
    # 1889E3547E130184 - pimoroni pico lipo 2xl w
    # F51508674660002F - rpi pico2 (config53 @125 MHz)

    # 471CF1562C3925CA - rpi pico (keybuart)
    
    # E0C9125B0D9B - rpi pico (hunter adams's vga graphics primitives demo)

    # E0C912D24340 - rpi pico (macintosh video out simulator) 
fi