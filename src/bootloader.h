/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 */

#ifndef	__MCP_CAN_BOOT_MAIN_H__
#define	__MCP_CAN_BOOT_MAIN_H__

#include <inttypes.h>
#include <avr/boot.h>
#include <avr/wdt.h>

#include "mcp2515.h"
#include "config.h"
#include "controllers.h"

/**
 * Commandset version of this bootloader.
 * Used to identify a possibly incompatilbe flash application on remote.
 */
#define BOOTLOADER_CMD_VERSION 0x01

/*
 * Positions of fixed data parts in each bootloader CAN message.
 */
#define CAN_DATA_BYTE_MCU_ID_MSB   0
#define CAN_DATA_BYTE_MCU_ID_LSB   1
#define CAN_DATA_BYTE_CMD          2
#define CAN_DATA_BYTE_LEN_AND_ADDR 3

/*
 * CAN message commands definitions
 */
#define CMD_ERROR                    0b00000001 // mcu -> remote
#define CMD_BOOTLOADER_START         0b00000010 // mcu -> remote
#define CMD_FLASH_INIT               0b00000110 // remote -> mcu
#define CMD_FLASH_READY              0b00000100 // mcu -> remote
#define CMD_FLASH_SET_ADDRESS        0b00001010 // remote -> mcu
#define CMD_FLASH_ADDRESS_ERROR      0b00001011 // mcu -> remote
#define CMD_FLASH_DATA               0b00001000 // remote -> mcu
#define CMD_FLASH_DATA_ERROR         0b00001101 // mcu -> remote
#define CMD_FLASH_DONE               0b00010000 // remote -> mcu
#define CMD_FLASH_DONE_VERIFY        0b01010000 // remote <-> mcu
#define CMD_FLASH_ERASE              0b00100000 // remote -> mcu
#define CMD_FLASH_READ               0b01000000 // remote -> mcu
#define CMD_FLASH_READ_DATA          0b01001000 // mcu -> remote
#define CMD_FLASH_READ_ADDRESS_ERROR 0b01001011 // mcu -> remote
#define CMD_START_APP                0b10000000 // mcu <-> remote

/*
 * Fixed definitions to be used in the code.
 */
#define MCU_ID_LSB (MCU_ID & 0xFF)
#define MCU_ID_MSB ((MCU_ID >> 8) & 0xFF)
#define FLASHEND_BL (FLASHEND - BOOTLOADER_SIZE)

/*
 * Function declarations
 */
int main ();
void writeFlashPage ();
void boot_program_page (uint16_t page, uint8_t *buf);
void startApp ();

#endif
