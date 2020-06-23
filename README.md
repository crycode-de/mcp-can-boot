# MCP-CAN-Boot

CAN bus bootloader for **AVR microcontrollers** attached to an **MCP2515** CAN controller.


## Supported features
* Flash the main application into a MCU (microcontroller unit)
* Verify after flashing
* Read the whole flash (excluding the bootloader area)
* Erase the whole flash (excluding the bootloader area)
* Unique 16bit IDs to identify the MCU to flash
* Correctly handled disabling of the watchdog at startup to prevent bootloader loops when using the watchdog in the main application
* Very low impact on active CAN systems which enables to flash MCUs in active networks


## Bootloader size
This bootloader will fit into a 2048 words (4096 bytes) bootloader section.

The fuse bits of the MCU have to be set correctly to a boot flash section size of 2048 words and the boot reset vector must be enabled (BOOTRST=0).


## CAN bus communication

The whole communication via the CAN bus uses only two extended frame CAN-IDs.

*Defaults:*
* `0x1FFFFF01` for messages from MCU to remote
* `0x1FFFFF02` for messages from remote to MCU

Using this two IDs nearly at the end of CAN-ID range with the lowest priority there will be almost none interference flashing an MCU in a active CAN system.

Each CAN message consists of fixed eight data bytes.
The first four bytes are used for MCU identification, commands, data lengths and data identification. The other four bytes contain the data to read or write.


## Currently supported AVR controllers
* [ATmega32](http://ww1.microchip.com/downloads/en/devicedoc/doc2503.pdf)
* [ATmega328P](http://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf)
* [ATmega64](http://ww1.microchip.com/downloads/en/DeviceDoc/atmel-2490-8-bit-avr-microcontroller-atmega64-l_datasheet.pdf)
* [ATmega644P](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-42744-ATmega644P_Datasheet.pdf)
* [ATmega128](http://ww1.microchip.com/downloads/en/DeviceDoc/doc2467.pdf)
* [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/doc8059.pdf)
* [ATmega2560](https://ww1.microchip.com/downloads/en/devicedoc/atmel-2549-8-bit-avr-microcontroller-atmega640-1280-1281-2560-2561_datasheet.pdf)


## Flash-App

The remote application for flashing the MCU using the CAN bus is written in [Node.js](https://nodejs.org/) and located in the `flash-app` directory.

The flash-app requires a CAN interface at the system where it should run. This may be a Raspberry Pi with an attached MCP2515 CAN controller.

To install the dependencies for the flash-app just run `npm install` in the `flash-app` directory.

To run the flash-app just call `node flash-app.js [...]`.

### Flash-app parameters

```
--file, -f    Hex file to flash                            [string] [required]
--iface, -i   CAN interface to use                  [string] [default: "can0"]
--partno, -p  Specific AVR device like in avrdude          [string] [required]
--mcuid, -m   ID of the MCU bootloader                     [string] [required]
-e            Erase whole flash before flashing new data             [boolean]
-V            Do not verify                                          [boolean]
-r            Read whole flash and save to given file (no flashing!)  [string]
-F            Force flashing, even if the bootloader version missmatched
                                                                     [boolean]
--help, -h    Show help                                              [boolean]
```

Example: `node flash-app.js -f firmware.hex -p m1284p -m 0x0042`

## Detailed description of the used CAN messages

***TODO***


## License

**CC BY-NC-SA 4.0**

[creative commons Attribution-NonCommercial-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-nc-sa/4.0/)

Copyright (C) 2020 Peter Müller <peter@crycode.de> (https://crycode.de)
