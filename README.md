# palavo


## PIO-Accomplished Logic Analyser with VGA Output

TODO  
![A Raspberry Pi Pico displaying some logic on a VGA monitor. A long description follows.](config0-on-pico-photo.png "Raspberry Pi Pico displaying some logic on a VGA monitor.")


Palavo uses the PIO (Programmable Input Output) feature of Raspberry Pi's RP2040 or RP2350 microcontroller to capture the state of its GPIO pins over time, and then uses PIO to display those captured states on a VGA monitor. Calling Palavo a logic analyser is a bit of a stretch, as it does very little analysis, but it does allow the user, via a simple interface, to analyse the logic themselves. The interface is controlled using a serial terminal (on a PC), a keyboard to serial terminal adapter, and/or an infra-red remote control. The user can specify which GPIO pins to capture, the frequency at which they should be captured, which GPIO pin should be used to trigger the capture, and what type of trigger should be used.

When using the RP2350, Palavo can be configured to mirror its VGA output to DVI. I thought about changing the project name to Palavocado, but decided against it - mainly because I couldn't come up with anything for the 'c' to stand for, and because I wanted to be taken at least a little seriously. Also, I believe [palavo](https://en.wiktionary.org/wiki/palavo) means 'I shovelled' in Italian, and shovelling is very much what I felt I was doing when working on the source code.

