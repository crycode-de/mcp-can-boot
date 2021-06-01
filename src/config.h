/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020-2021 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 *
 * Configuration
 */

#ifndef	__MCP_CAN_BOOT_CONFIG_H__
#define	__MCP_CAN_BOOT_CONFIG_H__

/**
 * Optional definition of a LED port, which will be used to indicate
 * bootloader actions.
 */
//#define LED      PORTA1
//#define LED_DDR  DDRA
//#define LED_PORT PORTA

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
 * store The MCU Status Register to Register 2
 * The MCU Status Register provides information on which reset source
 * caused an MCU reset.
 *
 * [code main prog.]
 *
 * uint8_t mcusr __attribute__ ((section (".noinit")));//<= the MCU Status Register
 * void getMCUSR(void) __attribute__((naked)) __attribute__((section(".init0")));
 * void getMCUSR(void)
 * {
 *     __asm__ __volatile__ ( "mov %0, r2 \n" : "=r" (mcusr) : );
 *  }
 */
#define MCUSR_TO_R2 1

/**
 * Data rate of the CAN bus.
 * CAN_5KBPS, CAN_10KBPS, CAN_20KBPS, CAN_31K25BPS, CAN_33KBPS, CAN_40KBPS,
 * CAN_50KBPS, CAN_80KBPS, CAN_83K3BPS, CAN_95KBPS, CAN_100KBPS, CAN_125KBPS,
 * CAN_200KBPS, CAN_250KBPS, CAN_500KBPS or CAN_1000KBPS
 */
#define CAN_KBPS CAN_500KBPS

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
 * CAN-ID for bootloader message from MCU to remote.
 */
#define CAN_ID_MCU_TO_REMOTE 0x1FFFFF01UL

/**
 * CAN-ID for bootloader message from remote to MCU.
 */
#define CAN_ID_REMOTE_TO_MCU 0x1FFFFF02UL

#endif
