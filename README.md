# PALAVO

Palavo is a, and stands for: PIO-Assisted Logic Analyser with VGA Output.

*Assisted* doesn't really do PIO justice, as without PIO (Programmable Input 
Output on Raspberry Pi's RP2xxx microcontrollers) Palavo would not be 
possible.

Palavo is a keyboard*, or serial terminal, and infra-red remote
controlled logic analyser, which has a 6-bit colour, 640 x 480 VGA output
to display the captured input channels.

Palavo was inspired by Raspberry Pi's [Logic Analyser example in the SDK.](https://github.com/raspberrypi/pico-examples/tree/master/pio/logic_analyser)


*with a Raspberry Pi Pico (2) as a keyboard to serial terminal adapter.

## How to build Palavo

If one doesn't already exist, create a `build` directory in the Palavo directory.

`mkdir build`

Enter the `build` directory.

`cd build`

Create a suitably named directory (the target for this example is a Raspberry Pi Pico 2).

`mkdir pico2`

`cd pico2`


Specify where the pico-sdk directory can be found.

`export PICO_SDK_PATH=~/pico2.2/pico/pico-sdk`

Specify where the top level CMakeLists.txt file can be found - in this case
it's the grandparent directory `../../`, and specify the board `pico2`.

`cmake ../../ -DPICO_BOARD=pico2`

`cmake ../../ -DPICO_BOARD=pimoroni_pico_lipo2xl_w_rp2350`

`make -j4`

This should generate, amongst other files, a `palavo.uf2` file and a `palavo.elf` file.

To program the rp2350, put it into boot mode and copy `palavo.uf2` onto the drive that appears.

To program an rp2350 with a `palavo.elf` file using Openocd and a RPi Debug Probe:

`~/.pico-sdk/openocd/0.12.0+dev/openocd -s ~/.pico-sdk/openocd/0.12.0+dev/scripts -f ~/.vscode/extensions/marus25.cortex-debug-1.12.1/support/openocd-helpers.tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program filename.elf verify reset exit"`

Instead of the above, copy the script file `make-and-flash.sh` from the palavo directory:

`cp ../../make-and-flash.sh .`

Modify the `adapter_serial_no` variable to that of your Debug Probe. If you don't 
want to specify a serial number, blank the `target_adapter_cmnd` variable.

Then, attempt to make and flash the RP235x with:

`./make-and-flash.sh`

## Previously on this subject

Note that the above does not work when using dvi on hstx. See `make-and-flash.sh` for how to
fudge it to get it to work. Update: it's now proving troublesome even with this fix. It 
sometimes needs 2 or 3 attempts for `make-and-flash.sh` to succeed.

Update. When another Pico is generating VGA logic level signals and this Pico 2 is capturing
and mirroring them to the htsx dvi port the programming sometimes fails. If the Pico
generating the VGA signals is not powered the programming seems to succeed every time.

Update. What I said above no longer seems to be true. When the Pico generating the VGA is
not powered on but still connected, it takes two or more attempts to program the Pico 2.
When the Pico is connected, powered on, and held in reset (!RUN) the programming Pico 2 is
successful on the first attempt... Maybe the Pico is being parasitically powered through
its GPIO, or something like that?  

To make a script file (called `filename.sh`) executable, enter:

`chmod +x filename.sh`

## Pinout

       VGA In Dark Blue  GP0  1                  40 VBUS
      VGA In Light Blue  GP1  2                  39 VSYS
                         GND  3                  38 GND
      VGA In Dark Green  GP2  4                  37 3V3 EN
     VGA In Light Green  GP3  5                  36 3V3 OUT
        VGA In Dark Red  GP4  6                  35 ADC VREF
       VGA In Light Red  GP5  7                  34 GP28 IR RX
                         GND  8                  33 ADC GND
      VGA Out Dark Blue  GP6  9                  32 GP27  VGA In VSYNC
     VGA Out Light Blue  GP7 10     PICO 2       31 GP26  VGA In HSYNC or CSYNC
     VGA Out Dark Green  GP8 11                  30 RUN  CAPTAIN RESETTI
    VGA Out Light Green  GP9 12                  29 GP22  VGA Out CSYNC
                         GND 13                  28 GND
      VGA Out Dark Red  GP10 14                  27 GP21  Serial RX
     VGA Out Light Red  GP11 15                  26 GP20  Serial TX
               DVI D0+  GP12 16                  25 GP19  DVI D2+
               DVI D0-  GP13 17                  24 GP18  DVI D2-
                        GND  18                  23 GND
               DVI CK+  GP14 19                  22 GP17  DVI D1+
               DVI CK-  GP15 20                  21 GP16  DVI D1-


