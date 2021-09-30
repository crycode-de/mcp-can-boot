/*
 * MCP-CAN-Boot
 *
 * CAN bus bootloader for AVR microcontrollers attached to an MCP2515 CAN controller.
 *
 * Copyright (C) 2020-2021 Peter Müller <peter@crycode.de> (https://crycode.de)
 * License: CC BY-NC-SA 4.0
 */

#include "bootloader.h"

// define macros for LED control if enabled
#ifdef LED
  #define LED_ON     LED_PORT |= (1 << LED)
  #define LED_OFF    LED_PORT &= ~(1 << LED)
  #define LED_TOGGLE LED_PORT ^= (1 << LED)
  #define LED_INIT   LED_DDR |= (1 << LED)
  #define LED_DEINIT LED_DDR &= ~(1<<LED)
#else
  #define LED_ON
  #define LED_OFF
  #define LED_TOGGLE
  #define LED_INIT
  #define LED_DEINIT
#endif

// "function" to jump to the main application
void (*gotoApp)( void ) = 0x0000;

// global vars for flash data handling
uint8_t flashBuffer[SPM_PAGESIZE];
uint16_t flashBufferPos = 0;
uint16_t flashBufferDataCount = 0;
uint16_t flashPage = 0;

// CAN bus communication
struct can_frame canMsg;
MCP2515 mcp2515;

/*
 * Very early clear watchdog reset flag and turn off the watchdog.
 * "The watchdog timer remains active even after a system reset
 *  (except a power-on condition), using the fastest prescaler value
 *  (approximately 15 ms). It is therefore required to turn off the watchdog
 *  early during program startup"
 * https://www.nongnu.org/avr-libc/user-manual/group__avr__watchdog.html
 */
void get_mcusr(void) __attribute__((naked)) __attribute__((used)) __attribute__((section(".init3")));
void get_mcusr(void) {
#if defined(MCUCSR)
  #if MCUSR_TO_R2 // store MCUCSR into R2 if enabled
    __asm__ __volatile__(
        "  mov r2, %[mcusr_val] ;Move Between Registers \n\t"
        ::[mcusr_val] "r"(MCUCSR));
  #endif
  MCUCSR = 0;
#else
  #if MCUSR_TO_R2 // store MCUSR into R2 if enabled
    __asm__ __volatile__(
        "  mov r2, %[mcusr_val] ;Move Between Registers \n\t"
        ::[mcusr_val] "r"(MCUSR));
  #endif
  MCUSR = 0;
#endif
  wdt_disable();
}

/**
 * The main function of the bootloader.
 */
