# How to create a build directory

The target for this example is an RP2350 on a Pico 2

`mkdir build-pico2`

`cd build-pico2`

Specify where the pico-sdk directory can be found.

`export PICO_SDK_PATH=~/pico2/pico/pico-sdk`

Specify where the top level CMakeLists.txt file can be found. In this case it's the parent directory (`..`). Also, specify the board (`pico2`). For a list of possible boards search the `pico-sdk/src/boards/include/boards` directory.

`cmake .. -DPICO_BOARD=pico2`

`make -j4`

This should generate, amongst other things, a `.elf` file.


To program an rp2350 with a `filename.elf` file using openocd and a RPi Debug Probe:

`~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program filename.elf verify reset exit"`

Note that the above does not work when using dvi on hstx. See `make-and-flash.sh` for how to
fudge it to get it to work. Update: it's now proving troublesome even with this fix. It 
sometimes needs 2 or 3 attempts for `make-and-flash.sh` to succeed.

Update. When another Pico is generating VGA logic level signals and this Pico 2 is capturing
and mirroring them to the htsx dvi port the programming sometimes fails. If the Pico
generating the VGA signals is not powered the programming seems to succeed every time.


To make a script file (called `filename.sh`) executable, enter:

`chmod +x filename.sh`


# Pinout

       VGA In Dark Blue  GP0  1                  40 VBUS
      VGA In Light Blue  GP1  2                  39 VSYS
                         GND  3                  38 GND
      VGA In Dark Green  GP2  4                  37 3V3 EN
     VGA In Light Green  GP3  5                  36 3V3 OUT
        VGA In Dark Red  GP4  6                  35 ADC VREF
       VGA In Light Red  GP5  7                  34 GP28 VGA In CSYNC - not yet!
                         GND  8                  33 ADC GND
      VGA Out Dark Blue  GP6  9                  32 GP27  VGA In VSYNC
     VGA Out Light Blue  GP7 10     PICO 2       31 GP26  VGA In HSYNC
     VGA Out Dark Green  GP8 11                  30 RUN   CAPTAIN RESETTI
    VGA Out Light Green  GP9 12                  29 GP22  VGA Out CSYNC
                         GND 13                  28 GND
      VGA Out Dark Red  GP10 14                  27 GP21  Serial RX
     VGA Out Light Red  GP11 15                  26 GP20  Serial TX
               DVI D0+  GP12 16                  25 GP19  DVI D2+
               DVI D0-  GP13 17                  24 GP18  DVI D2-
                        GND  18                  23 GND
               DVI CK+  GP14 19                  22 GP17  DVI D1+
               DVI CK-  GP15 20                  21 GP16  DVI D1-

VGA Out GPIO                           VGA 15-way Socket


VGA Out Dark Blue    ---  1K  ---      3 Blue
VGA Out Light Blue   --- 470R ---      3 Blue

VGA Out Dark Green   ---  1K  ---      2 Green
VGA Out Light Green  --- 470R ---      2 Green

VGA Out Dark Red     ---  1K  ---      1 Red 
VGA Out Light Red    --- 470R ---      1 Red


VGA Out CSYNC        ---  47R ---      13 HSYNC




Strange thing I experienced just now

I've been developing this project mainly on a Raspberry Pi 5. Every so often I need to be away and don't really
want to take my Pi, so I copy the project folder with the git history etc. onto a Chromebook, on which
I'm using Linux (Debian) and VS Code. This time when I copied the folder back to the Pi I found that
the Git history wasn't working, either in VS Code or on the command line. I tried a number of times, thinking
I was doing something wrong and each time the same thing would happen. Finally, I tried zipping the folder on the
Chromebook and unzipping it on the Pi, and that worked. Chrome OS was updated since while I was away (using the
Chromebook), so maybe that did something.