## Pinout for Pimoroni Pico LiPo 2 W XL

   VGA In HSYNC or CSYNC  GP0  1                  60 VBUS
            VGA In VSYNC  GP1  2                  59 VSYS
                          GND  3                  58 GND
        VGA In Dark Blue  GP2  4                  57 3V3 EN
       VGA In Light Blue  GP3  5                  56 3V3 OUT
       VGA In Dark Green  GP4  6                  55 ADC VREF
      VGA In Light Green  GP5  7                  54 GP28
                          GND  8                  53 ADC GND
        VGA In Dark Blue  GP6  9                  52 GP27
       VGA In Light Blue  GP7 10 PICO LIPO 2 W XL 51 GP26
                          GP8 11                  30 RUN  CAPTAIN RESETTI (maybe needed not with PWR switch?)
                          GP9 12                  49 GP22
                          GND 13                  48 GND
                         GP10 14                  47 GP21
                         GP11 15                  46 GP20  
                DVI D0+  GP12 16                  45 GP19  DVI D2+
                DVI D0-  GP13 17                  44 GP18  DVI D2-
                         GND  18                  43 GND
                DVI CK+  GP14 19                  42 GP17  DVI D1+
                DVI CK-  GP15 20 ________________ 41 GP16  DVI D1-
                      GND 3V3 21                  40 BT
          VGA Out CSYNC  GP31 22                  39 GP47
                          GND 23                  38 GND
      VGA Out Dark Blue  GP32 24                  37 GP46  IR RX
     VGA Out Light Blue  GP33 25        XL        36 GP45
     VGA Out Dark Green  GP34 26                  34 GP44
    VGA Out Light Green  GP35 27                  33 GP43
                          GND 28                  32 GND
       VGA Out Dark Red  GP36 29                  31 GP39  Serial RX
      VGA Out Light Red  GP37 30                  31 GP38  Serial TX


VGA Out GPIO                           VGA 15-way Socket


VGA Out Dark Blue    ---  1K  ---      3 Blue
VGA Out Light Blue   --- 470R ---      3 Blue

VGA Out Dark Green   ---  1K  ---      2 Green
VGA Out Light Green  --- 470R ---      2 Green

VGA Out Dark Red     ---  1K  ---      1 Red 
VGA Out Light Red    --- 470R ---      1 Red


VGA Out CSYNC        ---  47R ---      13 HSYNC


## UART Comms
I use a Raspberry Pi Debug Probe (link) for programming the Pico (2) and for UART comms.

I prefer to use minicom as a serial monitor. Some - the one included with VS code for example - prevent
some keystrokes from being transmitted.

`$ minicom -b 115200 -w -D /dev/ttyACM0` 

Then enable carriage returns with Ctrl-A U.


## PIO State Machine Usage for Pico2

PIO      SM       Size  Needs PIO1*  Usage
0        0        6         y       vga_capture_program
0        1        11                vga_detect_vsync_program
0        2        14                vga_detect_vsync_on_csync_program
0        3
Total             31

1        0
1        1        13                rgb5_150_mhz_rp235x_program (rrggbb for vga out) 
1        2        15                hsync5_program (csync for vga out)      
1        3        1                 logic_capture
Total             29

2        0        31                nec_ir_rx_program
2        1
2        2
2        3
Total             31


## PIO State Machine Usage for Pico LiPo 2 W XL

PIO 0 (GPIO_BASE=0)
SM       Size  Needs PIO1*  GPIO(s)  Usage
0        6         y                 vga_capture_program
1        11                          vga_detect_vsync_program
2        14                          vga_detect_vsync_on_csync_program
3        1                           trigger and/or logic_capture for pin(s) using GPIO_BASE=0
Total             32


PIO 1 (GPIO_BASE=16)
0        1                           Could move logic_capture here to capture channels 16-48
1        13                          rgb5_150_mhz_rp235x_program (rrggbb for vga out) 
2        15                          hsync5_program (csync for vga out)
;3        1                           logic_capture (moved to PIO0)
Total             28

PIO 2 (GPIO_BASE=16)
0        31                          nec_ir_rx_program
1
2
;3
3        1                           trigger and/or logic_capture for pin(s) using GPIO_BASE=16. This needn't have been moved. Move back? Would save unloading and reloading ir_rx each time we capture
Total             32

* PIO1 has PIO features that were introduced in RP235x devices; RP2040 uses PIO0.


## Weird Issues

Strange thing I experienced just now

