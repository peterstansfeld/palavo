# palavo

### PIO-Assisted Logic Analyser with VGA Output*

Palavo uses the PIO (Programmable Input Output) feature of the Raspberry Pi RP235x microcontroller to capture the state of its GPIO pins over time. It then displays those captured states on a VGA monitor. Calling Palavo a logic analyser is a bit of a stretch, as it does very little analysis, but it does allow the user, via a simple interface, to analyse the logic themselves. It also allows the user to specify which GPIO pins to capture, the frequency at which they should be captured, which GPIO pin to use as the trigger pin to start the capturing, and what type of trigger that should be. The user interface is controlled using a serial terminal (on a PC), a keyboard to serial terminal adapter, and/or an [Argon IR Remote](https://argon40.com/products/argon-remote).

This project was inspired by, and uses code from, Raspberry Pi's [Logic Analyser example in the SDK.](https://github.com/raspberrypi/pico-examples/tree/master/pio/logic_analyser) as well as Hunter Adams' [PIO-Based VGA Graphics Driver for RP2040](https://github.com/vha3/Hunter-Adams-RP2040-Demos/blob/master/VGA_Graphics/README.md). I created Palavo to help me *view* the inputs and outputs of various PIO programs I was working on, including the VGA driver.

The VGA output uses a resolution of 640 x 480 with 6-bit colour (2 red, 2 green, 2 blue), and uses CSYNC (combined sync) instead of HSYNC (horizontal sync) and VSYNC (vertical sync) to save a GPIO pin. Not all VGA monitors support CSYNC, but many do. More details about CSYNC can be found on this [HDRetrovision blog post](https://www.hdretrovision.com/blog/2018/10/22/engineering-csync-part-1-setting-the-stage).

\*The *Assisted* in *PIO-Assisted* doesn't really do PIO justice, as without it Palavo would not be possible.


## How to build Palavo

There are a number of configurations for Palavo, which can use either the A variant (32 GPIOs) or the B variant (48 GPIOs) of the RP235x, or even both. Configurations are defined with the `PALAVO_CONFIG` variable in the `cmake` command line when creating a build directory.

## Configuration 0

#### PALAVO_CONFIG=0

### Hardware

At its simplest, a Raspberry Pi Pico 2 and a few resistors can be used:

```
      VGA_Out_VSYNC  GP0  1                40 VBUS
VGA_Out_HSYNC_CSYNC  GP1  2                39 VSYS
                     GND  3                38 GND
  VGA_Out_Dark_Blue  GP2  4                37 3V3 EN
 VGA_Out_Light_Blue  GP3  5                36 3V3 OUT
 VGA_Out_Dark_Green  GP4  6                35 ADC VREF
VGA_Out_Light_Green  GP5  7                34 GP28
                     GND  8                33 ADC GND
   VGA_Out_Dark_Red  GP6  9                32 GP27
  VGA_Out_Light_Red  GP7 10     PICO 2     31 GP26
          Serial_TX  GP8 11                30 RUN
          Serial_RX  GP9 12                29 GP22
                     GND 13                28 GND
      Infra_Red_RX  GP10 14                27 GP21
                    GP11 15                26 GP20
                    GP12 16                25 GP19
                    GP13 17                24 GP18
                    GND  18                23 GND
                    GP14 19                22 GP17
                    GP15 20                21 GP16
```

The VGA_Out_signals need to be fed into a resistor network to provide the voltages for the Red, Green, and Blue colour pins on a VGA socket or cable that's attached to a VGA monitor:

```
        Pico 2 GPIO  Resistor  VGA 15-way Socket 
------------------------------------------------
VGA_Out_HSYNC_CSYNC  ---47R--  13 HSYNC
                GND  ---0R---  5 GND
  VGA_Out_Dark Blue  ---1K---  3 Blue
 VGA_Out_Light Blue  --470R--  3 Blue
 VGA_Out_Dark Green  ---1K---  2 Green
VGA_Out_Light Green  --470R--  2 Green
   VGA_Out_Dark Red  ---1K---  1 Red
  VGA_Out_Light Red  --470R--  1 Red
```

The Serial_RX and Serial_TX can be connected to a PC via a 3.3V logic level UART to USB serial adapter. I use [Raspberry Pi's Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) as it can also be used to program and debug the Pico via its Debug interface.

Alternatively the Serial_RX and Serial_TX can be connected to a keyboard to serial terminal adapter, which is a Pico 2 in USB host mode with a keyboard attached. The details for this can be found in this repository.


The Infra_Red_RX pin, along with connections to 3.3V and GND, can be connected to an infra-red receiver. I use this [Grove IR Receiver](https://thepihut.com/products/grove-infrared-receiver), and Palavo accepts commands transmitted from this [Argon IR Remote](https://argon40.com/products/argon-remote) control.


### Firmware

There may be a simpler way, using Raspberry Pi's Pico Visual Studio Code Extension perhaps, but I haven't experimented with that enough yet (todo). This is how I build the firmware (on a Raspberry Pi 5):

Follow the instructions in Appendix C: Manual toolchain setup of [Getting started with Raspberry Pi Pico-series](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

Clone this repository (Palavo) into a suitable location on your PC.

Enter the `palavo` directory:

`$ cd palavo`

If one doesn't already exist, create a `build` directory and enter it:

`$ mkdir build`

`$ cd build`

Create a suitably-named directory (the target for this example is a Raspberry Pi Pico 2) and enter that directory:

`$ mkdir pico2`

`$ cd pico2`

Create a suitably-named directory for this particular configuration of Palavo and enter that directory:

`$ mkdir config0`

`$ cd config`


Specify where the pico-sdk directory can be found:

`$ export PICO_SDK_PATH=~/pico/pico-sdk`

Specify where the top level CMakeLists.txt file can be found - in this case
it's the great-grandparent directory `../../../`, specify the board `pico2`, 
and specify the configuration `PALAVO_CONFIG=0`:

`$ cmake ../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=0`

Then build it:

`$ make -j4`

This should generate, amongst other files, a `palavo.uf2` file and a `palavo.elf` file.

To program the RP2350 using the `palavo.uf2` file, put the Pico into boot mode and copy the file onto the drive that appears on you  PC.

To program the RP2350 using the `palavo.elf` file using Openocd and a Raspberry Pi Debug Probe:

`$ openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "init; reset; program palavo.elf verify reset exit"`

Instead of remembering the above command line each time, you can copy the script file `make-and-flash.sh` from the palavo directory:

`$ cp ../../make-and-flash.sh .`

Using a text editor modify the `adapter_serial_no` variable to that of your Debug Probe. If you don't want or need to specify a serial number, blank the `target_adapter_cmnd` variable.

Then make and flash the RP235x:

`$ ./make-and-flash.sh`


### Testing

If all went well, when Palavo starts you should see something like the following screen on your VGA monitor:

![A monitor screen displaying the main coloured traces of a section of the logic levels of various GPIO pins. Above the main traces, at the top of the screen is the Palavo logo on the left, and then various adjustable settings. Underneath the main traces is a minimap of traces of the whole of each captured channel. Beneath the minimap, at the bottom of the screen, is a status bar.](image.jpg)

Open a serial terminal on your PC. I use Minicom with this command line (you may need to change the device - the bit after the `-D`):

`$ minicom -b 115200 -w -D /dev/ttyACM0`

Press the 'h' key and something like the following help screen should appear:

```
HELP

Press...

LEFT / RIGHT to scroll one sample period left / right  
CTRL-LEFT / CTRL-RIGHT to scroll to previous / next edge on the selected channel (ch)  
PGUP / PGDN to scroll one page left / right  
HOME / END to scroll to the beginning / end  
TAB / SHIFT-TAB to select the next / previous setting  
UP / DOWN to increase / decrease the selected setting  
c to capture a sample using the settings  
z to zoom to fit all the samples on one page  
+ / - to zoom in / out  
= to set zoom to 1:1  
m to measure VGA timings  
h to show this help window  
a to show the about window  
SPACE to play / pause graphics demo  
Press any key to close this window  
```

Hopefully, the above instructions are clear enough to be able to get started using Palavo. If so, congratulations! I hope you find it interesting, and maybe even useful.


## Configuration 1

#### PALAVO_CONFIG=1

"That's all well and good." I hear you say, "But can't the Pico do DVI with its HSTX peripheral?". Well, yes it can. Try this:

### Hardware

```
      VGA_Out_VSYNC  GP0  1                40 VBUS
VGA_Out_HSYNC_CSYNC  GP1  2                39 VSYS
                     GND  3                38 GND
  VGA_Out_Dark_Blue  GP2  4                37 3V3 EN
 VGA_Out_Light_Blue  GP3  5                36 3V3 OUT
 VGA_Out_Dark_Green  GP4  6                35 ADC VREF
VGA_Out_Light_Green  GP5  7                34 GP28
                     GND  8                33 ADC GND
   VGA_Out_Dark_Red  GP6  9                32 GP27
  VGA_Out_Light_Red  GP7 10     PICO 2     31 GP26
          Serial_TX  GP8 11                30 RUN
          Serial_RX  GP9 12                29 GP22
                     GND 13                28 GND
      Infra_Red_RX  GP10 14                27 GP21
                    GP11 15                26 GP20
           DVI_D0+  GP12 16                25 GP19  DVI_D2+
           DVI_D0-  GP13 17                24 GP18  DVI_D2-
                    GND  18                23 GND
           DVI_CK+  GP14 19                22 GP17  DVI_D1+
           DVI_CK-  GP15 20                21 GP16  DVI_D1-
```

In addition to the previous configuration's hardware, the DVI pins (GP12-GP19) should be connected to a DVI monitor via a [Pico DVI Sock](https://github.com/Wren6991/Pico-DVI-Sock) and an HDMI-shaped cable. Originally designed by Raspberry Pi's Luke Wren, Adafruit now make their own version called the [DVI Sock for Pico](https://www.adafruit.com/product/5957). There are other products, such as the [PiCowBell HSTX DVI Output for Pico](https://www.adafruit.com/product/6363), that could be used instead.

With this setup the VGA_Out output is mirrored to the DVI pins, which is amazing for a less-than-$1 microcontroller.


### Firmware

In the `build/pico2` directory create a different, but still suitably-named, directory, and enter that directory:

`$ mkdir config1`

`$ cd config1`

Repeat the rest of the previous build process, only use this CMAKE command instead:

`$ cmake ../../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=1`


### Testing

If all went well, when Palavo starts you should see something like the following test screen on your *DVI* monitor:

![A monitor screen displaying lots of vertical coloured bars of various colours.](image.jpg)

Shortly after that, you should see the same screen as you can see on the VGA monitor (asuming you have two monitors):

![A monitor screen displaying coloured traces of a section of the logic levels of various GPIO pins. Above the traces, at the top of the screen is the Palavo logo on the left, and then various adjustable settings. Underneath the main traces is a minimap of the whole of each trace. Beneath the minimap, at the bottom of the screen, is a status bar.](image.jpg)

Press the 'h' key and the same help menu should appear with the addition of this item:

![v to cycle DVI modes: mirror VGA out -> test -> VGA in ](image.jpg)

In 'mirror VGA out' mode, whatever is displayed on VGA_Out is also displayed on DVI.  
In 'test' mode, a test screen is displayed on DVI.  
In 'VGA in' mode, whatever is detected on VGA_In is also displayed on DVI. In addition to VGA_In_CSYNC, VGA_HSYNC_In and VGA_VSYNC_In are used for synchronisation.  


### Thoughts

This is great, but it doesn't leave many pins free to capture external input signals - only 7, namely: GP15, 20, 21, 22, 26, 27 and 28. So...


## Configuration 13

#### PALAVO_CONFIG=13

If we use another Pico 2, and change the VGA_Out\*s to VGA_In\*s we can to make a VGA (6-bit logic level) - DVI Converter. If we also lose, or move, the Infra_Red_RX we can squeeze in VGA_Out_CSYNC and VGA_Out_RGB to provide a monochrome VGA output for testing purposes.

```
       VGA_In_VSYNC  GP0  1                40 VBUS
 VGA_In_HSYNC_CSYNC  GP1  2                39 VSYS
                     GND  3                38 GND
   VGA_In_Dark_Blue  GP2  4                37 3V3 EN
  VGA_In_Light_Blue  GP3  5                36 3V3 OUT
  VGA_In_Dark_Green  GP4  6                35 ADC VREF
 VGA_In_Light_Green  GP5  7                34 GP28  Infra_Red_RX
             Ground  GND  8                33 ADC GND
    VGA_In_Dark_Red  GP6  9                32 GP27
   VGA_In_Light_Red  GP7 10     PICO 2     31 GP26
          Serial_TX  GP8 11                30 RUN
          Serial_RX  GP9 12                29 GP22
                     GND 13                28 GND
     VGA_Out_CSYNC  GP10 14                27 GP21
       VGA_Out_RGB  GP11 15                26 GP20
           DVI_D0+  GP12 16                25 GP19  DVI_D2+
           DVI_D0-  GP13 17                24 GP18  DVI_D2-
                    GND  18                23 GND
           DVI_CK+  GP14 19                22 GP17  DVI_D1+
           DVI_CK-  GP15 20                21 GP16  DVI_D1-
```

This can then sit on top of, or underneath, the Pico 2 (Configuration 0) with *only* the connections we need, namely all the VGA_In pins (which connect to the VGA_Out pins), VSYS, and the GNDs.





Something fun to do here is get Hunter Adams' [VGA_Graphics_Primitives](https://github.com/vha3/Hunter-Adams-RP2040-Demos/tree/master/VGA_Graphics/VGA_Graphics_Primitives) example working on a a Pico or Pico 2:

                     GP0  1                40 VBUS
                     GP1  2                39 VSYS
                     GND  3                38 GND
                     GP2  4                37 3V3 EN
                     GP3  5                36 3V3 OUT
                     GP4  6                35 ADC VREF
                     GP5  7                34 GP28
                     GND  8      PICO      33 ADC GND
                     GP6  9       or       32 GP27
                     GP7 10     PICO 2     31 GP26
                     GP8 11    running     30 RUN
                     GP9 12      VGA       29 GP22  
                     GND 13    Graphics    28 GND
                    GP10 14   Primitives   27 GP21 VGA_Out_Red
                    GP11 15      Demo      26 GP20 VGA_Out_Blue
                    GP12 16                25 GP19 VGA_Out_Light_Green
                    GP13 17                24 GP18 VGA_Out_Dark_Green
                    GND  18                23 GND
                    GP14 19                22 GP17 VGA_Out_VSYNC
                    GP15 20                21 GP16 VGA_Out_HSYNC

And then connect it to a Pico 2 (or similar) running Palavo in Configuration 13:


```
RPi PICO running           RP2350x board running
VGA Graphics Primitives    Palavo in Configuration 13
----------------------------------------------------
VGA_Out_VSYNC              VGA_In_VSYNC
VGA_Out_HSYNC              VGA_In_HSYNC_CSYNC
GND                        GND
VGA_Out_Blue               VGA_In_Dark_Blue & VGA_In_Light_Blue
VGA_Out_Dark_Green         VGA_In_Dark_Green
VGA_Out_Light_Green        VGA_In_Light_Green
VGA_Out_Red                VGA_In_Dark_Red & VGA_In_Light_Red
```

Start both devices and hopefully you should see Hunter's demo appearing on the DVI monitor... Told you it was fun.

If you're wondering why Palavo uses 6-bit colour (RRGGBB), it's because when converting the VGA output to DVI, using the HSTX peripheral on the RP235x, the colours remain looking the same as the VGA output. This is due to each colour having the same number of bits, which is not the case with Hunter's and Bruce's 4-bit colour (RGGB). To save SRAM used by the VGA driver each horizontal line consists of only two 6-bit colours.


## Configuration 2

#### PALAVO_CONFIG=2

The trouble with the original configuration (Configuration 0) is that we're using quite a few GPIO pins for the VGA_Out, Serial_TX, Serial_RX, and Infra_Red_RX. What if we wanted to capture 24 external inputs? We can't with a Pico 2. But we can with a board that uses the B variant of the RP2350. The RP2350B has 48 GPIO pins, and we only need 7 for VGA_Out, or 8 for DVI_Out. The slight incovenience with DVI_Out is that it's fixed on pins GP12-GP19, whereas with VGA_Out we can put its 7 signals on whichever pins we like. Allow me introduce you to the [Pimoroni Pico LiPo 2 XL W](https://shop.pimoroni.com/products/pimoroni-pico-lipo-2-xl-w):

### Hardware

```
                       GP0  1                  60 VBUS
                       GP1  2                  59 VSYS
                       GND  3                  58 GND
                       GP2  4                  57 3V3 EN
                       GP3  5                  56 3V3 OUT
                       GP4  6                  55 ADC VREF
                       GP5  7                  54 GP28
                       GND  8                  53 ADC GND
                       GP6  9                  52 GP27
                       GP7 10 PICO LIPO 2 XL W 51 GP26
                       GP8 11                  30 RUN
                       GP9 12                  49 GP22
                       GND 13                  48 GND
                      GP10 14                  47 GP21
                      GP11 15                  46 GP20
                      GP12 16                  45 GP19
                      GP13 17                  44 GP18
                      GND  18                  43 GND
                      GP14 19                  42 GP17
                      GP15 20 ________________ 41 GP16
                   GND 3V3 21                  40 BT
       VGA_Out_CSYNC  GP31 22                  39 GP47
                       GND 23                  38 GND
   VGA_Out_Dark_Blue  GP32 24                  37 GP46  Infra_Red_RX
  VGA_Out_Light_Blue  GP33 25        XL        36 GP45
  VGA_Out_Dark_Green  GP34 26                  34 GP44
 VGA_Out_Light_Green  GP35 27                  33 GP43
                       GND 28                  32 GND
    VGA_Out_Dark_Red  GP36 29                  31 GP39  Serial_RX
   VGA_Out_Light_Red  GP37 30                  31 GP38  Serial_TX
```

It's *so* long. And just *look* at all those free GPIO pins. Enough to intercept and examine the signals from a keyboard's matrix, perhaps? It's pictured here attached, mostly, to a [Pimoroni Pico Omnibus](https://shop.pimoroni.com/products/pico-omnibus) with 24 signals from a keyboard matrix connected to GP0 to GP26. 


![A Pimoroni PICO LIPO 2XL W attached, well, 66.67% attached to a Pimoroni Pico .](image.jpg)


### Firmware

In the `build` directory create a different, but still suitably-named, directory, and enter that directory:

`$ mkdir pimoroni_pico_lipo2xl_w`

`$ cd pimoroni_pico_lipo2xl_w`

Repeat the rest of the previous build process, only use this CMAKE command instead:

`$ cmake ../../ -DPICO_BOARD=pimoroni_pico_lipo2xl_w_rp2350 -DPALAVO_CONFIG=2`


## Testing

The screen on the VGA monitor should look the same as it does in the first configuration, except that the 'base' and 'trigger' settings can be set to use GP0 to GP47 (rather than just GP0 to GP31).

If you're missing the DVI output, you can make a VGA to DVI converter and connect the VGA_Out pins on your Palavo logic analyser to the VGA_In pins on the converter.


#### Making a VGA to DVI converter with a Pico 2
 
 (Or any other suitable board with an RP235x.)

 Palavo can be configured to be a 6-bit-colour, 3.3V-logic-level, VGA to DVI converter. It is the same as `PALAVO_CONFIG=1` mode except that it starts up in the 'capture VGA in' mode rather than the 'mirror VGA out' mode. To build the firmware repeat the rest of the previous build process, only use this CMAKE command instead:

`$ cmake ../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=5`


## Configuration 8

#### PALAVO_CONFIG=8

What if we wanted to capture 32 contiguous channels? Unfortunately, it's not possible with the Pimoroni Pico LiPo 2 XL W because some GPIO pins are not broken out, and others are used for the on-board PSRAM and the wireless module. The answer is to use something that breaks out every GPIO pin, and the only boards which do that, that I know of, are the [Pimoroni PGA2350](https://shop.pimoroni.com/products/pga2350) and the [Solder Party RP2350 Stamp XL](https://www.solder.party/docs/rp2350-stamp-xl/). Here's the Stamp XL housed in a [Solder Party RP2xxx Stamp Carrier Basic](https://www.solder.party/docs/rp2xxx-stamp-carrier-basic/):

```
 G 0                           USB                          GND G
 G 1  VGA_Out_CSYNC                                          3V G
 G 2  VGA_Out_Dark_Blue                                      5V G
 G 3  VGA_Out_Light_Blue  -------------------               BAT G
 G 4  VGA_Out_Dark_Green                     |             BOOT G
 G 5  VGA_Out_Light_Green                    |              RST G
 G 6  VGA_Out_Dark_Red                       |              CLK G
 G 7  VGA_Out_Light_Red                      |               IO G
 G 8  Serial_TX   |                          |               EN G
 G 9  Serial_RX   |                          |         BAT STAT G
 G 10 Infra_Red_RX|                          |               47 G
 G 11             |      RP2350 Stamp XL     |               46 G
 G 12             |                          |               45 G
 G 13             |                          |               44 G
 G 14             |                          |               43 G
 G 15             |                          |               42 G
 G 16             |                          |               41 G
 G 17             |                          |               40 G
 G 18             |                          |               39 G
 G 19             |                          |               38 G
 G 20              ---------------------------               37 G
 G 21              RP2xxx Stamp Carrier Basic                36 G

      22  23  24  25  26  27  28  29  30  31  32  33  34  35
      G   G   G   G   G   G   G   G   G   G   G   G   G   G

```

```

                        B
                        O
            3     U  U  O
         V  V     S  S  T
         B  3  R  B  B  S
         U  E  U  D  D  E
    G 1  S  N  N  P  M  L  46 G
   2  G  0  3  A  S  S  47 G  45
   4  3     V  D  W  W     43 44
   6  5     3  C  C  D     41 42
   8  7        V  L  I     39 40
   10 9        R  K  O     37 38
   12 11       E           35 36
   14 13       F           33 34
   16 15 19 21 23 25 27 29 31 32
    G 17 18 20 22 24 26 28 30 G
      
         Pimoroni PGA2350

```

```
                                             B
                                             O
                     3           U     U     O
               V     V           S     S     T
               B     3     R     B     B     S
               U     E     U     D     D     E
    G    1     S     N     N     P     M     L     46    G
               
   2     G     0     3     A     S     S     47    G     45
                     V     D     W     W
   4     3           3     C     C     D           43    44
                           V     L     I
   6     5                 R     K     O           41    42
                           E           
   8     7                 F                       39    40

   10    9                                         37    38
                      Pimoroni PGA2350
   12    11                                        35    36

   14    13                                        33    34

   16    15    19    21    23    25    27    29    31    32

    G     17    18    20    22    24    26    28    30   G








```











```
                                                                             GP0  1                  60 VBUS
                                                                             GP1  2                  59 VSYS
                                                                             GND  3                  58 GND
                                                                             GP2  4                  57 3V3 EN
                                                                             GP3  5                  56 3V3 OUT
                                                                             GP4  6                  55 ADC VREF
                                                                             GP5  7                  54 GP28
                                                                             GND  8                  53 ADC GND
                                                                             GP6  9                  52 GP27
                                                                             GP7 10 PICO LIPO 2 XL W 51 GP26
                                                                             GP8 11                  30 RUN
                                                                             GP9 12                  49 GP22
                                                                             GND 13                  48 GND
                                                                            GP10 14                  47 GP21
                                                                            GP11 15                  46 GP20
                                                                            GP12 16                  45 GP19
                                                                            GP13 17                  44 GP18
                                                                            GND  18                  43 GND
                                                                            GP14 19                  42 GP17
                                                                            GP15 20 ________________ 41 GP16
                                                                         GND 3V3 21                  40 BT
                                                             VGA_Out_CSYNC  GP31 22                  39 GP47
                                                                             GND 23                  38 GND
                                          -------------- VGA_Out_Dark_Blue  GP32 24                  37 GP46  Infra_Red_RX
                                                        VGA_Out_Light_Blue  GP33 25        XL        36 GP45
                                                        VGA_Out_Dark_Green  GP34 26                  34 GP44
                                                       VGA_Out_Light_Green  GP35 27                  33 GP43
                                                                             GND 28                  32 GND
                                                          VGA_Out_Dark_Red  GP36 29                  31 GP39  Serial_RX
                                       ----------------- VGA_Out_Light_Red  GP37 30                  31 GP38  Serial_TX



                                                           VGA_In_Dark Blue  GP0  1                  40 VBUS
                                                          VGA_In_Light Blue  GP1  2                  39 VSYS
                                                                             GND  3                  38 GND
                                                          VGA_In_Dark_Green  GP2  4                  37 3V3 EN
                                                         VGA_In_Light_Green  GP3  5                  36 3V3 OUT
                                                   |        VGA_In_Dark_Red  GP4  6                  35 ADC VREF
                                             ------|------ VGA_In_Light Red  GP5  7                  34 GP28
                                                                             GND  8                  33 ADC GND
                                                          VGA_Out_Dark Blue  GP6  9                  32 GP27  VGA_In_VSYNC
                                                         VGA_Out_Light Blue  GP7 10      PICO  2     31 GP26  VGA_In_HSYNC_OR_CSYNC
                                                         VGA_Out_Dark Green  GP8 11                  30 RUN
                                                        VGA_Out_Light Green  GP9 12                  29 GP22  VGA_Out_CSYNC
                                                                             GND 13                  28 GND
                                                          VGA_Out_Dark Red  GP10 14                  27 GP21
                                                         VGA_Out_Light Red  GP11 15                  26 GP20
                                                                   DVI_D0+  GP12 16                  25 GP19  DVI_D2+
                                                                   DVI_D0-  GP13 17                  24 GP18  DVI_D2-
                                                                            GND  18                  23 GND
                                                                   DVI_CK+  GP14 19                  22 GP17  DVI_D1+
                                                                   DVI_CK-  GP15 20                  21 GP16  DVI_D1-
````









# End of Document

Everything from here is stuff I may use and don't want to delete just yet






## Pinout for Raspberry Pi Pico 2

```
   VGA_In_Dark Blue  GP0  1                  40 VBUS
  VGA_In_Light Blue  GP1  2                  39 VSYS
                     GND  3                  38 GND
  VGA_In_Dark Green  GP2  4                  37 3V3 EN
 VGA_In_Light Green  GP3  5                  36 3V3 OUT
    VGA_In_Dark Red  GP4  6                  35 ADC VREF
   VGA_In_Light Red  GP5  7                  34 GP28  Infra-red RX
                     GND  8                  33 ADC GND
  VGA_Out_Dark Blue  GP6  9                  32 GP27  VGA_In_VSYNC
 VGA_Out_Light Blue  GP7 10     PICO 2       31 GP26  VGA_In_HSYNC_OR_CSYNC
 VGA_Out_Dark Green  GP8 11                  30 RUN  CAPTAIN RESETTI
VGA_Out_Light Green  GP9 12                  29 GP22  VGA_Out_CSYNC
                     GND 13                  28 GND
  VGA_Out_Dark Red  GP10 14                  27 GP21  Serial RX
 VGA_Out_Light Red  GP11 15                  26 GP20  Serial TX
           DVI D0+  GP12 16                  25 GP19  DVI D2+
           DVI D0-  GP13 17                  24 GP18  DVI D2-
                    GND  18                  23 GND
           DVI CK+  GP14 19                  22 GP17  DVI D1+
           DVI CK-  GP15 20                  21 GP16  DVI D1-
```

## Pinout for Pimoroni Pico LiPo 2 W XL

```
                       GP0  1                  60 VBUS
                       GP1  2                  59 VSYS
                       GND  3                  58 GND
                       GP2  4                  57 3V3 EN
                       GP3  5                  56 3V3 OUT
                       GP4  6                  55 ADC VREF
                       GP5  7                  54 GP28
                       GND  8                  53 ADC GND
                       GP6  9                  52 GP27
                       GP7 10 PICO LIPO 2 W XL 51 GP26
                       GP8 11                  30 RUN  CAPTAIN RESETTI (maybe needed not with PWR switch?)
                       GP9 12                  49 GP22
                       GND 13                  48 GND
                      GP10 14                  47 GP21
                      GP11 15                  46 GP20
                      GP12 16                  45 GP19
                      GP13 17                  44 GP18
                      GND  18                  43 GND
                      GP14 19                  42 GP17
                      GP15 20 ________________ 41 GP16
                   GND 3V3 21                  40 BT
       VGA_Out_CSYNC  GP31 22                  39 GP47
                       GND 23                  38 GND
   VGA_Out_Dark Blue  GP32 24                  37 GP46  Infra-red RX
  VGA_Out_Light Blue  GP33 25        XL        36 GP45
  VGA_Out_Dark Green  GP34 26                  34 GP44
 VGA_Out_Light Green  GP35 27                  33 GP43
                       GND 28                  32 GND
    VGA_Out_Dark Red  GP36 29                  31 GP39  Serial RX
   VGA_Out_Light Red  GP37 30                  31 GP38  Serial TX
```

## VGA 15-way Socket Wiring 

| VGA_Out_GPIO        | Resistor | VGA 15-way Socket |
| :---                |   :---:  | :---              |
|       VGA_Out_CSYNC |    47R   | 13 HSYNC          |
|                 GND |    0R    | 5 GND             |
|   VGA_Out_Dark Blue |    1K    | 3 Blue            |
|  VGA_Out_Light Blue |   470R   | 3 Blue            |
|  VGA_Out_Dark Green |    1K    | 2 Green           |
| VGA_Out_Light Green |   470R   | 2 Green           |
|    VGA_Out_Dark Red |    1K    | 1 Red             |
|   VGA_Out_Light Red |   470R   | 1 Red             |


## UART Comms
I use a Raspberry Pi Debug Probe (link) for programming the Pico (2) and for UART comms.

I prefer to use minicom as a serial monitor. Some - the one included with VS Code for example - prevent
some keystrokes from being transmitted.

`$ minicom -b 115200 -w -D /dev/ttyACM0` 

Then enable carriage returns with Ctrl-A U.


## PIO State Machine Usage for Pico2

```
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
```

## PIO State Machine Usage for Pico LiPo 2 W XL

PIO 0 (GPIO_BASE=0)
SM       Size  Needs PIO1*  GPIO(s)  Usage
0        6         y                 vga_capture_program (not used if USE_DVI=0)
1        11                          vga_detect_vsync_program (not used if USE_DVI=0)
2        14                          vga_detect_vsync_on_csync_program (not used if USE_DVI=0)
3        1                           trigger and/or logic_capture for pin(s) using GPIO_BASE=0
Total    32

PIO 1 (GPIO_BASE=16)
0
1        13                          rgb5_150_mhz_rp235x_program (rrggbb for vga out) 
2        15                          hsync5_program (csync for vga out)
3        1                           trigger and/or logic_capture for pin(s) using GPIO_BASE=16
Total    29

PIO 2 (GPIO_BASE=16)
0        31                          nec_ir_rx_program
1
2
3
Total    31

* PIO1 has PIO features that were introduced in RP235x devices; RP2040 uses PIO0.



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
a state machine and writes to the DVI memory, which is constantly going out to the DVI/HSTX pins.

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
To fix that all the RGB lines could be buffered too. Yes, that worked - in that it stopped the VGA to DVI Pico2 going haywire, although
it has affected the VGA signal. Hmm.


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


* whereas using Hunter Adams' original 4-bit colour (1 red, 2 green, 1 blue) they don't.



How many combinations of Palavo are there?

1. Using an RP2350A or RP2350B with VGA output, DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=0 (0b000))
2. Using an RP2350A or RP2350B with VGA output, no DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=1 (0b001))
3. Using an RP2350B with VGA output, DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=2 (0b010))
4. Using an RP2350B with only VGA output, i.e. no DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=3 (0b011))
5. Using an RP2350A or RP2350B with VGA output, DVI output, and defaults to outputting the VGA_In_signals to DVI, rather than its own captured signals. (PALAVO_CONFIG=4 (0b100))

N.B. Available for capture does NOT mean you can put external signals into any GPIO pins that are used as outputs. It means you can still capture those output signals.

## Palavo Config

Palavo can be configured, depending on the variant of the RP235x, in one of 5 modes and the mode is selected by passing the additional define into the CMAKE command line, above:

` -DPALAVO_CONFIG=X` where X is a number between 0 and 7, although 4 is the same as 0, and 6 is the same as 2.

| PALAVO_CONFIG | RP235x Variant | DVI Output at Startup | Use High GPIO for UI | DVI Out | VGA Out |
|     :---:     | :---           |         :---:         |        :---:         |  :---:  |  :---:  |
|       0       | A or B         |          n/a          |          No          |   No    |   Yes   |
|       1       | A or B         |        VGA Out        |          No          |   Yes   |   Yes   |
|       2       | B              |          n/a          |          Yes         |   No    |   Yes   |
|       3       | B              |        VGA Out        |          Yes         |   Yes   |   Yes   |
|       4       | A or B         |          n/a          |          No          |   No    |   Yes   |
|       5       | A or B         |        VGA In         |          No          |   Yes   |   Yes   |
|       6       | A or B         |          n/a          |          Yes         |   No    |   Yes   |
|       7       | A or B         |        VGA In         |          Yes         |   Yes   |   Yes   |



Use_Off_Pico_UI

Currently only Types 0, 1, 2 and 4, and 5 have been tested (4 is the same as 0). 

### Palavo Type Bits

```
 ___ USE_VGA_IN_TO_DVI (on start up display VGA In - i.e. a VGA to DVI converter.) 
| __ USE_GPIO_32_47 (Only set this if using an RP235xB.)
|| _ USE_DVI (Display either VGA In, VGA Out, or a test screen to DVI via HSTX.)
|||
000

```

The way I have my system setup is PALAVO_CONFIG=2 on a Pimoroni Pico LiPo 2 W XL, and for DVI output I use PLAVO_TYPE=5 on a Raspberry Pi Pico 2. This allows more contiguous GPIO pins free as inputs for capture. It will also allow for lots of RAM to be freed up for longer captures (not yet implemented - todo).








| VGA_Out_GPIO        | Resistor | VGA 15-way Socket |
| :---                |   :---:  | :---              |
|       VGA_Out_CSYNC |    47R   | 13 HSYNC          |
|                 GND |    0R    | 5 GND             |
|   VGA_Out_Dark Blue |    1K    | 3 Blue            |
|  VGA_Out_Light Blue |   470R   | 3 Blue            |
|  VGA_Out_Dark Green |    1K    | 2 Green           |
| VGA_Out_Light Green |   470R   | 2 Green           |
|    VGA_Out_Dark Red |    1K    | 1 Red             |
|   VGA_Out_Light Red |   470R   | 1 Red             |




the Pico uses a couple of PIO programs to output the 7 VGA_Out signals, whilst another PIO program simultaneously looks for a CSYNC 

In 'mirror VGA out' mode the Pico uses a couple of PIO programs to output the 7 VGA_Out signals, whilst another PIO program simultaneously looks for a CSYNC signal on CSYNC , and shortly after detecting one nothe PIO program copying the state of the Red, Green and Blue pins to the DVI's frame buffer, which is simultaneously and constantly being, via the HSTX peripheral, to the DVI pins.

In 'test' mode the above PIO programs are stopped and DVI's frame buffer is filled with a test pattern. 

In 'VGA in' mode the stopped PIO programs used in 'mirror VGA out' mode are restarted, only using the VGA_In pins for CSYNC detection and for Red, Green and Blue sampling. Another PIO program is started which is simultaneously looking for HSYNC and VSYNC signals.



In the `build` directory create a different, but still suitably-named, directory, and enter that directory:

`$ mkdir solderparty_rp2350_stamp_xl-DVI`

`$ cd solderparty_rp2350_stamp_xl-DVI`

Repeat the rest of the previous build process, only use this CMAKE command instead:

`$ cmake ../../ -DPICO_BOARD=solderparty_rp2350_stamp_xl -DPALAVO_CONFIG=1`

<!-- [text](../pico-sdk/src/boards/include/boards/solderparty_rp2350_stamp_xl.h) -->


PICO target board is solderparty_rp2350_stamp_xl.
CMake Error at /home/peter/pico/pico-sdk/src/boards/generic_board.cmake:22 (message):
  Unable to find definition of board 'solderparty_rp2350_stamp_xl' (specified
  by PICO_BOARD):

I specified    

     Looked for solderparty_rp2350_stamp_xl.h in /home/peter/pico/pico-sdk/src/boards/include/boards (additional paths specified by PICO_BOARD_HEADER_DIRS)
     Looked for solderparty_rp2350_stamp_xl.cmake in /home/peter/pico/pico-sdk/src/boards (additional paths specified by PICO_BOARD_CMAKE_DIRS)
Call Stack (most recent call first):
  /home/peter/pico/pico-sdk/src/board_setup.cmake:28 (include)
  /home/peter/pico/pico-sdk/src/CMakeLists.txt:15 (include)


  CMake Error at /home/peter/pico/pico-sdk/src/boards/generic_board.cmake:22 (message):
  Unable to find definition of board 'solderparty_rp2350_stamp_xl' (specified
  by PICO_BOARD):

     Looked for solderparty_rp2350_stamp_xl.h in /home/peter/pico/pico-sdk/src/boards/include/boards (additional paths specified by PICO_BOARD_HEADER_DIRS)
     Looked for solderparty_rp2350_stamp_xl.cmake in /home/peter/pico/pico-sdk/src/boards (additional paths specified by PICO_BOARD_CMAKE_DIRS)
Call Stack (most recent call first):
  /home/peter/pico/pico-sdk/src/board_setup.cmake:28 (include)
  /home/peter/pico/pico-sdk/src/CMakeLists.txt:15 (include)

```

      G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G
      21  20  19  18  17  16  15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
 G 22
 G 23                          RP2xxx Stamp Carrier Basic
 G 24                ------------------------------------------------
 G 25                |                                              |
 G 26                |                                              |                 -------
 G 27                |                                              |                 |     |
 G 28                |              RP2350 Stamp XL                 |                 | USB |
 G 29                |                                              |                 |     |
 G 30                |                                              |                 -------
 G 31                |                                              |
 G 32                |                                              |
 G 33                ------------------------------------------------     B
 G 34                                                                     O
 G 35                                                                     O
      36  37  38  39  40  41  42  43  44  45  46  47  BS  EN  IO  CLK RST T  BAT 5V  3V  GND
      G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G   G
```
```
      VGA_Out_VSYNC  GP0  1                40 VBUS
VGA_Out_HSYNC_CSYNC  GP1  2                39 VSYS
                     GND  3                38 GND
  VGA_Out_Dark_Blue  GP2  4                37 3V3 EN
 VGA_Out_Light_Blue  GP3  5                36 3V3 OUT
 VGA_Out_Dark_Green  GP4  6                35 ADC VREF
VGA_Out_Light_Green  GP5  7                34 GP28
                     GND  8                33 ADC GND
   VGA_Out_Dark_Red  GP6  9                32 GP27
  VGA_Out_Light_Red  GP7 10     PICO 2     31 GP26
          Serial_TX  GP8 11                30 RUN
          Serial_RX  GP9 12                29 GP22
                     GND 13                28 GND
      Infra_Red_RX  GP10 14                27 GP21
                    GP11 15                26 GP20

```

To open a new terminal window, enter:

```$ lxterminal -t "a helpful title" -e bash -c "minicom -b 115200 -w -D /dev/ttyACM1; bash"```



```
   VGA_In_Dark_Blue  GP0  1                40 VBUS
  VGA_In_Light_Blue  GP1  2                39 VSYS
                     GND  3                38 GND
  VGA_In_Dark_Green  GP2  4                37 3V3 EN
 VGA_In_Light_Green  GP3  5                36 3V3 OUT
    VGA_In_Dark_Red  GP4  6                35 ADC VREF
   VGA_In_Light_Red  GP5  7                34 GP28  Infra_Red_RX
                     GND  8                33 ADC GND
  VGA_Out_Dark_Blue  GP6  9                32 GP27  VGA_In_VSYNC
 VGA_Out_Light_Blue  GP7 10     PICO 2     31 GP26  VGA_In_HSYNC_OR_CSYNC
 VGA_Out_Dark_Green  GP8 11                30 RUN
VGA_Out_Light_Green  GP9 12                29 GP22  VGA_Out_CSYNC
                     GND 13                28 GND
  VGA_Out_Dark_Red  GP10 14                27 GP21  Serial_RX
 VGA_Out_Light_Red  GP11 15                26 GP20  Serial_TX
           DVI_D0+  GP12 16                25 GP19  DVI_D2+
           DVI_D0-  GP13 17                24 GP18  DVI_D2-
                    GND  18                23 GND
           DVI_CK+  GP14 19                22 GP17  DVI_D1+
           DVI_CK-  GP15 20                21 GP16  DVI_D1-
```
