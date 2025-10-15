#! /bin/bash

# This script uses `openocd` to make the palavo firmware and, if succesful,
# flash it onto a raspberry Pi RP2350 using a Raspberry Pi Debug Probe. 

# To target a specific adpater (Debug Probe) specify its serial number:

# adapter_serial_no=E6614103E761312F
# adapter_serial_no=E663AC91D3139137
# adapter_serial_no=E6614103E7440D2F
adapter_serial_no=E6614103E71C4A2F

target_adapter_cmnd="adapter serial ${adapter_serial_no}"

# To target any adpater (RPi Debug Probe) uncomment the following line:

# target_adapter_cmnd=

openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "${target_adapter_cmnd}" -c "adapter speed 5000" -c "init; reset; exit"
