/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020-2023 Peter Müller <peter@crycode.de> (https://crycode.de)
 *
 * License: CC BY-NC-SA 4.0
 *
 *
 * Controller specific definitions.
 */

#ifndef	__MCP_CAN_BOOT_CONTROLLERS_H__
#define	__MCP_CAN_BOOT_CONTROLLERS_H__

#include <avr/io.h>

#define BOOTLOADER_SIZE 4096

#if defined(__AVR_ATmega32__)
  #define IV_REG GICR

  #define SPI_DDR  DDRB
  #define SPI_PORT PORTB
  #define SPI_SS   4
  #define SPI_MOSI 5
  #define SPI_MISO 6
  #define SPI_SCK  7

#elif defined(__AVR_ATmega64__) || defined(__AVR_ATmega128__) || defined(__AVR_ATmega2560__)
  #define IV_REG MCUCR

  #define SPI_DDR  DDRB
  #define SPI_PORT PORTB
  #define SPI_SS   0
  #define SPI_MOSI 2
  #define SPI_MISO 3
  #define SPI_SCK  1

#elif defined(__AVR_ATmega32U4__)
  #define IV_REG MCUCR

  #define SPI_DDR DDRB
  #define SPI_PORT PORTB
  #define SPI_SS 0
  #define SPI_MOSI 2
  #define SPI_MISO 3
  #define SPI_SCK 1

#elif defined(__AVR_ATmega328P__)
  #define IV_REG MCUCR

  #define SPI_DDR  DDRB
  #define SPI_PORT PORTB
  #define SPI_SS   2
  #define SPI_MOSI 3
  #define SPI_MISO 4
  #define SPI_SCK  5

#elif defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284P__)
  #define IV_REG MCUCR

  #define SPI_DDR  DDRB
  #define SPI_PORT PORTB
  #define SPI_SS   4
  #define SPI_MOSI 5
  #define SPI_MISO 6
  #define SPI_SCK  7

#else
  #error Unsupported MCU
#endif

#endif
