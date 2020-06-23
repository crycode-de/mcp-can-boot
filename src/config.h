/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 *
 * Configuration
 */

#ifndef	__MCP_CAN_BOOT_CONFIG_H__
#define	__MCP_CAN_BOOT_CONFIG_H__

/**
 * Optional definition of a LED port, which will be used to indecate
 * bootloader actions.
 */
//#define LED      PORTA1
//#define LED_DDR  DDRA
//#define LED_PORT PORTA

/**
 * The ID of the MCU to identify it in bootloader CAN messages.
 * Range: 0x0000 to 0xFFFF
 */
#define MCU_ID 0x0042

/**
 * Timeout for the bootloader in milliseconds.
 * In this amount of time after MCU reset a "flash init" command must be
 * received via CAN to enter the bootloading mode. Otherwise the main
 * application will be started.
 */
#define TIMEOUT 250

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
 * CAN-ID for bootloader message from MCU to remote.
 */
#define CAN_ID_MCU_TO_REMOTE 0x1FFFFF01UL

/**
 * CAN-ID for bootloader message from remote to MCU.
 */
#define CAN_ID_REMOTE_TO_MCU 0x1FFFFF02UL

#endif
