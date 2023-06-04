/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020-2023 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 *
 * Configuration
 */

#ifndef	__MCP_CAN_BOOT_CONFIG_H__
#define	__MCP_CAN_BOOT_CONFIG_H__

/**
 * The ID of the MCU to identify it in bootloader CAN messages.
 * You may use a fixed value or an expressions to get the ID from the EEPROM
 * which allows you to change the ID by your main application.
 * Range: 0x0000 to 0xFFFF
 */
#define MCU_ID 0x0042
//#define MCU_ID eeprom_read_word((uint16_t*) 0x00)
//#define MCU_ID eeprom_read_byte((uint8_t*) 0x00)

/**
 * Timeout for the bootloader in milliseconds.
 * In this amount of time after MCU reset a "flash init" command must be
 * received via CAN to enter the bootloading mode. Otherwise the main
 * application will be started.
 */
#define TIMEOUT 250

/**
 * Bitrate of the CAN bus.
 * CAN_5KBPS, CAN_10KBPS, CAN_20KBPS, CAN_31K25BPS, CAN_33KBPS, CAN_40KBPS,
 * CAN_50KBPS, CAN_80KBPS, CAN_83K3BPS, CAN_95KBPS, CAN_100KBPS, CAN_125KBPS,
 * CAN_200KBPS, CAN_250KBPS, CAN_500KBPS or CAN_1000KBPS
 */
#define CAN_KBPS CAN_500KBPS

/**
 * Set multiple bitrates to let the bootloader try to detect the bitrate used on
 * the CAN bus.
 * For each set bitrate the MCP2515 will be set to this bitrate and into listen
 * only mode. Then the bootloader will wait for a defined timeout to receive a
 * valid message. If a message is received the bootloader will assume the
 * current bitrate as the bitrate to use.
 * If no bitrate could be detected, the fixed bitrate defined in CAN_KBPS will
 * be used. If not defined, only the fixed bitrate will be used.
 * Hint: If you use this together with the LED definition, you may need to use
 *   a 4096 words bootloader section together with the *_4k PlatformIO envs
 *   for some MCUs, but a 4096 words bootloader is not supported by all MCUs.
 * Hint: If you use this, the delay to boot the main application will be in
 *   worst case the TIMEOUT set above plus the number of bitrates to detect
 *   multiplied by the TIMEOUT_DETECT_CAN_KBPS.
 */
//#define CAN_KBPS_DETECT CAN_50KBPS, CAN_100KBPS, CAN_125KBPS, CAN_250KBPS, CAN_500KBPS

/**
 * Timeout for each bitrate to detect a valid message in milliseconds.
 * In this time a valid message must be received to let the bootloader detect
 * the bitrate.
 * Only used if CAN_KBPS_DETECT is set.
 */
//#define TIMEOUT_DETECT_CAN_KBPS 100

/**
 * Clock speed of the MCP2515 CAN controller.
 * MCP_8MHZ, MCP_16MHZ or MCP_20MHZ
 */
#define MCP_CLOCK MCP_16MHZ

/**
 * Optional definition of a custom CS (chip select) pin for the MCP2515.
 * If not defined, the default SPI_SS pin will be used for chip select.
 */
//#define MCP_CS      PORTB0
//#define MCP_CS_DDR  DDRB
//#define MCP_CS_PORT PORTB

/**
 * When using a custom CS pin, then it must be ensured that the SPI_SS pin is
 * defined as an output or externally pulled high. Otherwise the bootloader may
 * be unresponding because the controller may enter SPI slave mode.
 * Use the following definition to set SPI_SS as an output (if a custom CS pin
 * is used) with the given value (HIGH or LOW).
 */
//#define SET_SPI_SS_OUTPUT HIGH

/**
 * Use the Extended Frame Format (EFF) for CAN-Messages. (8 hex chars)
 * Set to `false` to use the Standard Frame Format (SFF). (3 hex chars)
 */
#define CAN_EFF true

/**
 * CAN-ID for bootloader message from MCU to remote.
 */
#define CAN_ID_MCU_TO_REMOTE 0x1FFFFF01UL
//#define CAN_ID_MCU_TO_REMOTE 0x1F1

/**
 * CAN-ID for bootloader message from remote to MCU.
 */
#define CAN_ID_REMOTE_TO_MCU 0x1FFFFF02UL
//#define CAN_ID_REMOTE_TO_MCU 0x1F2

/**
 * Optional definition of a LED port, which will be used to indicate
 * bootloader actions.
 */
//#define LED      PORTA1
//#define LED_DDR  DDRA
//#define LED_PORT PORTA

/**
 * Store the MCU Status Register to Register 2 (R2).
 * The MCU Status Register provides information on which reset source
 * caused an MCU reset.
 *
 * Paste this code into your program (not the bootloader):
 * \code
 *  uint8_t mcusr __attribute__ ((section (".noinit")));
 *  void getMCUSR(void) __attribute__((naked)) __attribute__((section(".init0")));
 *  void getMCUSR(void) {
 *    __asm__ __volatile__ ( "mov %0, r2 \n" : "=r" (mcusr) : );
 *  }
 * \endcode
 *
 * To use:
 * \code
 *  mcusr; // the MCU Status Register (global variable)
 * \endcode
 *
 * Or using a local variable:
 * \code
 *  void main() {
 *    uint8_t mcusr;
 *    __asm__ __volatile__ ( "mov %0, r2 \n" : "=r" (mcusr) : );
 *  }
 * \endcode
 *
 * To use:
 * \code
 *  mcusr; // the MCU Status Register (local variable in function main)
 * \endcode
 */
#define MCUSR_TO_R2 true

#endif
