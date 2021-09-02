# Si4702 / Si4703 FM radio library for the Raspberry Pi Pico

This library provides a convenient interface to the Si4702 and Si4703 FM radio chips from Silicon Labs. It targets the Raspberry Pi Pico and similar RP2040 based boards using the C/C++ SDK.

![Si470x_400px](https://user-images.githubusercontent.com/278476/131880806-17f74a33-8a87-4193-bc10-9c23272fba24.png)

## Overview

Main controls:

- FM band (87.5-108 / 76-90 / 76-108 MHz)
- channel spacing and de-emphasis
- seek sensitivity
- volume (with extended range)
- softmute (mutes noisy frequencies)
- mono / stereo output

Features:

- tune / seek the next station without blocking the CPU
- monitor signal strength and stereo signal
- RDS (only on Si4703) - decode station name, radio-text, and alternative frequencies

## Example

A test program is included. To interact with it, establish a serial connection through USB or UART.

```
Si470X - test program
=====================
- =   Volume down / up
1-9   Station presets
{ }   Frequency down / up
[ ]   Seek down / up
s     Toggle seek sensitivity
0     Toggle mute
f     Toggle softmute
m     Toggle mono
i     Print station info
r     Print RDS info
x     Power down
?     Print help
```

### Building

Follow the instructions in [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf) to setup your build environment. Then:

- clone repo
- `mkdir build`, `cd build`, `cmake ../`, `make`
- copy `fm_example.uf2` to Raspberry Pico

### Wiring

Communication is done through I2C. An additional GPIO pin is used to reset the FM chip during powerup. The default pins are:

| Si470x pin | Raspberry Pi Pico pin |
| ---------- | --------------------- |
| SDA        | GP4                   |
| SCL        | GP5                   |
| RST        | GP15                  |

## Links

- [pico_rda5807](https://github.com/vmilea/pico_rda5807) is a similar library for the RDA5807 FM radio chip.

## Authors

Valentin Milea <valentin.milea@gmail.com>
