# palavo


## PIO-Accomplished Logic Analyser with VGA Output

TODO  
![A Raspberry Pi Pico displaying some logic on a VGA monitor. A long description follows.](config0-on-pico-photo.png "Raspberry Pi Pico displaying some logic on a VGA monitor.")


Palavo uses the PIO (Programmable Input Output) feature of Raspberry Pi's RP2040 or RP2350 microcontroller to capture the state of its GPIO pins over time, and then uses PIO to display those captured states on a VGA monitor. Calling Palavo a logic analyser is a bit of a stretch, as it does very little analysis, but it does allow the user, via a simple interface, to analyse the logic themselves. The interface is controlled using a serial terminal (on a PC), a keyboard to serial terminal adapter, and/or an infra-red remote control. The user can specify which GPIO pins to capture, the frequency at which they should be captured, which GPIO pin should be used to trigger the capture, and what type of trigger should be used.

When using the RP2350, Palavo can be configured to mirror its VGA output to DVI. I thought about changing the project name to Palavocado, but decided against it - mainly because I couldn't come up with anything for the 'c' to stand for, and because I wanted to be taken at least a little seriously. Also, I believe [palavo](https://en.wiktionary.org/wiki/palavo) means 'I shovelled' in Italian, and shovelling is very much what I felt I was doing when working on the source code.