int main () {
  // Read from R2 into local mcusr variable if enabled.
  // We use a local variable here to save some space.
  #if MCUSR_TO_R2
    uint8_t mcusr = 0x00; // MCUSR from bootloader
    __asm__ __volatile__("  mov %[mcusr_val],r2 ;Move Between Registers \n\t"
                        :[mcusr_val] "=r"(mcusr));
  #endif

  // call init from arduino framework to setup timers
  init();

  // local variables to save some flash space used by the bootloader
  uint32_t flashAddr = 0;
  boolean flashing = false;

  // change interrupt vectors to bootloader section
  uint8_t sregtemp = SREG;
  cli();
  uint8_t ivtemp = IV_REG;
  IV_REG = ivtemp | (1<<IVCE);
  IV_REG = ivtemp | (1<<IVSEL);
  SREG = sregtemp;

  // init CAN controller
  mcp2515.init();

  // init LED (if defined)
  LED_INIT;
  LED_ON;

  // fill flash buffer with predefined data
  memset(flashBuffer, 0xFF, SPM_PAGESIZE);

  // reset the CAN controller, go into infinite loop with LED blinking on errors
  if (mcp2515.reset() != MCP2515::ERROR_OK) {
    while (1) {
      LED_OFF;
      delay(50);
      LED_ON;
      delay(50);
    }
  }

  mcp2515.setBitrate(CAN_KBPS, MCP_CLOCK);

  // set mcp2515 filter to accept CAN_ID_REMOTE_TO_MCU only
  #if CAN_EFF
    mcp2515.setFilterMask(MCP2515::MASK0, true, CAN_EFF_MASK);
    mcp2515.setFilter(MCP2515::RXF0, true, CAN_ID_REMOTE_TO_MCU);
  #else
    mcp2515.setFilterMask(MCP2515::MASK0, false, CAN_SFF_MASK);
    mcp2515.setFilter(MCP2515::RXF0, false, CAN_ID_REMOTE_TO_MCU);
  #endif

  mcp2515.setNormalMode();

  // set own mcu ID as a variable which enables mcu ID to be read from eeprom
  uint16_t mcuId = MCU_ID;

  // send bootloader start message
  #if CAN_EFF
    canMsg.can_id = CAN_ID_MCU_TO_REMOTE | CAN_EFF_FLAG;
  #else
    canMsg.can_id = CAN_ID_MCU_TO_REMOTE;
  #endif
  canMsg.can_dlc = 8;
  canMsg.data[CAN_DATA_BYTE_MCU_ID_MSB]   = MCU_ID_MSB;
  canMsg.data[CAN_DATA_BYTE_MCU_ID_LSB]   = MCU_ID_LSB;
  canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_BOOTLOADER_START;
  canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
  canMsg.data[4] = SIGNATURE_0;
  canMsg.data[5] = SIGNATURE_1;
  canMsg.data[6] = SIGNATURE_2;
  canMsg.data[7] = BOOTLOADER_CMD_VERSION;
  mcp2515.sendMessage(&canMsg);

  // local vars for timed actions
  uint32_t startTime = millis();
  uint32_t curTime;
  uint32_t ledTime = 0;

  // main loop
  while (1) {
    curTime = millis();

    // start the main application if we are not in bootloading mode and run into timeout
    if (!flashing && curTime > startTime + TIMEOUT) {
      startApp();
    }

    // turn led on if time to turm is set and greater than current time
    if (ledTime != 0 && curTime >= ledTime) {
      LED_ON;
      ledTime = 0;
    }

    // try to get a message from the CAN controller
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
      // got a message...
      if (canMsg.can_id ==
          #if CAN_EFF
            (CAN_ID_REMOTE_TO_MCU | CAN_EFF_FLAG)
          #else
            CAN_ID_REMOTE_TO_MCU
          #endif
        && canMsg.can_dlc == 8
        && canMsg.data[CAN_DATA_BYTE_MCU_ID_MSB] == MCU_ID_MSB
        && canMsg.data[CAN_DATA_BYTE_MCU_ID_LSB] == MCU_ID_LSB) {
        // ... and the message is for this bootloader

        // for all CAN messages to send in this block, can_dlc and MCU ID will
        // be set correctly by the incoming message, so we can save flash space
        // by not setting them again ... :-)

        // toggle the LED on each can message and set time to turn it on again
        // after 100ms of inactivity
        LED_TOGGLE;
        ledTime = curTime + 100;

        // set the can_id once to save flash space
        #if CAN_EFF
          canMsg.can_id = CAN_ID_MCU_TO_REMOTE | CAN_EFF_FLAG;
        #else
          canMsg.can_id = CAN_ID_MCU_TO_REMOTE;
        #endif

        if (!flashing) {
          // we are not in bootloading mode... only halde flash init messages
          if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_INIT
            && canMsg.data[4] == SIGNATURE_0
            && canMsg.data[5] == SIGNATURE_1
            && canMsg.data[6] == SIGNATURE_2) {
            // init bootloading mode

            flashing = true;

            // send flash ready message
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READY;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
            canMsg.data[4] = (flashAddr >> 24) & 0xFF;
            canMsg.data[5] = (flashAddr >> 16) & 0xFF;
            canMsg.data[6] = (flashAddr >> 8) & 0xFF;
            canMsg.data[7] = flashAddr & 0xFF;
            mcp2515.sendMessage(&canMsg);

          }

        } else {
          // we are in flashing mode...
          if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_ERASE) {
            // erase flash
            flashAddr = 0;
            do {
              boot_page_erase(flashAddr);
              boot_spm_busy_wait();
              flashAddr += SPM_PAGESIZE;
            } while (flashAddr < FLASHEND_BL);

            memset(flashBuffer, 0xFF, SPM_PAGESIZE);
            flashBufferDataCount = 0;
            flashPage = 0;
            flashBufferPos = 0;
            flashAddr = 0;

            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READY;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
            canMsg.data[4] = (flashAddr >> 24) & 0xFF;
            canMsg.data[5] = (flashAddr >> 16) & 0xFF;
            canMsg.data[6] = (flashAddr >> 8) & 0xFF;
            canMsg.data[7] = flashAddr & 0xFF;
            mcp2515.sendMessage(&canMsg);

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_READ) {
            // read flash memory at given address
            uint32_t readFlashAddr = (uint32_t)canMsg.data[7] + ((uint32_t)canMsg.data[6] << 8) + ((uint32_t)canMsg.data[5] << 16) + ((uint32_t)canMsg.data[4] << 24);

            if (readFlashAddr > FLASHEND_BL) {
              // flash read after flash end
              canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READ_ADDRESS_ERROR;
              canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
              canMsg.data[4] = ((uint32_t)FLASHEND_BL >> 24) & 0xFF;
              canMsg.data[5] = ((uint32_t)FLASHEND_BL >> 16) & 0xFF;
              canMsg.data[6] = ((uint32_t)FLASHEND_BL >> 8) & 0xFF;
              canMsg.data[7] = FLASHEND_BL & 0xFF;
              mcp2515.sendMessage(&canMsg);
              continue;
            }

            uint8_t len = 0;
            for (uint8_t i = 0; i < 4; i++) {
              if (readFlashAddr + i <= FLASHEND_BL) {
                // in flash area
                canMsg.data[4 + i] = pgm_read_byte_near(readFlashAddr + i);
                len++;
              } else {
                // not in flash area
                canMsg.data[4 + i] = 0x00;
              }
            }

            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READ_DATA;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = (len << 5) | (readFlashAddr & 0b00011111);  // number of data bytes read and address part
            mcp2515.sendMessage(&canMsg);

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_SET_ADDRESS) {
            // set the start address for flashing
            uint32_t newFlashAddr = (uint32_t)canMsg.data[7] + ((uint32_t)canMsg.data[6] << 8) + ((uint32_t)canMsg.data[5] << 16) + ((uint32_t)canMsg.data[4] << 24);
            uint16_t newFlashPage = newFlashAddr / SPM_PAGESIZE;
            uint16_t newFlashBufferPos = newFlashAddr % SPM_PAGESIZE;

            if (newFlashAddr > FLASHEND_BL) {
              // address cannot be flashed
              canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_ADDRESS_ERROR;
              canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
              canMsg.data[4] = ((uint32_t)FLASHEND_BL >> 24) & 0xFF;
              canMsg.data[5] = ((uint32_t)FLASHEND_BL >> 16) & 0xFF;
              canMsg.data[6] = ((uint32_t)FLASHEND_BL >> 8) & 0xFF;
              canMsg.data[7] = FLASHEND_BL & 0xFF;
              mcp2515.sendMessage(&canMsg);
              continue;
            }

            if (newFlashPage != flashPage && flashBufferDataCount > 0) {
              // new flash page and data in buffer to write to last flash page...
              // write data to flash page
              writeFlashPage();
            }

            flashAddr = newFlashAddr;
            flashPage = newFlashPage;
            flashBufferPos = newFlashBufferPos;

            // send flash ready
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READY;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = flashAddr & 0b00011111; // no data bytes written, just address part
            canMsg.data[4] = (flashAddr >> 24) & 0xFF;
            canMsg.data[5] = (flashAddr >> 16) & 0xFF;
            canMsg.data[6] = (flashAddr >> 8) & 0xFF;
            canMsg.data[7] = flashAddr & 0xFF;
            mcp2515.sendMessage(&canMsg);

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_DATA) {
            // data for flashing

            // check address part (lower 5 bits)
            if ((flashAddr & 0b00011111) != (canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] & 0b00011111)) {
              // send flash data error with the exprected flash address
              canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_DATA_ERROR;
              canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = flashAddr & 0b00011111; // no data bytes written, just address part
              canMsg.data[4] = (flashAddr >> 24) & 0xFF;
              canMsg.data[5] = (flashAddr >> 16) & 0xFF;
              canMsg.data[6] = (flashAddr >> 8) & 0xFF;
              canMsg.data[7] = flashAddr & 0xFF;
              mcp2515.sendMessage(&canMsg);
              continue;
            }

            // data length can be up to 4 bytes
            uint8_t len = (canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] >> 5);
            if ((flashAddr + len - 1) > FLASHEND_BL) {
              // address cannot be flashed
              canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_ADDRESS_ERROR;
              canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
              canMsg.data[4] = ((uint32_t)FLASHEND_BL >> 24) & 0xFF;
              canMsg.data[5] = ((uint32_t)FLASHEND_BL >> 16) & 0xFF;
              canMsg.data[6] = ((uint32_t)FLASHEND_BL >> 8) & 0xFF;
              canMsg.data[7] = FLASHEND_BL & 0xFF;
              mcp2515.sendMessage(&canMsg);
              continue;
            }
            for (uint8_t i = 0; i < len; i++) {
              flashBuffer[flashBufferPos++] = canMsg.data[4 + i];
              flashBufferDataCount++;
              flashAddr++;
              if (flashBufferPos >= SPM_PAGESIZE) {
                // flash page is full... write it!
                writeFlashPage();
              }
            }

            // send flash ready
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_READY;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = (len << 5) | (flashAddr & 0b00011111);  // number of data bytes written and address part
            canMsg.data[4] = (flashAddr >> 24) & 0xFF;
            canMsg.data[5] = (flashAddr >> 16) & 0xFF;
            canMsg.data[6] = (flashAddr >> 8) & 0xFF;
            canMsg.data[7] = flashAddr & 0xFF;
            mcp2515.sendMessage(&canMsg);

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_DONE) {
            // flashing done...
            if (flashBufferDataCount > 0) {
              // still data in flash buffer... write last page
              writeFlashPage();
            }

            // send start app
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_START_APP;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
            canMsg.data[4] = 0x00;
            canMsg.data[5] = 0x00;
            canMsg.data[6] = 0x00;
            canMsg.data[7] = 0x00;
            mcp2515.sendMessage(&canMsg);

            // delay for 50ms to let the mcp send the message
            delay(50);

            // write value of local mcusr into R2
            #if MCUSR_TO_R2
              __asm__ __volatile__("  mov r2,%[mcusr_val] ;Move Between Registers \n\t"
                         ::[mcusr_val] "r" (mcusr));
            #endif

            // start the main application
            startApp();

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_FLASH_DONE_VERIFY) {
            // flashing done and verify requested...
            if (flashBufferDataCount > 0) {
              // still data in flash buffer... write last page
              writeFlashPage();
            }

            // send flash done verify back
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_FLASH_DONE_VERIFY;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
            canMsg.data[4] = 0x00;
            canMsg.data[5] = 0x00;
            canMsg.data[6] = 0x00;
            canMsg.data[7] = 0x00;
            mcp2515.sendMessage(&canMsg);

          } else if (canMsg.data[CAN_DATA_BYTE_CMD] == CMD_START_APP) {
            // just start the main application now
            canMsg.data[CAN_DATA_BYTE_CMD]          = CMD_START_APP;
            canMsg.data[CAN_DATA_BYTE_LEN_AND_ADDR] = 0x00;
            canMsg.data[4] = 0x00;
            canMsg.data[5] = 0x00;
            canMsg.data[6] = 0x00;
            canMsg.data[7] = 0x00;
            mcp2515.sendMessage(&canMsg);

            // delay for 50ms to let the mcp send the message
            delay(50);

            // write value of local mcusr into R2
            #if MCUSR_TO_R2
              __asm__ __volatile__("  mov r2,%[mcusr_val] ;Move Between Registers \n\t"
                         ::[mcusr_val] "r" (mcusr));
            #endif

            // start the main application
            startApp();
          }
        }

      }
    }
  }
}

