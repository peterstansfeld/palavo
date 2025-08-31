#! /bin/bash

# To select a specific adpater (RPi Debug Probe) uncomment the appropriate line:

adapter_serial_no=E6614103E761312F
# adapter_serial_no=E663AC91D3139137
# adapter_serial_no=E6614103E7440D2F
# adapter_serial_no=E6614103E71C4A2F

target_adapter_cmnd="adapter serial ${adapter_serial_no}"

#To target any adpater (RPi Debug Probe) uncomment the following line:

# target_adapter_cmnd=

# echo $target_adapter_cmnd

make -j4

if [ $? -ne 0 ]; then
    echo "An error occured compiling the project."
else

    echo $target_adapter_cmnd

    # For some slightly concerning reason when running dvi on hstx the target needs to be reset before issuing the 
    # second (programming) command. The target doesn't require it when the hasn't been running dvi.
    # ~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "init; reset; exit"
    ~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "${target_adapter_cmnd}" -c "adapter speed 5000" -c "init; reset; exit"

    if [ $? -ne 0 ]; then
        echo "An error occured trying to reset the target device."
    else
        # The second (programming command).

        # ~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program palavo.elf verify reset exit"

        ~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "${target_adapter_cmnd}" -c "adapter speed 5000" -c "program palavo.elf verify reset exit"

        # Note that when another Pico is generating VGA logic level signals and this Pico 2 is capturing
        # and mirroring them to the htsx dvi port the programming sometimes fails. Turn off the Pico and
        # programming seems to succeed every time.
    fi
fi