I've been developing this project mainly on a Raspberry Pi 5. Every so often I need to be away and don't really
want to take my Pi, so I copy the project folder with the git history etc. onto a Chromebook, on which
I'm using Linux (Debian) and VS Code. This time when I copied the folder back to the Pi I found that
the Git history wasn't working, either in VS Code or on the command line. I tried a number of times, thinking
I was doing something wrong and each time the same thing would happen. Finally, I tried zipping the folder on the
Chromebook and unzipping it on the Pi, and that worked. Chrome OS was updated since while I was away (using the
Chromebook), so maybe that did something.


The HSTX DVI output is sometimes locking up. Very occasionally it will recover by itself (it just did it 3 times within a couple of minutes), but more often than not
it needs to be dvi_deinit()'ed and dvi_init()'ed., which has so far worked 77 times in a row. You can get it to fail
by playing the graphics demo (Hunter Adams'). It doesn't fail, however, when capturing the VGA from another
Pico that's running the same demo (only, in 16 colours rather than 2 per line on the Pico2). ie when we're 
writing stuff to the VGA memory, which is constantly going out to the VGA pins at 60Hz and then being captured by
a state machine and writes to the DVI memory, which is constantly going out to the DIV pins.

Also, there are times when I just can't get it to lock up at all.


I think I've found the issue. I've found something that certainly makes things worse and that is the 'sleep_ms()' SDK function.
If I stop using that with the graphics demo running, the dvi output is never lost. I've seen it flicker off very briefly, but then on again all by itself.
I have managed to lose it once, by scrolling the captured plot with an ir remote control. I had to reset it manually, with the 'r' key on the keyboard.

Arrgh, I've just done it again using ir - actually two remotes for double the auto-repeat rate. I wonder if it could be a power / noise issue?



There's also another bug, which occurs if you scroll way past the end of the trace of the logic analyser.

Noisy CSYNC signal

When sharing the VGA logic level pins (from the RP2350B) with a Pico 2 (for VGA to DVI conversion for example)
the Pico 2 seems to see a lot of noise on the CSYNC line, so I'm going to add a buffer/driver.

Try a 74HC245N as I have loads of them

            3.3V    DIR  1 |                 | 20  VCC    3.3V 
           CSYNC IN  A1  2 |                 | 19  (OE)
            GND      A2  3 |                 | 18  B1     CSYNC OUT to resistors to VGA connector
            GND      A3  4 |                 | 17  B2
            GND      A4  5 |    74HC245N     | 16  B3
            GND      A5  6 |                 | 15  B4
            GND      A6  7 |                 | 14  B5
            GND      A7  8 |                 | 13  B6
            GND      A8  9 |                 | 12  B7
            GND    GND  10 |                 | 11  B8

   INPUTS      OPERATION
   (OE) DIR
     L   L     B data to A bus
     L   H     A data to B bus   
     H   X     Isolation

Note it is particularly bad when the Pico 2 is powered from its own USB port, as opposed to VS from the VS of Pico LiPo W XL. Also,
if a debug probe is attached (powered or not - I'm not sure) the problem is even worse. 

Yes, that stops the problem, although the VGA output is missing a vertical line of pixels - due to the propogation delay of the buffer.
To fix that all the RGB lines could be buffered too. Yes, that worked.


            3.3V    DIR  1 |                 | 20  VCC    3.3V 
           CSYNC IN  A1  2 |                 | 19  (OE)
            3.3V     A2  3 |                 | 18  B1     CSYNC OUT to resistors to VGA connector
       DARK BLUE IN  A3  4 |                 | 17  B2
      LIGHT BLUE IN  A4  5 |    74HC245N     | 16  B3     DARK BLUE OUT
      DARK GREEN IN  A5  6 |                 | 15  B4     LIGHT BLUE OUT
     LIGHT GREEN IN  A6  7 |                 | 14  B5     DARK GREEN OUT
        DARK RED IN  A7  8 |                 | 13  B6     LIGHT GREEN OUT
       LIGHT RED IN  A8  9 |                 | 12  B7     DARK RED OUT
            GND    GND  10 |                 | 11  B8     LIGHT RED OUT


So, we are getting false CSYNC pulses, presumably. Possibly missing CSYNC pulses. Hmm... Increasing the drive strength of CSYNC to 12mA fixes it.

I repeated the test with a Pimoroni Pico Plus 2 (not XL) instead of the Pico 2, and it still fails (misses, or possibly gets false, CSYNC syncs). I used
the same UF2 as the Pico 2, and it (Pimoroni's Pico Plus 2) seems to work fine, despite having some differences (mainly in the power supply area).