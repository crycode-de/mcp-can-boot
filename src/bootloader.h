/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020-2021 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 */

#ifndef	__MCP_CAN_BOOT_MAIN_H__
#define	__MCP_CAN_BOOT_MAIN_H__

#include <inttypes.h>
#include <avr/boot.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>

#include "mcp2515.h"
#include "config.h"
#include "controllers.h"

/**
 * Command set version of this bootloader.
 * Used to identify a possibly incompatible flash application on remote.
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
#define CMD_PING                     0b00000000 // remote -> mcu
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
#define MCU_ID_LSB (mcuId & 0xFF)
#define MCU_ID_MSB ((mcuId >> 8) & 0xFF)
#define FLASHEND_BL (FLASHEND - BOOTLOADER_SIZE)

/*
 * Function declarations
 */
int main ();
void writeFlashPage ();
void boot_program_page (uint16_t page, uint8_t *buf);
void startApp ();

/*
 * Definition checks
 */
#ifdef LED
  #if !defined(LED_DDR) || !defined(LED_PORT)
    #error When using LED, also LED_DDR and LED_PORT must be defined!
  #endif
#endif

#ifdef MCP_CS
  #if !defined(MCP_CS_DDR) || !defined(MCP_CS_PORT)
    #error When using MCP_CS, also MCP_CS_DDR and MCP_CS_PORT must be defined!
  #endif

  #ifndef SET_SPI_SS_OUTPUT
    #warning You are using a custom CS pin and SPI_SS is not defined as an output which may lead to an unresponding bootloader. Please check config.h for SET_SPI_SS_OUTPUT or make sure your hardware will pull up SPI_SS externally.
  #endif
#endif

#if CAN_EFF
  #if CAN_ID_MCU_TO_REMOTE > 0x1FFFFFFF
    #error CAN_ID_MCU_TO_REMOTE is greater than 0x1FFFFFFF! Please check your config!
  #endif
  #if CAN_ID_REMOTE_TO_MCU > 0x1FFFFFFF
    #error CAN_ID_REMOTE_TO_MCU is greater than 0x1FFFFFFF! Please check your config!
  #endif
#else
  #if CAN_ID_MCU_TO_REMOTE > 0x7FF
    #error CAN_EFF is not enabled and CAN_ID_MCU_TO_REMOTE is greater than 0x7FF! Please check your config!
  #endif
  #if CAN_ID_REMOTE_TO_MCU > 0x7FF
    #error CAN_EFF is not enabled and CAN_ID_REMOTE_TO_MCU is greater than 0x7FF! Please check your config!
  #endif
#endif

#ifdef CAN_KBPS_DETECT
  #if !defined(TIMEOUT_DETECT_CAN_KBPS)
    #error When using CAN_KBPS_DETECT, also TIMEOUT_DETECT_CAN_KBPS must be defined!
  #endif
#endif

#endif