The project was inspired by, and uses code from, Raspberry Pi's [Logic Analyser (Pico SDK) Example](https://github.com/raspberrypi/pico-examples/tree/master/pio/logic_analyser), their [DVI Out HSTX Encoder example for the Pico 2](https://github.com/raspberrypi/pico-examples/tree/master/hstx/dvi_out_hstx_encoder), and Hunter Adams' [PIO-Based VGA Graphics Driver for RP2040](https://github.com/vha3/Hunter-Adams-RP2040-Demos/blob/master/VGA_Graphics/README.md).

The VGA output uses a resolution of 640 x 480 with 6-bit colour (2 red, 2 green, 2 blue). It can use either HSYNC (horizontal sync) and VSYNC (vertical sync), or CSYNC (combined sync). Not all VGA monitors support CSYNC, but many do. Some details about CSYNC can be found on this [HDRetrovision blog post](https://www.hdretrovision.com/blog/2019/10/10/engineering-csync-part-2-falling-short).


## How to build Palavo

There are a number of configurations for Palavo, which can sample any of the 32 GPIOs of the RP2040/RP2350A, or the 48 GPIOs of the RP2350B. Pre-built binaries (`.uf2` files) are available to get you up and running without the need to compile any firmware.  

If you do plan to build your own firmware, configurations are defined by adding the appropriate `PALAVO_CONFIG` variable to the `cmake` command line that's used to create a build directory, in which the firmware can then be built. Instructions for doing this are detailed for each of the configurations.


## Configuration 0

(PALAVO_CONFIG=0)


### Hardware

At its simplest, a [Raspberry Pi Pico or Pico 2](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html) and a few components can be used:

![A circuit diagram showing example hardware for Configuration 0. A long description follows.](images/config0-circuit.svg "Raspberry Pi Pico in Configuration 0")

*A Raspberry Pi Pico (or Pico 2) housed in a half-sized breadboard. Nine of the Pico's pins are connected to one side of various resistors, which are housed in a mini breadboard. The other side of the resistors are connected to the pins of VGA socket (or the plug on one end of a VGA cable). Here are the connections and resistor values:*

| Pico Pin | Palavo Description  | Resistor | VGA Socket Pin No |
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

 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these options use the following pins:*

| Pico Pin | Palavo Description |
|  :---:   | :---               |
|   GP8    | UART_TX            |
|   GP9    | UART_RX            |
|   GP10   | IR_RX              |

*Also not shown in the above diagram is the abilty to use CSYNC instead of HSYNC & VSYNC by adding 16 to PALAVO_CONFIG. When using CSYNC VGA_Out_VSYNC (GP0) is not used and configured as an input.*

### Firmware

#### Using pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the appropriate `.uf2` file to the drive that appears on your PC:

For the Pico use [palavo_config0_on_pico.uf2](assets/palavo_config0_on_pico.uf2).  
For the Pico 2 use  [palavo_config0_on_pico2.uf2](assets/palavo_config0_on_pico2.uf2).

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

To program the RP2040 on the Pico, or the RP2350 on the Pico 2, with `palavo.uf2` put the Pico into BOOTSEL as described above. Alternatively, use the `picotool` utility:

`$ picotool load palavo.uf2 -f`

To program the RP2040 on the Pico with `palavo.elf` use `openocd` and a [Raspberry Pi Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html), or suitable alternative:

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

*As described in the previous image except the coloured plots in the main section and in the minimap are now white, the information section of the status reads "104 help", and in the centre of the screen is a white filled rectangle onto which has been drawn the following black text:*


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

Note. After a period of inactivity (keyboard or infra-red) the VGA output will halt in order to allow the attached monitor to enter a power saving mode. Any keyboard or infra-red activity will restart the VGA output. This period of inactivity (5 minutes in the following example) can be configured by adding `-DVGA_TIMEOUT=300` to the appropriate `$ cmake ...` line, above. To prevent the VGA output from ever being halted, except by pressing 'S' (uppercase), add `-DVGA_TIMEOUT=0`. 


The below 3 paragraphs need work or moving - todo

If using UART for serial comms, Serial_RX and Serial_TX can be connected to a PC via a 3.3V logic level UART to USB adapter. I use [Raspberry Pi's Debug Probe](https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html) as it can also be used to program and debug the Pico via its Debug interface.

Alternatively the Serial_RX and Serial_TX can be connected to a keyboard-to-serial-terminal adapter, which is essentially a Pico or Pico 2 with its USB port in host mode, and which converts keyboard input to serial (UART) output. Details of the adapter can be found in this [keybuart repository](https://github.com/peterstansfeld/keybuart.git).

The Infra_Red_RX pin, along with connections to 3.3V and GND, can be connected to an infra-red receiver, e.g. this [Grove IR Receiver](https://thepihut.com/products/grove-infrared-receiver), and Palavo accepts a commands transmitted from this [Argon IR Remote control](https://argon40.com/products/argon-remote).



## Configuration 1

#### PALAVO_CONFIG=1

"That's all well and good", I hear you say, "but can't the Pico 2 output DVI with its RP2350's HSTX peripheral?". Well, yes it can. Try this:

### Hardware

![A circuit diagram showing example hardware for Configuration 1. A long description follows.](images/config1-circuit.svg "Raspberry Pi Pico in Configuration 1")

*This is the same circuit as used in Configuration 0 with the addition of a [Pico DVI Sock](https://github.com/Wren6991/Pico-DVI-Sock). Originally designed by Raspberry Pi's Luke Wren, Adafruit now make their own version called the [DVI Sock for Pico](https://www.adafruit.com/product/5957). The DVI Sock connects the HSTX peripheral pins on the RP2350 to an HDMI-shaped socket allowing a DVI monitor to be used instead of, or as well as, a VGA monitor. The DVI Sock is designed to be soldered or connected to the underside of the Pico 2, but is shown here housed in the same breadboard as the Pico 2 and connected using jumper wires:*

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

I find it amazing that the RP2350 can output a DVI signal without too much trouble. However, the DVI frame buffer currently uses a lot of SRAM and this reduces the amount of signal data we can capture. Also this configuration doesn't leave many pins (between 7 and 11) free to capture external signal data. We could lose the VGA output pins and gain a little SRAM by freeing up the VGA frame buffer, but if we had a second Pico 2...


## Configuration 21

#### PALAVO_CONFIG=21

If we have a second Pico 2, and change the VGA_Out\*s to VGA_In\*s we can make a 6-bit logic level VGA - DVI Converter. We can also squeeze in a VGA_Out_CSYNC and a VGA_Out_RGB to provide a monochrome VGA output for testing purposes, which can also be mirrored to the DVI output.

![A circuit diagram showing example hardware for Configuration 21. A long description follows.](images/config21-circuit.svg "Raspberry Pi Pico in Configuration 21")

*A Raspberry Pi Pico 2 housed in a half-sized breadboard, connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)), and listing the following functions:*

| Pico 2 Pin | Palavo Function    | Pico Sock Pin |
|   :---:    | :---               |     :---:     |
|    GP0     | VGA_In_VSYNC       |               |
|    GP1     | VGA_In_HSYNC_CSYNC |               |
|    GND     | GND                |               |
|    GP2     | VGA_In_Dark_Blue   |               |
|    GP3     | VGA_In_Light_Blue  |               |
|    GP4     | VGA_In_Dark_Green  |               |
|    GP5     | VGA_In_Light_Green |               |
|    GP6     | VGA_In_Dark_Red    |               |
|    GP7     | VGA_In_Light_Red   |               |
|    GP10    | VGA_Out_CSYNC      |               |
|    GP11    | VGA_Out_RGB        |               |
 
 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these options use the following pins:*

| Pico 2 Pin | Palavo Function |
|   :---:    | :---            |
|    GP8     | UART_TX         |
|    GP9     | UART_RX         |
|    GP28    | IR_RX           |

This Pico 2 can then sit on top of, or below, the Pico (or Pico 2) in Configuration 0 with *only* the connections we need, namely all the VGA_In pins (which connect to the VGA_Out pins), and the GNDs. N.B. If this Pico 2 (Configuration 21) is not being powered via its USB port, it can be powered by connecting its VSYS pin to the VSYS pin of the Pico (or Pico 2) in Configuration 0. N.B. Do NOT power both Picos with USB (or another source) if their VSYS pins are connected.


### Firmware

#### Using Pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy the [palavo_config21_on_pico.uf2](assets/palavo_config21_on_pico2.uf2) file to the drive that appears on your PC:

Skip to [Testing Configuration 21](#testing-configuration-21).


#### Building the Firmware

In the `build/pico2` directory create a different, but still suitably-named, directory, and enter it:

`$ mkdir config21`

`$ cd config21`

Repeat the rest of the previous build process, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=pico2 -DPALAVO_CONFIG=21`


### Testing Configuration 21

Something fun to do here is get Hunter Adams' [VGA_Graphics_Primitives Demo](https://github.com/vha3/Hunter-Adams-RP2040-Demos/tree/master/VGA_Graphics/VGA_Graphics_Primitives) working on a Pico or a Pico 2, and then connect it to a Pico 2 running Configuration 21:

![A circuit diagram showing example hardware for all this fun. A long description follows.](images/config21-with-ha-vga-demo-circuit.svg "A Raspberry Pi Pico running Hunter Adams' VGA Demo connected to a Pico 2 in Configuration 21")


*A Raspberry Pi Pico or Pico 2 (running Hunter Adams' VGA Graphics Primitives Demo) housed in a half-sized breadboard. Nine of its pins are connected to corresponding pins on a Pico 2 (in Configuration 21), which is housed in another half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Here are the additional connections:*

| Pico or Pico 2 Pin | VGA Demo Function   | Palavo (Configuration 21) Function | Pico 2 Pin |
|       :---:        | ---:                | :---                               |   :---:    |
|        GP17        | VGA_Out_VSYNC       | VGA_In_VSYNC                       |    GP0     |
|        GP16        | VGA_Out_HSYNC_CSYNC | VGA_In_HSYNC_CSYNC                 |    GP1     |
|        GND         | GND                 | GND                                |    GND     |
|        GP20        | VGA_Out_Blue        | VGA_In_Dark_Blue                   |    GP2     |
|        GP20        | VGA_Out_Blue        | VGA_In_Light_Blue                  |    GP3     |
|        GP18        | VGA_Out_Dark_Green  | VGA_In_Dark_Green                  |    GP4     |
|        GP19        | VGA_Out_Light_Green | VGA_In_Light_Green                 |    GP5     |
|        GP21        | VGA_Out_Red         | VGA_In_Dark_Red                    |    GP6     |
|        GP21        | VGA_Out_Red         | VGA_In_Light_Red                   |    GP7     |

Start both devices and hopefully you should see Hunter's demo appearing on your DVI monitor...

![A frame from Hunter Adams' VGA demo. A long description follows.](images/ha-vga-demo-paused.png "Hunter Adams' VGA demo")

*A frame of an animated demo of various graphical primitives drawn in 15 colours on a black background. The primitives are filled rectangles, filled circles, unfilled circles, unfilled squares, horizontal lines, vertical lines, and diagonal lines. There are four lines of small text in the left of three filled rectangles at the top of the screen, and they read "Raspberry Pi Pico Graphics Primitives demo", "Hunter Adams", "vha@cornell.edu" and "4-bit mod by Bruce Land". The middle filled rectangle has two lines of large text, which read "Time Elapsed:" and "189". When animated this number increments every second, and below the three filled rectangles the rest of the primitives are being redrawn in different colours, sizes and/or locations every 20 milliseconds.*

I said it was fun.

If you've been wondering why Palavo uses 6-bit colour (RRGGBB), it's because when converting the VGA output to DVI, using the HSTX peripheral on the RP2350, the colours remain the same as the VGA output. This is due to each colour being made up of the same number of bits, which is not the case with Hunter's and Bruce's 4-bit colour (RGGB). To save SRAM used by Palavo's VGA driver, each horizontal line consists of a maximum of two 6-bit colours.


## Configuration 2

#### PALAVO_CONFIG=2

The trouble with [Configuration 0](#configuration-0) is that we're using quite a few GPIO pins for the VGA_Out signals, and optionally for Serial_TX, Serial_RX, and Infra_Red_RX. What if we wanted to capture 24 external inputs? We can't with a Pico or Pico 2. But we can with a board that uses the B variant of the RP2350. The RP2350B has 48 GPIO pins, and we only need 7 or 8 for VGA_Out, or 8 for DVI_Out. The slight incovenience with DVI_Out (using HSTX) is that it's fixed on pins GP12-GP19, whereas with VGA_Out we can put its 7 or 8 outputs on whichever pins we like. Allow me introduce you to the [Pimoroni Pico LiPo 2 XL W](https://shop.pimoroni.com/products/pimoroni-pico-lipo-2-xl-w):

### Hardware

![A Pimoroni Pico Lipo 2XL W connected to a Pi Pico 2 (in Configuration 21). A long description follows.](images/config2-circuit.svg "A Pimoroni Pico Lipo 2XL W connected to a Pi Pico 2 (in Configuration 21).")

*A Pimoroni Pico Lipo 2XL W occupying the whole length of a half-sized breadboard. Eight of its pins are connected to corresponding pins on a Pico 2 (running Configuration 21), which is housed in another half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Here are the connections:*

| Pico Lipo 2XL W Pin | Pico Lipo 2XL W Function | Palavo (Configuration 21) Function | Pico 2 Pin |
|        :---:        | ---:                     | :---                               |   :---:    |
|         GP31        | VGA_Out_CSYNC            | VGA_In_HSYNC_CSYNC                 |    GP1     |
|         GND         | GND                      | GND                                |    GND     |
|         GP32        | VGA_Out_Dark_Blue        | VGA_In_Dark_Blue                   |    GP2     |
|         GP33        | VGA_Out_Light_Blue       | VGA_In_Light_Blue                  |    GP3     |
|         GP34        | VGA_Out_Dark_Green       | VGA_In_Dark_Green                  |    GP4     |
|         GP35        | VGA_Out_Light_Green      | VGA_In_Light_Green                 |    GP5     |
|         GP36        | VGA_Out_Dark_Red         | VGA_In_Dark_Red                    |    GP6     |
|         GP37        | VGA_Out_Light_Red        | VGA_In_Light_Red                   |    GP7     |

 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these options use the following pins:*

| Pico 2 Pin | Palavo Function |
|   :---:    | :---            |
|    GP38    | UART_TX         |
|    GP39    | UART_RX         |
|    GP46    | IR_RX           |

It's *so* long. And just *look* at all those free GPIO pins - enough to capture the signals from a keyboard's switch matrix, perhaps? It's pictured here attached, mostly, to a [Pimoroni Pico Omnibus](https://shop.pimoroni.com/products/pico-omnibus) with 24 signals from a keyboard switch matrix connected to GP0 to GP26. 


*TODO*  
![A Pimoroni PICO LIPO 2XL W attached - well, 66.67% attached - to a Pimoroni Pico Omnibus.](image.jpg)


### Firmware

#### Using Pre-built Firmware

Place the Pico or Pico 2 in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy [palavo_config2_on_pimoroni_pico_lipo2xl_w.uf2](assets/palavo_config2_on_pimoroni_pico_lipo2xl_w.uf2) to the drive that appears on your PC:

Skip to [Testing Configuration 2](#testing-configuration-2).


#### Building the Firmware

In the `build` directory create a suitably-named directory, and enter that directory:

`$ mkdir pimoroni_pico_lipo2xl_w`

`$ cd pimoroni_pico_lipo2xl_w`

Create a suitably-named directory for this particular configuration of Palavo and enter that directory:

`$ mkdir config2`

`$ cd config2`


Repeat the rest of the previous build process, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=pimoroni_pico_lipo2xl_w_rp2350 -DPALAVO_CONFIG=2`


### Testing Configuration 2

The screen on the VGA monitor should look the same as it does in Configuration 1, except that the 'base' and 'trig.' settings can be set to use GP0 to GP47 (rather than just GP0 to GP31).


## Configuration 8

#### PALAVO_CONFIG=8

What if we wanted to capture 32 contiguous channels? Unfortunately, it's not possible with the Pimoroni Pico LiPo 2 XL W because some GPIO pins are not broken out, and others are used for the on-board PSRAM and others are used for the wireless module. The answer is to use something that breaks out every GPIO pin, and the only boards which do that, that I know of, are the [Solder Party RP2350 Stamp XL](https://www.solder.party/docs/rp2350-stamp-xl/) and the [Pimoroni PGA2350](https://shop.pimoroni.com/products/pga2350). Here's the Stamp XL housed in a [Solder Party RP2xxx Stamp Carrier Basic](https://www.solder.party/docs/rp2xxx-stamp-carrier-basic/):


### Hardware

![A Solder Party RP2350 Stamp XL connected to a Pi Pico 2 in Configuration 21. A long description follows.](images/config8-circuit.svg "A Solder Party RP2350 Stamp XL connected to a Pi Pico 2 in Configuration 21.")

*A Solder Party RP2350 Stamp XL housed in a Solder Party RP2xxx Stamp Carrier Basic breakout board. Nine of its pins are connected to corresponding pins on a Pico 2 (running Configuration 21), which is housed in a half-sized breadboard and connected to a DVI Sock ([as described in Configuration 1](#connecting-a-pico-2-to-a-dvi-sock)). Here are the connections:*

| RP2xxx Stamp Carrier Basic Pin | Palavo (Configuration 8) Function | Palavo (Configuration 21) Function | Pico 2 Pin |
|        :---:                   | ---:                              | :---                               |   :---:    |
|         GP31                   | VGA_Out_CSYNC                     | VGA_In_HSYNC_CSYNC                 |    GP1     |
|         GND                    | GND                               | GND                                |    GND     |
|         GP32                   | VGA_Out_Dark_Blue                 | VGA_In_Dark_Blue                   |    GP2     |
|         GP33                   | VGA_Out_Light_Blue                | VGA_In_Light_Blue                  |    GP3     |
|         GP34                   | VGA_Out_Dark_Green                | VGA_In_Dark_Green                  |    GP4     |
|         GP35                   | VGA_Out_Light_Green               | VGA_In_Light_Green                 |    GP5     |
|         GP36                   | VGA_Out_Dark_Red                  | VGA_In_Dark_Red                    |    GP6     |
|         GP37                   | VGA_Out_Light_Red                 | VGA_In_Light_Red                   |    GP7     |

 *Not shown in the above diagram is the option to enable infra-red reception by adding 8 to PALAVO_CONFIG (when creating a build directory), and another option to enable UART comms by adding 32 to PALAVO_CONFIG . When enabled these options use the following pins:*

| RP2xxx Stamp Carrier Basic Pin | Palavo Function |
|             :---:              | :---            |
|              GP8               | UART_TX         |
|              GP9               | UART_RX         |
|              GP10              | IR_RX           |


\* UART serial comms can be used in addition to USB serial comms - add 32 to PALAVO_CONFIG  
\** To enable IR remote control add 8 to PALAVO_CONFIG  

As you can see the GPIOs GP8 to GP47 (40 GPIOs in total) are free to use and are conveniently and contiguously located.


### Firmware

#### Using Pre-built Firmware

Place the RP2350 Stamp XL in BOOTSEL mode (hold the BOOTSEL button down during board power-up) and copy [palavo_config8_on_solderparty_rp2350_stamp_xl.uf2](assets/palavo_config2_on_solderparty_rp2350_stamp_xl.uf2) to the drive that appears on your PC:

Skip to [Testing Configuration 8](#testing-configuration-8).


#### Building the Firmware


In the `build` directory create a suitably-named directory, and enter it:

`$ mkdir solderparty_rp2350_stamp_xl`

`$ cd solderparty_rp2350_stamp_xl`

Create a suitably-named directory for this particular configuration of Palavo and enter it:

`$ mkdir config8`

`$ cd config8`


Repeat the rest of the previous build process, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=solderparty_rp2350_stamp_xl -DPALAVO_CONFIG=8`


### Testing Configuration 8

The screen on the VGA monitor should look the same as it does in Configuration 1, except that the 'base' and 'trig.' settings can be set to use GP0 to GP47 (rather than GP0 to GP31).


### PALAVO_CONFIG

When we add `-DPALAVO_CONFIG=[number]` to a `$ cmake ...` command, CMake passes this definition to the c compiler, which has the same effect as if we'd defined it in the source code:

`#define PALAVO_CONFIG [number]`

Depending on the value of \[number\], Palavo will have certain features enabled, disabled, or configured.

```
76543210
||||||||_ Bit0   (1): USE_DVI - display either VGA In, VGA Out, or a test screen to DVI.
|||||||__ Bit1   (2): USE_GPIO_31_47 - instead of GPIO 0 to 7 for VGA Out. Only relevant for the RP2350B.
||||||___ Bit2   (4): USE_VGA_IN_TO_DVI - instead of VGA Out (on start-up). Needs USE_DVI to be set to make sense.
|||||____ Bit3   (8): USE_IR - enable infra-red remote control.
||||_____ Bit4  (16): USE_CSYNC - instead of VSYNC and HSYNC for VGA Out. Needs to be set if USE_GPIO_31_47 is set.
|||______ Bit5  (32): USE_UART - as well as USB for STDIO serial comms.
||_______ Bit6  (64): Not used.
|________ Bit7 (128): Not used.
```








Notes to self:

Tags, and GitHub Releases and Assets and Licenses

Example

tag
    asset
    asset

palavo-v1.0.6
    palavo_config0_on_pico.uf2
    palavo_config0_on_pico2.uf2
    palavo_config1_on_pico2.uf2
    palavo_config21_on_pico2.uf2
    palavo_config53_on_pico2.uf2
    palavo_config53_at_125mhz_on_pico2.uf2
    palavo_config2_on_pimoroni_pico_lipo2xl_w_rp2350.uf2
    palavo_config8_on_solderparty_rp2350_stamp_xl.uf2

1. Use `$ ./make-assets.sh` to `make` all the relevant `.uf2` files, rename them, and copy them to the `assets/` directory.

2. Use `$ ls assets/ -xt | tac` to list the assets in the chronological order in which they were created.



## Useful script files

Instead of remembering the above command each time, you can copy the script `make-and-flash.sh` from the palavo directory:

`$ cp ../../../make-and-flash.sh .`

Using a text editor open `make-and-flash.sh`, modify the `adapter_serial_no` variable to that of your Debug Probe. If you don't want or need to specify a serial number (you only have one Debug Probe connected), blank the `target_adapter_cmnd` variable.

Then `make` the firmware and flash the RP2040:

`$ ./make-and-flash.sh`


Don't know what happened to this text!
 window in the next image. Underneath the 8th channel (channel 7) is a small white marker and a minimap of the whole of all of the captured channels. The small marker indicates the length and position of the scrollable section relative to the mimimap.

Open a serial terminal on your PC. I use `minicom` with this command line (You may need to change the device - the bit after the `-D`.):

`$ minicom -b 115200 -w -D /dev/ttyACM0`

(After minicom opens you may also need to add carriage returns with 'Ctrl-A U'.)

Press the 'h' key and something like the following help screen should appear:


![A monitor displaying the main coloured traces of a section of the captured data of various GPIO pins. Above the main traces, at the top of the screen is the Palavo logo on the left



# End of Document

Everything from here is stuff I may use and don't want to delete just yet


Here is I've paired this with a Pico 2, or another board with an RP2350 in Configuation 21


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
   12    11                      depending on the variant of the RP2350, in one of 5 modes and the mode is selected                    35    36

   14    13                                        33    34

   16    15    19    21    23    25    27    29    31    32

    G    17    18    20    22    24    26    28    30   G

```











```
                                                                             GP0  1       USB        60 VBUS
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



                                                           VGA_In_Dark_Blue  GP0  1       USB        40 VBUS
                                                          VGA_In_Light_Blue  GP1  2                  39 VSYS
                                                                             GND  3                  38 GND
                                                          VGA_In_Dark_Green  GP2  4                  37 3V3 EN
                                                         VGA_In_Light_Green  GP3  5                  36 3V3 OUT
                                                   |        VGA_In_Dark_Red  GP4  6                  35 ADC VREF
                                             ------|------ VGA_In_Light_Red  GP5  7                  34 GP28
                                                                             GND  8                  33 ADC GND
                                                          VGA_Out_Dark_Blue  GP6  9                  32 GP27  VGA_In_VSYNC
                                                         VGA_Out_Light_Blue  GP7 10      PICO  2     31 GP26  VGA_In_HSYNC_OR_CSYNC
                                                         VGA_Out_Dark_Green  GP8 11                  30 RUN
                                                        VGA_Out_Light_Green  GP9 12                  29 GP22  VGA_Out_CSYNC
                                                                             GND 13                  28 GND
                                                          VGA_Out_Dark_Red  GP10 14                  27 GP21
                                                         VGA_Out_Light_Red  GP11 15                  26 GP20
                                                                   DVI_D0+  GP12 16                  25 GP19  DVI_D2+
                                                                   DVI_D0-  GP13 17                  24 GP18  DVI_D2-
                                                                            GND  18                  23 GND
                                                                   DVI_CK+  GP14 19                  22 GP17  DVI_D1+
                                                                   DVI_CK-  GP15 20                  21 GP16  DVI_D1-
```


How many combinations of Palavo are there?

1. Using an RP2350A or RP2350B with VGA output, DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=0 (0b000))
2. Using an RP2350A or RP2350B with VGA output, no DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=1 (0b001))
3. Using an RP2350B with VGA output, DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=2 (0b010))
4. Using an RP2350B with only VGA output, i.e. no DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=3 (0b011))
5. Using an RP2350A or RP2350B with VGA output, DVI output, and defaults to outputting the VGA_In_signals to DVI, rather than its own captured signals. (PALAVO_CONFIG=4 (0b100))

N.B. Available for capture does NOT mean you can put external signals into any GPIO pins that are used as outputs. It means you can still capture those output signals.

## Palavo Config

Palavo is configured by passing the additional define into the `cmake` command line, above:

` -DPALAVO_CONFIG=X` where X is a number between 0 and 7, although 4 is the same as 0, and 6 is the same as 2.

| PALAVO_CONFIG | RP2350 Variant | DVI Output at Startup | Use High GPIO for UI | DVI Out | VGA Out |
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

543210
||||||_ Bit0  (1): USE_DVI - display either VGA In, VGA Out, or a test screen to DVI
|||||__ Bit1  (2): USE_GPIO_31_47 - instead any of the GPIO broken out on the Pico or Pico 2. Only relevant for the RP2350B
||||___ Bit2  (4): USE_VGA_IN_TO_DVI - instead of VGA Out (on start-up) - needs USE_DVI to make sense
|||____ Bit3  (8): USE_IR - enable infra-red remote control
||_____ Bit4 (16): USE_CSYNC - instead of VSYNC and HSYNC for VGA Out 
|______ Bit5 (32): USE_USB - instead of UART for STDIO serial comms

```

The default version PALAVO_CONFIG = 0 (binary info ok)
PALAVO_CONFIG = 1 (binary info wrong)
PALAVO_CONFIG = 16 (binary info ok)

So, for a VGA to DVI converter with USB, CSYNC and no IR we need  1 + 4 + 16 + 32 = 53

for a 6-bit VGA Out 

So, for a VGA and DVI analyser with USB we need  1 + 32 = 33 No longer works - might have been a conscious decision? There's not a CSYNC so VGA Out is not detecting it. There is now - and it is.

So, add CSYNC (16) 33 + 16 = 49 - try it - works, except VGA_In is monochrome because that's what we're expecting. This config isn't really a goer. Yes it is, at least now it is.


Solder Party version RP2xxx Basic Stamp version with RP2350B fitted using IR and USB UART = 8 + 32 = 40 (still working; binary info too)
      without USB = 8
AND a Pico2 with USE_DVI, USE_VGA_IN_TO_DVI and USE_CSYNC = 1 + 4 + 16 = 21 (still working; binary info too)




## Pinout for Raspberry Pi Pico 2

```
   VGA_In_Dark_Blue  GP0  1       USB        40 VBUS
  VGA_In_Light_Blue  GP1  2                  39 VSYS
                     GND  3                  38 GND
  VGA_In_Dark_Green  GP2  4                  37 3V3 EN
 VGA_In_Light_Green  GP3  5                  36 3V3 OUT
    VGA_In_Dark_Red  GP4  6                  35 ADC VREF
   VGA_In_Light_Red  GP5  7                  34 GP28  Infra-red RX
                     GND  8                  33 ADC GND
  VGA_Out_Dark_Blue  GP6  9                  32 GP27  VGA_In_VSYNC
 VGA_Out_Light_Blue  GP7 10     PICO 2       31 GP26  VGA_In_HSYNC_OR_CSYNC
 VGA_Out_Dark_Green  GP8 11                  30 RUN  CAPTAIN RESETTI
VGA_Out_Light_Green  GP9 12                  29 GP22  VGA_Out_CSYNC
                     GND 13                  28 GND
  VGA_Out_Dark_Red  GP10 14                  27 GP21  Serial_RX
 VGA_Out_Light_Red  GP11 15                  26 GP20  Serial_TX
           DVI D0+  GP12 16                  25 GP19  DVI D2+
           DVI D0-  GP13 17                  24 GP18  DVI D2-
                    GND  18                  23 GND
           DVI CK+  GP14 19                  22 GP17  DVI D1+
           DVI CK-  GP15 20                  21 GP16  DVI D1-
```

## Pinout for Pimoroni Pico LiPo 2 W XL

```
                       GP0  1       USB        60 VBUS
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
   VGA_Out_Dark_Blue  GP32 24                  37 GP46  Infra-red RX
  VGA_Out_Light_Blue  GP33 25        XL        36 GP45
  VGA_Out_Dark_Green  GP34 26                  34 GP44
 VGA_Out_Light_Green  GP35 27                  33 GP43
                       GND 28                  32 GND
    VGA_Out_Dark_Red  GP36 29                  31 GP39  Serial_RX
   VGA_Out_Light_Red  GP37 30                  31 GP38  Serial_TX
```

`hello`

## VGA 15-way Socket Wiring 

| VGA_Out_GPIO        | Resistor | VGA 15-way Socket |
| :---                |   :---:  | :---              |
|       VGA_Out_CSYNC |    47R   | 13 HSYNC          |
|                 GND |    0R    | 5 GND             |
|   VGA_Out_Dark_Blue |    1K    | 3 Blue            |
|  VGA_Out_Light_Blue |   470R   | 3 Blue            |
|  VGA_Out_Dark_Green |    1K    | 2 Green           |
| VGA_Out_Light_Green |   470R   | 2 Green           |
|    VGA_Out_Dark_Red |    1K    | 1 Red             |
|   VGA_Out_Light_Red |   470R   | 1 Red             |


## UART Comms
I use a Raspberry Pi Debug Probe (link) for programming the Pico (2) and for UART comms.

I prefer to use minicom as a serial monitor. Some - the one included with VS Code for example - prevent
some keystrokes from being transmitted.

`$ minicom -b 115200 -w -D /dev/ttyACM0` 

Then enable carriage returns with Ctrl-A U.


## PIO State Machine Usage for Pico (RP2040)

```
PIO      SM       Size  Needs PIO1*  Usage
0        0        6
0        1        11
0        2        14
0        3        1                 logic_capture
Total             31

1        0                          hsync5_program (vsync and hsync for vga out) - presumably not using this???
1        1        13                rgb5_150_mhz_RP2350_program (rrggbb for vga out) 
1        2        15                hsync5_program (vsync and hsync for vga out OR csync for vga out)     
1        3        1                 logic_capture
Total             29
```






## PIO State Machine Usage for Pico2 (RP235x)

```
PIO      SM       Size  Needs PIO1*  Usage
0        0        6         y       vga_capture_program
0        1        11                vga_detect_vsync_program
0        2        14                vga_detect_vsync_on_csync_program
0        3
Total             31

1        0                          hsync5_program (vsync and hsync for vga out) - presumably not using this???
1        1        13                rgb5_150_mhz_RP2350_program (rrggbb for vga out)
1        2        15                hsync5_program (vsync and hsync for vga out OR csync for vga out)
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
0                                    hsync5_program (vsync and hsync for vga out)
1        13                          rgb5_150_mhz_RP2350_program (rrggbb for vga out) 
2        15                          hsync5_program (csync for vga out)
3        1                           trigger and/or logic_capture for pin(s) using GPIO_BASE=16
Total    29

PIO 2 (GPIO_BASE=16)
0        31                          nec_ir_rx_program
1
2
3
Total    31

* PIO1 has PIO features that were introduced in RP2350 devices; RP2040 uses PIO0.



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




The way I have my system setup is PALAVO_CONFIG=2 on a Pimoroni Pico LiPo 2 W XL, and for DVI output I use PALAVO_CONFIG=5 on a Raspberry Pi Pico 2. This allows more contiguous GPIO pins free as inputs for capture. It will also allow for lots of RAM to be freed up for longer captures (not yet implemented - todo).








| VGA_Out_GPIO        | Resistor | VGA 15-way Socket |
| :---                |   :---:  | :---              |
|       VGA_Out_CSYNC |    47R   | 13 HSYNC          |
|                 GND |    0R    | 5 GND             |
|   VGA_Out_Dark_Blue |    1K   
How many combinations of Palavo are there?

1. Using an RP2350A or RP2350B with VGA output, DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=0 (0b000))
2. Using an RP2350A or RP2350B with VGA output, no DVI output, and with GPIOs 0-31 available for capture (PALAVO_CONFIG=1 (0b001))
3. Using an RP2350B with VGA output, DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=2 (0b010))
4. Using an RP2350B with only VGA output, i.e. no DVI output, and with GPIOs 0-47 available for capture (PALAVO_CONFIG=3 (0b011))
5. Using an RP2350A or RP2350B with VGA output, DVI output, and defaults to outputting the VGA_In_signals to DVI, rather than its own captured signals. (PALAVO_CONFIG=4 (0b100))

N.B. Available for capture does NOT mean you can put external signals into any GPIO pins that are used as outputs. It means you can still capture those output signals.

## Palavo Config

Palavo can be configured, depending on the variant of the RP2350, in one of 5 modes and the mode is selected by passing the additional define into the `cmake` command line, above:

` -DPALAVO_CONFIG=X` where X is a number between 0 and 7, although 4 is the same as 0, and 6 is the same as 2.
```


| PALAVO_CONFIG | RP2350 Variant | DVI Output at Startup | Use High GPIO for UI | DVI Out | VGA Out |
|     :---:     | :---           |         :---:         |        :---:         |  :---:  |  :---:  |
|       0       | A or B         |          n/a          |          No          |   No    |   Yes   |
|       1       | A or B         |        VGA Out        |          No          |   Yes   |   Yes   |
|       2       | B              |          n/a          |          Yes         |   No    |   Yes   |
|       3       | B              |        VGA Out        |          Yes         |   Yes   |   Yes   |
|       4       | A or B         |          n/a          |          No          |   No    |   Yes   |
|       5       | A or B         |        VGA In         |          No          |   Yes   |   Yes   |
|       6       | A or B         |          n/a          |          Yes         |   No    |   Yes   |
|       7       | A or B         |        VGA In         |          Yes         |   Yes   |   Yes   |

```

Use_Off_Pico_UI

Currently only Types 0, 1, 2 and 4, and 5 have been tested (4 is the same as 0). 

### Palavo Config Bits

```
 ___ USE_USB - Use USB for STDIO instead of UART ().
 ___ USE_CSYNC - Use Combined SYNC instead of VSYNC & HSYNC.
 ___ USE_IR - Use infra-red remote control.
 ___ USE_VGA_IN_TO_DVI - 0n start up display VGA In - i.e. a VGA to DVI converter.)
| __ USE_GPIO_31_47 - Don't use any GPIO pins found on the Pico or Pico 2. Only set this if using an RP2350B.
|| _ USE_DVI - Display either VGA In, VGA Out, or a test screen to DVI via HSTX.
|||
000

``` | 3 Blue            |
|  VGA_Out_Light_Blue |   470R   | 3 Blue            |
|  VGA_Out_Dark_Green |    1K    | 2 Green           |
| VGA_Out_Light_Green |   470R   | 2 Green           |
|    VGA_Out_Dark_Red |    1K    | 1 Red             |
|   VGA_Out_Light_Red |   470R   | 1 Red             |




the Pico uses a couple of PIO programs to output the 7 VGA_Out signals, whilst another PIO program simultaneously looks for a CSYNC 

In 'mirror VGA out' mode the Pico uses a couple of PIO programs to output the 7 VGA_Out signals, whilst another PIO program simultaneously looks for a CSYNC signal on CSYNC , and shortly after detecting one nothe PIO program copying the state of the Red, Green and Blue pins to the DVI's frame buffer, which is simultaneously and constantly being, via the HSTX peripheral, to the DVI pins.

In 'test' mode the above PIO programs are stopped and DVI's frame buffer is filled with a test pattern. 

In 'VGA in' mode the stopped PIO programs used in 'mirror VGA out' mode are restarted, only using the VGA_In pins for CSYNC detection and for Red, Green and Blue sampling. Another PIO program is started which is simultaneously looking for HSYNC and VSYNC signals.



In the `build` directory create a different, but still suitably-named, directory, and enter that directory:

`$ mkdir solderparty_rp2350_stamp_xl-DVI`

`$ cd solderparty_rp2350_stamp_xl-DVI`

Repeat the rest of the previous build process, except use this `cmake` command:

`$ cmake ../../../ -DPICO_BOARD=solderparty_rp2350_stamp_xl -DPALAVO_CONFIG=1`

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
      VGA_Out_VSYNC  GP0  1       USB      40 VBUS
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
   VGA_In_Dark_Blue  GP0  1       USB      40 VBUS
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
 I created Palavo to help me *view* the inputs and outputs of various PIO programs I was working on, including the VGA driver.


Making notes before changing UART functions

in `build/pico2/config29/`
current `$ picotool info palavo.uf2`

 Program Information
 name:                palavo
 web site:            https://github.com/peterstansfeld/palavo
 description:         A logic analyser with VGA output and UART control
 features:            VGA input
                      DVI output
 binary start:        0x10000000
 binary end:          0x1000c494
 target chip:         RP2350
 image type:          ARM Secure


with `pico_enable_stdio_uart(palavo 1)` includee

Program Information
 name:          palavo
 web site:      https://github.com/peterstansfeld/palavo
 description:   A logic analyser with VGA output and UART control
 features:      VGA input
                DVI output
                UART stdin / stdout
 binary start:  0x10000000
 binary end:    0x1000c724
 target chip:   RP2350
 image type:    ARM Secure


Program Information
 name:          palavo
 web site:      https://github.com/peterstansfeld/palavo
 description:   A logic analyser with VGA output and UART control
 features:      VGA input
                DVI output
                UART stdin / stdout
 binary start:  0x10000000
 binary end:    0x1000ba0c
 target chip:   RP2350
 image type:    ARM Secure
 
pico2/config0


uart

Program Information
 name:          palavo
 web site:      https://github.com/peterstansfeld/palavo
 description:   A logic analyser with VGA output and UART control
 features:      Infra-red control
 binary start:  0x10000000
 binary end:    0x1000b72c
 target chip:   RP2350
 image type:    ARM Secure

stdio uart


Program Information
 name:          palavo
 web site:      https://github.com/peterstansfeld/palavo
 description:   A logic analyser with VGA output and UART control
 features:      Infra-red control
                UART stdin / stdout
 binary start:  0x10000000
 binary end:    0x1000afbc
 target chip:   RP2350
 image type:    ARM Secure


stdio usb

Program Information
 name:          palavo
 web site:      https://github.com/peterstansfeld/palavo
 description:   A logic analyser with VGA output and UART control
 features:      Infra-red control
                USB stdin / stdout
 binary start:  0x10000000
 binary end:    0x1000e4b8
 target chip:   RP2350
 image type:    ARM Secure

pico2 config29      

Program Information
 name:                palavo
 web site:            https://github.com/peterstansfeld/palavo
 description:         PIO-Assisted Logic Analyser with VGA Output
 features:            VGA input
                      DVI output
                      UART stdin / stdout
 binary start:        0x10000000
 binary end:          0x1000bc14
 target chip:         RP2350
 image type:          ARM Secure


solder_party stamp

Program Information
 name:                palavo
 web site:            https://github.com/peterstansfeld/palavo
 description:         PIO-Assisted Logic Analyser with VGA Output
 features:            Infra-red control
                      USB stdin / stdout
 binary start:        0x10000000
 binary end:          0x1000e330
 target chip:         RP2350
 image type:          ARM Securepalavo



To build and install `pioasm` so that it's not downloaded and built on each `make` of a new pico SDK project build folder, I followed [these instructions](https://forums.raspberrypi.com/viewtopic.php?p=2329581&hilit=pioasm+keeps+building#p2329581):

```
cd ~/pico/pico-sdk/tools/pioasm
mkdir build
cd build
cmake -DPIOASM_VERSION_STRING="2.2.0" ..
make
sudo make install
`sudo ln -s ~/pico/pico-sdk/tools/pioasm/build/pioasm /usr/local/bin/pioasm
```

Although, the last instruction reported this error:

`ln: failed to create symbolic link '/usr/local/bin/pioasm': File exists`

Thanks, hippy (the person who shared the instructions).


*A Raspberry Pi Pico (or Pico 2) housed in a half-sized breadboard (Configuration 0). Nine of its pins are connected to corresponding pins on another Pico 2 (Configuration 21), which is housed in another half-sized breadboard, along with a DVI Sock. Here are the connections:*

| Pico or Pico 2 Pin | Palavo 0 Function   | Palavo 21 Function | Pico 2 Pin | Pico Sock Pin |
|       :---:        | ---:                | :---               |   :---:    |     :---:     |
|        GP0         | VGA_Out_VSYNC       | VGA_In_VSYNC       |    GP0     |               |
|        GP1         | VGA_Out_HSYNC_CSYNC | VGA_In_HSYNC_CSYNC |    GP1     |               |
|        GND         | GND                 | GND                |    GND     |               |
|        GP2         | VGA_Out_Dark_Blue   | VGA_In_Dark_Blue   |    GP2     |               |
|        GP3         | VGA_Out_Light_Blue  | VGA_In_Light_Blue  |    GP3     |               |
|        GP4         | VGA_Out_Dark_Green  | VGA_In_Dark_Green  |    GP4     |               |
|        GP5         | VGA_Out_Light_Green | VGA_In_Light_Green |    GP5     |               |
|        GP6         | VGA_Out_Dark_Red    | VGA_In_Dark_Red    |    GP6     |               |
|        GP7         | VGA_Out_Light_Red   | VGA_In_Light_Red   |    GP7     |               |
|                    |                     | VGA_Out_CSYNC      |    GP10    |               |
|                    |                     | VGA_Out_RGB        |    GP11    |               |
|                    |                     |                    |    GP12    |     12/D0+    |
|                    |                     |                    |    GP13    |     13/D0-    |
|                    |                     |                    |    GND     |      GND      |
|                    |                     |                    |    GP14    |     14/CK+    |
|                    |                     |                    |    GP15    |     15/CK-    |
|                    |                     |                    |    GP16    |     16/D2+    |
|                    |                     |                    |    GP17    |     17/D2-    |
|                    |                     |                    |    GND     |      GND      |
|                    |                     |                    |    GP18    |     18/D1+    |
|                    |                     |                    |    GP19    |     19/D1-    |





```
                       | USB |
                       |     |
 G 0  VGA_Out_VSYNC     -----                  GND G
 G 1  VGA_Out_HSYNC or VGA_Out_CSYNC            3V G
 G 2  VGA_Out_Dark_Blue                         5V G
 G 3  VGA_Out_Light_Blue ---------------       BAT G
 G 4  VGA_Out_Dark_Green                |     BOOT G
 G 5  VGA_Out_Light_Green               |      RST G
 G 6  VGA_Out_Dark_Red                  |      CLK G
 G 7  VGA_Out_Light_Red                 |       IO G
 G 8  Serial_TX*                        |       EN G
 G 9  Serial_RX*                        | BAT STAT G
 G 10 Infra_Red_RX**                    |       47 G
 G 11        |      RP2350 Stamp XL     |       46 G
 G 12        |                          |       45 G
 G 13        |                          |       44 G
 G 14        |                          |       43 G
 G 15        |                          |       42 G
 G 16        |                          |       41 G
 G 17        |                          |       40 G
 G 18        |                          |       39 G
 G 19        |                          |       38 G
 G 20         --------------------------        37 G
 G 21         RP2xxx Stamp Carrier Basic        36 G

      22 23 24 25 26 27 28 29 30 31 32 33 34 35
      G  G  G  G  G  G  G  G  G  G  G  G  G  G
```





```
                       GP0  1       USB        60 VBUS
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
                       3V3 21                  40 BOOT
       VGA_Out_CSYNC  GP31 22                  39 GP47
                       GND 23                  38 GND
   VGA_Out_Dark_Blue  GP32 24                  37 GP46  Infra_Red_RX**
  VGA_Out_Light_Blue  GP33 25        XL        36 GP45
  VGA_Out_Dark_Green  GP34 26                  34 GP44
 VGA_Out_Light_Green  GP35 27                  33 GP43
                       GND 28                  32 GND
    VGA_Out_Dark_Red  GP36 29                  31 GP39  Serial_RX*
   VGA_Out_Light_Red  GP37 30                  31 GP38  Serial_TX*
```

