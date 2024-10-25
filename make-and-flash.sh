#! /bin/bash

make -j4

if [ $? -ne 0 ]; then
    echo "An error occured compiling your project, Pete."
else
# For some slightly concerning reason when running dvi on hstx the target needs to be reset before issuing the 
# second (programming) command. The target doesn't require it when the hasn't been running dvi.
~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "init; reset; exit"

# The second (programming command).
~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program palavo.elf verify reset exit"

# Note that when another Pico is generating VGA logic level signals and this Pico 2 is capturing
# and mirroring them to the htsx dvi port the programming sometimes fails. Turn off the Pico and
# programming seems to succeed every time.

fi