/**
 * Write data from current global flash buffer to the flash page.
 * After writing the global flash buffer will be emptied, the buffer position
 * will be reset to 0 and the flash page will be increased.
 */
void writeFlashPage () {
  boot_program_page(flashPage, flashBuffer);
  memset(flashBuffer, 0xFF, SPM_PAGESIZE);
  flashBufferDataCount = 0;
  flashPage++;
  flashBufferPos = 0;
}

/**
 * Write data from buffer to a flash page.
 * @param page Flash page number to write to.
 * @param buf  Buffer containing the flash data for that page.
 */
void boot_program_page (uint16_t page, uint8_t *buf) {
  uint16_t i;
  uint8_t sreg;

  uint32_t addr = ((uint32_t)page) * SPM_PAGESIZE; // type cast of `page` to support addresses bigger than 0xFFFF

  // Disable interrupts
  sreg = SREG;
  cli();

  eeprom_busy_wait();

  boot_page_erase(addr);
  boot_spm_busy_wait(); // Wait until the memory is erased

  for (i=0; i<SPM_PAGESIZE; i+=2) {
    // Set up little-endian word
    uint16_t w = *buf++;
    w += (*buf++) << 8;

    boot_page_fill(addr + i, w);
  }

  boot_page_write(addr); // Store buffer in flash page
  boot_spm_busy_wait(); // Wait until the memory is written

  // Reenable RWW-section again. We need this if we want to jump back
  // to the application after bootloading.
  boot_rww_enable();

  // Re-enable interrupts (if they were ever enabled)
  SREG = sreg;
}

/**
 * Cleanup and start the main application.
 */
void startApp () {

  // reset SPI interface to power-up state
  SPCR = 0;
  SPSR = 0;

  // reset SPI pins to input
  SPI_DDR = 0;

  // disable timer 0 overflow interrupt
  #if defined(TIMSK) && defined(TOIE0)
    TIMSK &= ~(1<<TOIE0);
  #elif defined(TIMSK0) && defined(TOIE0)
    TIMSK0 &= ~(1<<TOIE0);
  #else
    #error Timer 0 overflow interrupt not disabled correctly
  #endif

  // reset interrupt vectors
  cli();
  uint8_t ivtemp = IV_REG;
  IV_REG = ivtemp | (1<<IVCE);
  IV_REG = ivtemp & ~(1<<IVSEL);

  // turn off LED and reset pin to input
  LED_OFF;
  LED_DEINIT;

  // use EIND register if available to jump back to 0x0000 for application start
  #ifdef EIND
    EIND = 0;
  #endif

  // jump to main application
  gotoApp();
}