The project was inspired by, and uses code from, Raspberry Pi's [Logic Analyser (Pico SDK) Example](https://github.com/raspberrypi/pico-examples/tree/master/pio/logic_analyser), Hunter Adams' [PIO-Based VGA Graphics Driver for RP2040](https://github.com/vha3/Hunter-Adams-RP2040-Demos/blob/master/VGA_Graphics/README.md), and Raspberry Pi's [DVI Out HSTX Encoder example for the Pico 2](https://github.com/raspberrypi/pico-examples/tree/master/hstx/dvi_out_hstx_encoder).

The VGA output uses a resolution of 640 x 480 with 6-bit colour (2 red, 2 green, 2 blue). It can use either HSYNC (horizontal sync) and VSYNC (vertical sync), or CSYNC (combined sync). Not all VGA monitors support CSYNC, but many do. Some details about CSYNC can be found on this [HDRetrovision blog post](https://www.hdretrovision.com/blog/2019/10/10/engineering-csync-part-2-falling-short).


## How to build Palavo

There are a number of configurations for Palavo, which can sample any of the 32 GPIOs of the RP2040/RP2350A, or the 48 GPIOs of the RP2350B. Pre-built binaries (`.uf2` files) are available to get up and running without the need to compile any firmware.  

If you do plan to build your own firmware, configurations are defined by adding the appropriate `PALAVO_CONFIG` variable to the `cmake` command line that's used to create a build directory, in which the firmware can then be built. Instructions for doing this are detailed for each of the configurations.


## Configuration 0

(PALAVO_CONFIG=0)


### Hardware

At its simplest, a [Raspberry Pi Pico or Pico 2](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html) and a few components can be used:

![A circuit diagram showing example hardware for Configuration 0. A long description follows.](images/config0-circuit.svg "Raspberry Pi Pico in Configuration 0")

*A Raspberry Pi Pico (or Pico 2) housed in a half-sized breadboard. Nine of the Pico's pins are connected to one side of various resistors, which are housed in a mini breadboard. The other side of the resistors are connected to the pins of VGA socket (or the plug on one end of a VGA cable). Here are the connections and resistor values:*

| Pico Pin | Function            | Resistor | VGA Socket Pin No |
|  :---:   | :---                |  :---:   |      :---:        |
|   GP0    | VGA_Out_VSYNC       |   47R    |       14          |
|   GP1    | VGA_Out_HSYNC_CSYNC |   47R    |       13          |
|   GND    | GND                 |    0R    |        5          |
|   GP2    | VGA_Out_Dark_Blue   |    1K    |        3          |
|   GP3    | VGA_Out_Light_Blue  |   470R   |        3          |
|   GP4    | VGA_Out_Dark_Green  |    1K    |        2          |
|   GP5    | VGA_Out_Light_Green |   470R   |        2          |
|   GP6    | VGA_Out_Dark_Red    |    1K    |        1          |
|   GP7    | VGA_Out_Light_Red   |   470R   |        1          |

 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these functions use the following pins:*

| Pico Pin | Function |
|  :---:   | :---     |
|   GP8    | UART_TX  |
|   GP9    | UART_RX  |
|   GP10   | IR_RX    |

*Also not shown in the above diagram is the abilty to use CSYNC instead of HSYNC & VSYNC by adding 16 to PALAVO_CONFIG. When using CSYNC VGA_Out_VSYNC (GP0) is not used and configured as an input.*

### Firmware

#### Using pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the appropriate `.uf2` file to the drive that appears on your PC:

For the Pico use [palavo_config0_on_pico.uf2](assets/palavo_config0_on_pico.uf2).  
For the Pico 2 use [palavo_config0_on_pico2.uf2](assets/palavo_config0_on_pico2.uf2).

Skip to [Testing Configuration 0](#testing-configuration-0).


#### Building the Firmware

There may be a simpler way to build Palavo's firmware, using Raspberry Pi's Pico Visual Studio Code Extension perhaps, but I haven't experimented with that enough yet (todo). This is how I currently build it on a Raspberry Pi 5 running Raspberry Pi OS:

Follow the instructions in Appendix C: Manual toolchain setup of [Getting started with Raspberry Pi Pico-series](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

Clone this repository (palavo) into a suitable location on your PC:

`$ git clone https://github.com/peterstansfeld/palavo.git`

Enter the `palavo` directory:

`$ cd palavo`

Create a `build` directory and enter it:

`$ mkdir build`

`$ cd build`

In the rest of this example the original Raspberry Pi Pico is used. If you'e using a Pico 2, replace any 'pico' references with 'pico2', and any 'rp2040' references with 'rp2350'. 

Create a suitably-named directory and enter it:

`$ mkdir pico`

`$ cd pico`

Create a suitably-named directory for this particular configuration of Palavo and enter it:

`$ mkdir config0`

`$ cd config0`


Specify where the pico-sdk directory can be found on your PC, e.g.:

`$ export PICO_SDK_PATH=~/pico/pico-sdk`

Run `cmake` specifying where the top level CMakeLists.txt file can be found - in this case it's the great-grandparent directory `../../../`, the board `pico`, and the configuration `PALAVO_CONFIG=0`:

`$ cmake ../../../ -DPICO_BOARD=pico -DPALAVO_CONFIG=0`

Then build it:

`$ make`

This should generate, among other files, a `palavo.uf2` and a`palavo.elf`.

To program the Pico with `palavo.uf2` put the Pico into BOOTSEL mode as described above. Alternatively, use the `picotool` utility:

`$ picotool load palavo.uf2 -f`

To program the Pico with `palavo.elf` use `openocd` and a [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html), or suitable alternative:

`$ openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "init; reset; program palavo.elf verify reset exit"`


### Testing Configuration 0

If all went well, when Palavo starts you should see something like the following screen on your VGA monitor:

![Palavo's start-up screen. A long description follows.](images/config0_on_pico.png "Palavo start-up screen")

*At the top of the screen, to the right of the palavo logo, are various adjustable settings. The first setting, which is highlighted, is the selected channel (0) followed by: the colour palette used to plot each of the captured channels (JJ - Jumper Jerky), the zoom level of the plots (1:1), the frequency divisor used when capturing (6), the base GPIO pin from which to capture (GP0), the number of pins to capture (8), the pin to use as a trigger pin (GP0), and the type of trigger used to start the capture (VSYNC). Below the settings and taking up most of the rest of the screen is a scrollable area filled with coloured plots of sections of each of the 8 captured channels, one below the other. Along each plot, if there is space for them, are the number of capture periods between transitions. Below this area is a minimap of the 8 channels, which is a condensed view of the whole of the captured channels scaled to fit the width of the screen. Just above the minimap is a small marker indicating which section of the minimap is being shown in the scrollable area above it. Below the minimap and at the bottom of the screen is a status bar. The left section of the status bar shows a little information, usually about the last key that was pressed; in this case it just reads "Press h for help." The right section of the status bar shows the current position of the main window "x: 0", its previous position "prev: 0", and the difference between the two "diff: 0".*

The channels captured in this screenshot are the GPIO pins used to generate the VGA signals which drive the VGA monitor: namely VSYNC, HSYNC, Dark Blue, Light Blue, Dark Green, Light Green, Dark Red and Light Red.


#### User Input

Open a serial terminal on your PC. I use `minicom` with this command line:

`$ minicom -b 115200 -w -D /dev/ttyACM0`

(You may need to change the device - the bit after the `-D`. After minicom opens you may also need to add carriage returns with 'Ctrl-A U'.)

Press the 'h' key and something like the following help screen should appear:

![Palavo's help screen, which lists various keyboard commands. A long description follows.](images/config0_on_pico_help.png "Palavo help screen")

*As described in the previous image except the coloured plots in the main section and in the minimap are now white, the information section of the status bar reads "104 help", and in the centre of the screen is a white filled rectangle onto which has been drawn the following black text:*


```
LEFT / RIGHT to scroll one sample period left / right
CTRL-LEFT / CTRL-RIGHT to scroll to previous / next edge
  on the selected channel (ch)
< / > to scroll to previous / next edge but one
PGUP / PGDN to scroll one page left / right
HOME / END to scroll to the beginning / end

TAB / SHIFT-TAB to select the next / previous setting
UP / DOWN to increase / decrease the selected setting
0..9 to set the selected numeric setting
c to capture a sample using the settings
z to zoom to fit all the samples on one page
+ / - / = to zoom in / out / to 1:1
h to show this help window
a to show the about window
S to start the screensaver
CTRL-P to upload the VGA framebuffer using xmodem

Press any key to close this window
```

Hopefully, the above instructions are clear enough to be able to get started using Palavo. If so, congratulations! I hope you find it interesting, and maybe even useful.

Note. After a period of inactivity (keyboard and infra-red) the VGA output will halt in order to allow the attached monitor to enter a power saving mode. Any keyboard or infra-red activity will restart the VGA output. This period of inactivity (5 minutes in the following example) can be configured by adding `-DVGA_TIMEOUT=300` to the appropriate `$ cmake ...` line, above. To prevent the VGA output from ever being halted, except by pressing 'S' (uppercase), add `-DVGA_TIMEOUT=0`.


## Configuration 1

#### PALAVO_CONFIG=1

"That's all well and good", I hear you say, "but can't the Pico 2 output DVI with its RP2350's HSTX peripheral?". Well, yes it can. Try this:

### Hardware

![A circuit diagram showing example hardware for Configuration 1. A long description follows.](images/config1-circuit.svg "Raspberry Pi Pico in Configuration 1")

*This is the same circuit as used in Configuration 0 with the addition of a [Pico DVI Sock](https://github.com/Wren6991/Pico-DVI-Sock). Originally designed by Raspberry Pi's Luke Wren, Adafruit now make their own version called the [DVI Sock for Pico](https://www.adafruit.com/product/5957). The DVI Sock connects the HSTX peripheral's pins on the RP2350 to an HDMI-shaped socket allowing a DVI monitor to be used instead of, or possibly as well as, a VGA monitor. The DVI Sock is designed to be soldered or connected to the underside of the Pico 2, but is shown here housed in the same breadboard as the Pico 2 and connected using jumper wires:*

#### Connecting a Pico 2 to a DVI Sock

| Pico 2 Pin | Pico Sock Label |
|   :---:    |     :---:       |
|    GP12    |     12/D0+      |
|    GP13    |     13/D0-      |
|    GND     |     GND         |
|    GP14    |     14/CK+      |
|    GP15    |     15/CK-      |
|    GP16    |     16/D2+      |
|    GP17    |     17/D2-      |
|    GND     |     GND         |
|    GP18    |     18/D1+      |
|    GP19    |     19/D1-      |

Adafruit make other products that could be used instead of the DVI Sock, such as their [DVI Breakout Board](https://www.adafruit.com/product/4984) or their [PiCowBell HSTX DVI Output for Pico](https://www.adafruit.com/product/6363). The PiCowBell, however, would need to have its SDA (GPIO4) and SCL (GPIO5) traces cut as they go to the Mini HDMI socket. I hope to offer another PALAVO_CONFIG option which will move the VGA Out RGB pins to GP6-GP11 so that the PiCowBell can be used without any modifications.

### Firmware

#### Using Pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the [palavo_config1_on_pico2.uf2](assets/palavo_config1_on_pico2.uf2) file to the drive that appears on your PC.

Skip to [Testing Configuration 1](#testing-configuration-1).


#### Building the Firmware

In the `build/` directory create a `pico2` directory, and enter it:

`$ mkdir pico2`

`$ cd pico2`

Create a suitably-named directory for this particular configuration of Palavo and enter it:

`$ mkdir config1`

`$ cd config1`

Repeat the rest of the previous build process, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=1`


### Testing Configuration 1

If all went well, when Palavo starts you should briefly the following test screen on your *DVI* monitor:

![Lots of vertical coloured bars of various colours.](images/dvi-test-screen.png "DVI test screen")

After that, you should see the same screen as you saw on the VGA monitor, which should look very similar to the screen in Configuration 0. If you use the 'tab' key to highlight the pins setting and change it from 8 to 20 with the 'up-arrow' key, and then press the 'c' key (for capture), you should see something like this:


![As described in the long description of the start-up screen image, above, except data from 20 channels are being displayed. A long description follows.](images/config1_on_pico2_pins_20.png "VGA and DVI logic traces")

*20 colourful channels of captured data. The first 8 channels are the relatively quiet VGA Out signals, the next 4 channels are absolutely silent unused GPIO pins, and the final 8 channels are the extremely busy DVI output signals, showing just how much more complex DVI is compared with VGA.*

Press the 'h' key and the same help window as Configuration 0 should appear with the addition of the 'v' item:

![As described in the long description of the help screen image, above, except there is an extra line regarding additional DVI modes. A long description follows.](images/config1_on_pico2_pins_20_help.png "Help screen with additional DVI modes")

*Note the extra line in the help window, which reads, `v to cycle DVI modes: mirror VGA out -> test -> VGA in`. In 'mirror VGA out' mode, whatever is displayed on VGA_Out is also displayed on DVI. In 'test' mode, a test screen is displayed on DVI. In 'mirror VGA in' mode, whatever is displayed on VGA_In is displayed on DVI.*


### Thoughts

I find it amazing that the RP2350 can output a DVI signal without too much trouble. However, the DVI frame buffer currently uses a lot of SRAM and this reduces the amount of signal data we can capture. Also this configuration doesn't leave many pins free to capture external signal data. We could lose the VGA output pins and gain a little SRAM by freeing up the VGA frame buffer, but if we had a second Pico 2...


## Configuration 21

#### PALAVO_CONFIG=21

If we have a second Pico 2, and change the VGA_Out\*s to VGA_In\*s we can make a 6-bit logic level VGA - DVI converter. We can also squeeze in a VGA_Out_CSYNC and a VGA_Out_RGB to provide a monochrome VGA output for testing purposes, which can also be mirrored to the DVI output.

![A circuit diagram showing example hardware for Configuration 21. A long description follows.](images/config21-circuit.svg "Raspberry Pi Pico in Configuration 21")

*A Raspberry Pi Pico 2 housed in a half-sized breadboard, connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)), and listing the following functions:*

| Pico 2 Pin | Function           |
|   :---:    | :---               |
|    GP0     | VGA_In_VSYNC       |
|    GP1     | VGA_In_HSYNC_CSYNC |
|    GND     | GND                |
|    GP2     | VGA_In_Dark_Blue   |
|    GP3     | VGA_In_Light_Blue  |
|    GP4     | VGA_In_Dark_Green  |
|    GP5     | VGA_In_Light_Green |
|    GP6     | VGA_In_Dark_Red    |
|    GP7     | VGA_In_Light_Red   |
|    GP10    | VGA_Out_CSYNC      |
|    GP11    | VGA_Out_RGB        |
 
 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG. When enabled these functions use the following pins:*

| Pico 2 Pin | Function |
|   :---:    | :---     |
|    GP8     | UART_TX  |
|    GP9     | UART_RX  |
|    GP28    | IR_RX    |

This Pico 2 can then sit on top of, or below, the Pico (or Pico 2) in Configuration 0 with *only* the connections we need, namely all the VGA_In pins (which connect to the VGA_Out pins), and the GNDs. N.B. If this Pico 2 (Configuration 21) is not being powered via its USB port, it can be powered by connecting its VSYS pin to the VSYS pin of the Pico (or Pico 2) in Configuration 0. N.B. Do NOT power both Picos with USB (or another source) if their VSYS pins are connected.


### Firmware

Note. Some DVI monitors don't support the 640x480 resolution at 72 Hz, which is the refresh rate that the Pico 2 uses to output DVI when operating at its default clock frequency of 150 MHz. If your monitor doesn't support 72 Hz, there's a good chance it will support 60 Hz, and this requires us to slow the Pico 2's clock frequency to 125 MHz.


#### Using Pre-built Firmware

Place the Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the appropriate `.uf2` file to the drive that appears on your PC:

For the 72 Hz refresh rate use [palavo_config21_on_pico.uf2](assets/palavo_config21_on_pico.uf2).  
For the 60 Hz refresh rate use [palavo_config21_at_125mhz_on_pico2.uf2](assets/palavo_config21_at_125mhz_on_pico2.uf2).

Skip to [Testing Configuration 21](#testing-configuration-21).


#### Building the Firmware

In the `build/pico2` directory create a different, but still suitably-named, directory, and enter it:

`$ mkdir config21`

`$ cd config21`

Repeat the rest of the build process as described in the Configuration 0 example, except use **one** of the following `cmake` commands:

For the 72 Hz refresh rate use:

`$ cmake ../../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=21`

**Or** for the 60 Hz refresh rate use:

`$ cmake ../../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=21 -DSYS_CLK_HZ=125000000`


### Testing Configuration 21

Something fun to do here is to get Hunter Adams' [VGA_Graphics_Primitives Demo](https://github.com/vha3/Hunter-Adams-RP2040-Demos/tree/master/VGA_Graphics/VGA_Graphics_Primitives) working on a Pico or a Pico 2, and then connect it to a Pico 2 in Configuration 21:

![A circuit diagram showing example hardware for all this fun. A long description follows.](images/config21-with-ha-vga-demo-circuit.svg "A Raspberry Pi Pico running Hunter Adams' VGA Demo connected to a Pico 2 in Configuration 21")

*A Raspberry Pi Pico or Pico 2 running Hunter Adams' VGA Graphics Primitives Demo, which housed in a half-sized breadboard. Nine of its pins are connected to corresponding pins on a Pico 2 in Configuration 21, which is housed in another half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Here are the connections between the Pico or Pico2 running the demo and the Pico 2:*

| Demo Pico or Pico 2 Pin | Function            | Pico 2 in Configuration 21 Pin | Function           |
|         :---:           | ---:                |              :---:             | :---               |
|          GP17           | VGA_Out_VSYNC       |               GP0              | VGA_In_VSYNC       |
|          GP16           | VGA_Out_HSYNC_CSYNC |               GP1              | VGA_In_HSYNC_CSYNC |
|          GND            | GND                 |               GND              | GND                |
|          GP20           | VGA_Out_Blue        |               GP2              | VGA_In_Dark_Blue   |
|          GP20           | VGA_Out_Blue        |               GP3              | VGA_In_Light_Blue  |
|          GP18           | VGA_Out_Dark_Green  |               GP4              | VGA_In_Dark_Green  |
|          GP19           | VGA_Out_Light_Green |               GP5              | VGA_In_Light_Green |
|          GP21           | VGA_Out_Red         |               GP6              | VGA_In_Dark_Red    |
|          GP21           | VGA_Out_Red         |               GP7              | VGA_In_Light_Red   |

### Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the appropriate `.uf2` file to the drive that appears on your PC:

For the Pico use [vga_demo_on_pico.uf2](assets/vga_demo_on_pico.uf2).  
For the Pico 2 use  [vga_demo_on_pico2.uf2](assets/vga_demo_on_pico2.uf2).


Start both devices and hopefully you should see Hunter's demo on your DVI monitor:

![A frame from Hunter Adams' VGA demo. A long description follows.](images/ha-vga-demo-paused.png "Hunter Adams' VGA demo")

*A frame of an animated demo of various graphical primitives drawn in 15 colours on a black background. The primitives are filled rectangles, filled circles, unfilled rectangles, unfilled squares, horizontal lines, vertical lines, and diagonal lines. There are four lines of small text in the left of three filled rectangles at the top of the screen, and they read "Raspberry Pi Pico Graphics Primitives demo", "Hunter Adams", "vha@cornell.edu" and "4-bit mod by Bruce Land". The middle filled rectangle has two lines of large text, which read "Time Elapsed:" and "189". When animated this number increments every second, and below the three filled rectangles the rest of the primitives are being redrawn in different colours, sizes and/or locations every 20 milliseconds.*

I said it was fun.

If you've been wondering why Palavo uses 6-bit colour (RRGGBB), it's because when converting the VGA output to DVI, using the HSTX peripheral on the RP2350, the colours remain the same as the VGA output. This is due to each colour being made up of the same number of bits, which is not the case with Hunter's and Bruce's 4-bit colour (RGGB). To save SRAM used by Palavo's VGA driver, each horizontal line consists of a maximum of two 6-bit colours.


## Configuration 2

#### PALAVO_CONFIG=2

The trouble with [Configuration 0](#configuration-0) is that we're using quite a few GPIO pins for the VGA_Out signals, and optionally for UART_TX, UART_RX, and IR_RX. What if we wanted to capture 24 external inputs? We can't with a Pico or Pico 2. But we can with a board that uses the B variant of the RP2350. The RP2350B has 48 GPIO pins, and we only need 7 or 8 for VGA_Out, or 8 for DVI_Out. The slight incovenience with DVI_Out (using HSTX) is that it's fixed on pins GP12-GP19, whereas with VGA_Out we can put its 7 or 8 outputs on whichever pins we like. Allow me introduce you to the [Pimoroni Pico LiPo 2 XL W](https://shop.pimoroni.com/products/pimoroni-pico-lipo-2-xl-w):

### Hardware

![A Pimoroni Pico Lipo 2XL W connected to a Pi Pico 2 (in Configuration 21). A long description follows.](images/config2-circuit.svg "A Pimoroni Pico Lipo 2XL W connected to a Pi Pico 2 (in Configuration 21).")

*A Pimoroni Pico Lipo 2XL W occupying the whole length of a half-sized breadboard. Eight of its pins are connected to corresponding pins on a Pico 2 (in Configuration 21), which is housed in another half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Here are the connections between the Pico Lipo and the Pico 2:*

| Pico Lipo 2XL W Pin | Function            | Pico 2 Pin | Function           |
|        :---:        | ---:                |   :---:    | :---               |
|         GP31        | VGA_Out_CSYNC       |    GP1     | VGA_In_HSYNC_CSYNC |
|         GND         | GND                 |    GND     | GND                |
|         GP32        | VGA_Out_Dark_Blue   |    GP2     | VGA_In_Dark_Blue   |
|         GP33        | VGA_Out_Light_Blue  |    GP3     | VGA_In_Light_Blue  |
|         GP34        | VGA_Out_Dark_Green  |    GP4     | VGA_In_Dark_Green  |
|         GP35        | VGA_Out_Light_Green |    GP5     | VGA_In_Light_Green |
|         GP36        | VGA_Out_Dark_Red    |    GP6     | VGA_In_Dark_Red    |
|         GP37        | VGA_Out_Light_Red   |    GP7     | VGA_In_Light_Red   |

 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these functions use the following pins:*

| Pico Lipo 2XL W Pin | Function |
|       :---:         | :---     |
|        GP38         | UART_TX  |
|        GP39         | UART_RX  |
|        GP46         | IR_RX    |

The Pico Lipo 2XL W is *so* long. And just *look* at all those free GPIO pins - enough to capture the signals from a keyboard's switch matrix, perhaps? It's pictured here attached, mostly, to a [Pimoroni Pico Omnibus](https://shop.pimoroni.com/products/pico-omnibus) with 24 signals from a keyboard switch matrix connected to GP0 to GP26. 


*TODO*  
![A Pimoroni PICO LIPO 2XL W attached - well, 66.67% attached - to a Pimoroni Pico Omnibus.](image.jpg)


### Firmware

#### Using Pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy [palavo_config2_on_pimoroni_pico_lipo2xl_w.uf2](assets/palavo_config2_on_pimoroni_pico_lipo2xl_w.uf2) to the drive that appears on your PC.

Skip to [Testing Configuration 2](#testing-configuration-2).


#### Building the Firmware

In the `build` directory create a suitably-named directory, and enter that directory:

`$ mkdir pimoroni_pico_lipo2xl_w`

`$ cd pimoroni_pico_lipo2xl_w`

Create a suitably-named directory for this particular configuration of Palavo and enter that directory:

`$ mkdir config2`

`$ cd config2`

Repeat the rest of the build process as described in the Configuration 0 example, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=pimoroni_pico_lipo2xl_w_rp2350 -DPALAVO_CONFIG=2`


### Testing Configuration 2

The screen on the DVI monitor should look the same as it does in Configuration 1, except that the 'base' and 'trig.' settings can be set to use GP0 to GP47 (rather than just GP0 to GP31).


## Configuration 40

#### PALAVO_CONFIG=40

What if we wanted to capture 32 contiguous channels? Unfortunately, it's not possible with the Pimoroni Pico LiPo 2 XL W because some GPIO pins are not broken out, and others are used for the on-board PSRAM and others are used for the wireless module. The answer is to use something that breaks out every GPIO pin, and the only boards which do that, that I know of, are the [Solder Party RP2350 Stamp XL](https://www.solder.party/docs/rp2350-stamp-xl/) and the [Pimoroni PGA2350](https://shop.pimoroni.com/products/pga2350). Here's the Stamp XL housed in a [Solder Party RP2xxx Stamp Carrier Basic](https://www.solder.party/docs/rp2xxx-stamp-carrier-basic/):


### Hardware

![A Solder Party RP2350 Stamp XL connected to a Pi Pico 2 in Configuration 21. A long description follows.](images/config8-circuit.svg "A Solder Party RP2350 Stamp XL connected to a Pi Pico 2 in Configuration 21.")

*A Solder Party RP2350 Stamp XL housed in a Solder Party RP2xxx Stamp Carrier Basic breakout board. Nine of its pins are connected to corresponding pins on a Pico 2 (in Configuration 21), which is housed in a half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Another pin, labelled IR_RX, is configured as an infra-red receiver input. Here are the connections between the Stamp and the Pico 2:*

| RP2xxx Stamp Carrier Basic Pin | Function            | Pico 2 Pin | Function           |
|        :---:                   | ---:                |   :---:    | :---               |
|         GP0                    | VGA_Out_VSYNC       |    GP0     | VGA_In_VSYNC       |
|         GP1                    | VGA_Out_CSYNC       |    GP1     | VGA_In_HSYNC_CSYNC |
|         GND                    | GND                 |    GND     | GND                |
|         GP2                    | VGA_Out_Dark_Blue   |    GP2     | VGA_In_Dark_Blue   |
|         GP3                    | VGA_Out_Light_Blue  |    GP3     | VGA_In_Light_Blue  |
|         GP4                    | VGA_Out_Dark_Green  |    GP4     | VGA_In_Dark_Green  |
|         GP5                    | VGA_Out_Light_Green |    GP5     | VGA_In_Light_Green |
|         GP6                    | VGA_Out_Dark_Red    |    GP6     | VGA_In_Dark_Red    |
|         GP7                    | VGA_Out_Light_Red   |    GP7     | VGA_In_Light_Red   |

*And here are the UART pins and the infra-red receive pin, which have been enabled by adding 32 and 8, ie 40, to PALAVO_CONFIG.*
 
| RP2xxx Stamp Carrier Basic Pin | Function            |
|        :---:                   | ---:                |
|         GP8                    | UART_TX             |
|         GP9                    | UART_RX             |
|         GP10                   | IR_RX               |


### Firmware

#### Using Pre-built Firmware

Place the RP2350 Stamp XL in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy [palavo_config40_on_solderparty_rp2350_stamp_xl.uf2](assets/palavo_config40_on_solderparty_rp2350_stamp_xl.uf2) to the drive that appears on your PC.

Skip to [Testing Configuration 40](#testing-configuration-40).


#### Building the Firmware

In the `build` directory create a suitably-named directory, and enter it:

`$ mkdir solderparty_rp2350_stamp_xl`

`$ cd solderparty_rp2350_stamp_xl`

Create a suitably-named directory for this particular configuration of Palavo and enter it:

`$ mkdir config40`

`$ cd config40`

Repeat the rest of the build process as described in the Configuration 0 example, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=solderparty_rp2350_stamp_xl -DPALAVO_CONFIG=40`


### Testing Configuration 40

The screen on the DVI monitor should look the more or less the same as it does in Configuration 1, except that the 'base' and 'trig.' settings can be configured to any value from GP0 to GP47 (rather than from GP0 to GP31).

As the UART has been enabled we can control Palavo with any 3.3V logic level UART serial port, such as a suitable USB to UART serial port adapter. I use [Raspberry Pi's Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) as it can also be used to program and debug the RP2xxx devices via their Debug interface.

Alternatively the UART_RX pin can be connected to a keyboard-to-serial-terminal adapter, which is essentially a Pico or Pico 2 with its USB port in host mode, and which converts keyboard input to UART serial output. Details of the adapter can be found in this [keybuart repository](https://github.com/peterstansfeld/keybuart.git).

The infra-red receive pin (IR_RX), along with connections to 3.3V and GND, can be connected to an infra-red receiver, e.g. this [Grove IR Receiver](https://thepihut.com/products/grove-infrared-receiver). Palavo accepts commands transmitted from this [Argon IR Remote control](https://argon40.com/products/argon-remote). This is currently an experimental feature and is limited in fuction, but it's a bit of fun.

That's it for the example configurations with pre-built firmware. If you want to build other configurations it's helpful to understand PALAVO_CONFIG. 

### PALAVO_CONFIG

When we add `-DPALAVO_CONFIG=[number]` to a `$ cmake ...` command CMake passes this definition to the c compiler, which has the same effect as if we'd #define'd it in the source code, like this:

`#define PALAVO_CONFIG [number]`

Depending on the value of each bit in the \[number\], Palavo will have the following features enabled, disabled, or configured:

|  Bit  | Value (decimal) | #define           | Descrition                                                            |
| :---: |     :---:       | :---              | :---                                                                  |
|   0   |       1         | USE_DVI           | Display either VGA In, VGA Out, or a test screen to DVI.*             |
|   1   |       2         | USE_GPIO_31_47    | Use GPIO 31 to 37 instead of GPIO 0 to 7 for VGA Out.**               |
|   2   |       4         | USE_VGA_IN_TO_DVI | Display VGA In (on start-up). Needs USE_DVI to be set to make sense.* |
|   3   |       8         | USE_IR            | Enable infra-red remote control.                                      |
|   4   |      16         | USE_CSYNC         | Use CSYNC instead of VSYNC and HSYNC for VGA Out.***                  |
|   5   |      32         | USE_UART          | Use UART as well as USB for STDIO comms.                              |
 
\* Only relevant for the RP2350.  
\*\* Only relevant for the RP2350B.  
\*\*\* Automatically set if USE_GPIO_31_47 is set.  


### Tips and Tricks

#### Picotool

Besides flashing a Pico or Pico2 or other RP2xxx-equipped device, Raspberry Pi's `picotool` utility is very useful for finding out what's on the device, or what's in a `.uf2` or `.elf` file before flashing it to a device.

To find out what's on the device place it in BOOTSEL mode and enter:

`$ picotool info -a`

To find out what's in a firmware file (e.g. palavo.uf2) enter:

`$ picotool info -a palavo.uf2`

<hr>

#### Useful script files

When in a build/device/config directory, instead of remembering the `openocd` command each time, you can copy the script `make-and-flash.sh` from the palavo directory:

`$ cp ../../../make-and-flash.sh .`

Using a text editor open `make-and-flash.sh`, modify the `adapter_serial_no` variable to that of your Debug Probe. If you don't want or need to specify a serial number (you only have one Debug Probe connected), blank the `target_adapter_cmnd` variable.

Then `make` the firmware and flash the RP2040:

`$ ./make-and-flash.sh`


### End of Document

<hr>

### Notes to self

Tags, and GitHub Releases and Assets and Licenses

Example
```
tag
    asset
    asset

palavo-v1.0.6
    palavo_config0_on_pico.uf2
    palavo_config0_on_pico2.uf2
    palavo_config1_on_pico2.uf2
    palavo_config21_on_pico2.uf2
    palavo_config21_at_125mhz_on_pico2.uf2
    palavo_config2_on_pimoroni_pico_lipo2xl_w_rp2350.uf2
    palavo_config8_on_solderparty_rp2350_stamp_xl.uf2
```

1. Use `$ ./make-assets.sh` to `make` all the relevant `palavo.uf2` files, rename them, and copy them to the `assets/` directory.

2. Use `$ ls assets/ -xt | tac` to list the assets in the chronological order in which they were created.

<hr>

 To make a script file (e.g. `filename.sh`) executable, enter:

`chmod +x filename.sh`

<hr>

To build and install `pioasm` so that it's not downloaded and built on each `make` of a new pico SDK project build folder, I followed [these instructions](https://forums.raspberrypi.com/viewtopic.php?p=2329581&hilit=pioasm+keeps+building#p2329581):

```
cd ~/pico/pico-sdk/tools/pioasm
mkdir build
cd build
cmake -DPIOASM_VERSION_STRING="2.2.0" ..
make
sudo make install
sudo ln -s ~/pico/pico-sdk/tools/pioasm/build/pioasm /usr/local/bin/pioasm
```
Although, the last instruction reported this error:

`ln: failed to create symbolic link '/usr/local/bin/pioasm': File exists`

Thanks, hippy (the person who shared the instructions).

<hr>
