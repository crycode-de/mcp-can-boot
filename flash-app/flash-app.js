const fs = require('fs');
const path = require('path');
const yargs = require('yargs');
const socketcan = require('socketcan');
const MemoryMap = require('nrf-intel-hex');

const BOOTLOADER_CMD_VERSION = 0x01;

const CAN_DATA_BYTE_MCU_ID_MSB   = 0
const CAN_DATA_BYTE_MCU_ID_LSB   = 1
const CAN_DATA_BYTE_CMD          = 2
const CAN_DATA_BYTE_LEN_AND_ADDR = 3

const CAN_ID_MCU_TO_REMOTE_DEFAULT = 0x1FFFFF01
const CAN_ID_REMOTE_TO_MCU_DEFAULT = 0x1FFFFF02

const CMD_ERROR                    = 0b00000001
const CMD_BOOTLOADER_START         = 0b00000010
const CMD_FLASH_INIT               = 0b00000110 // remote -> mcu
const CMD_FLASH_READY              = 0b00000100 // mcu -> remote
const CMD_FLASH_SET_ADDRESS        = 0b00001010 // remote -> mcu
const CMD_FLASH_ADDRESS_ERROR      = 0b00001011 // mcu -> remote
const CMD_FLASH_DATA               = 0b00001000 // remote -> mcu
const CMD_FLASH_DATA_ERROR         = 0b00001101 // mcu -> remote
const CMD_FLASH_DONE               = 0b00010000 // remote -> mcu
const CMD_FLASH_DONE_VERIFY        = 0b01010000 // remote <-> mcu
const CMD_FLASH_ERASE              = 0b00100000 // remote -> mcu
const CMD_FLASH_READ               = 0b01000000 // remote -> mcu
const CMD_FLASH_READ_DATA          = 0b01001000 // mcu -> remote
const CMD_FLASH_READ_ADDRESS_ERROR = 0b01001011 // mcu -> remote
const CMD_START_APP                = 0b10000000 // mcu <-> remote

const STATE_INIT     = 0;
const STATE_FLASHING = 1;
const STATE_READING  = 2;

class FlashApp {

  constructor () {
    this.args = yargs
      .locale('en')

      .option('file', {
        alias: 'f',
        description: 'Hex file to flash',
        type: 'string',
        demandOption: true,
        requiresArg: true
      })

      .option('iface', {
        alias: 'i',
        description: 'CAN interface to use',
        type: 'string',
        default: 'can0',
        requiresArg: true
      })

      .option('partno', {
        alias: 'p',
        description: 'Specific AVR device like in avrdude',
        type: 'string',
        demandOption: true,
        requiresArg: true
      })

      .option('mcuid', {
        alias: 'm',
        description: 'ID of the MCU bootloader',
        type: 'string',
        demandOption: true,
        requiresArg: true,
        coerce: this.parseNumber
      })

      .option('e', {
        description: 'Erase whole flash before flashing new data',
        type: 'boolean'
      })

      .option('V', {
        description: 'Do not verify',
        type: 'boolean'
      })

      .option('r', {
        description: 'Read flash and save to given file (no flashing!), optional with maximum address to read until',
        type: 'string',
        coerce: this.parseNumber
      })

      .option('F', {
        description: 'Force flashing, even if the bootloader version missmatched',
        type: 'boolean'
      })

      .option('can-id-mcu', {
        description: 'CAN-ID for messages from MCU to remote',
        type: 'string',
        default: CAN_ID_MCU_TO_REMOTE_DEFAULT,
        requiresArg: true,
        coerce: this.parseNumber
      })

      .option('can-id-remote', {
        description: 'CAN-ID for messages from remote to MCU',
        type: 'string',
        default: CAN_ID_REMOTE_TO_MCU_DEFAULT,
        requiresArg: true,
        coerce: this.parseNumber
      })

      .help()
      .version(false)
      .alias('help', 'h')
      .argv;

    this.mcuId = [(this.args.mcuid >> 8) & 0xFF, this.args.mcuid & 0xFF];

    this.doErase = !!this.args.e;
    this.doRead = (this.args.r !== undefined) ? true : false;

    this.doVerify = this.doRead ? false : !this.args.V; // if we are just reading, we cannot verify

    this.state = STATE_INIT;
    this.deviceSignature = this.getDeviceSignature(this.args.partno);

    if (!this.doRead) {
      // load from file if we are not only reading the flash
      const intelHexString = fs.readFileSync(this.args.file, 'latin1');
      this.memMap = MemoryMap.fromHex(intelHexString);

    } else {
      // we are only reading the flash... init an empty memory map
      this.memMap = new MemoryMap();

      // check if output file exists
      if (this.args.file !== '-' && fs.existsSync(this.args.file)) {
        console.log(`Output file ${this.args.file} already exists!`);
        process.exit(1);
      }
    }

    this.memMapKeys = this.memMap.keys(); // load all keys of the memory map
    this.memMapCurrentKey = null; // set current key to null to begin new key on flash ready
    this.memMapCurrentDataIdx = 0;

    this.curAddr = 0x0000; // current flash address

    this.readDataArr = [];

    this.can = socketcan.createRawChannel(this.args.iface, true);
    this.can.addListener('onMessage', this.handleCanMsg.bind(this));
    this.can.start();

    console.log(`Waiting for bootloader start message for MCU ID ${this.hexString(this.args.mcuid, 4)} ...`);
  }

  handleCanMsg (msg) {
    if (msg.data.length !== 8) return;
    if (msg.id !== this.args.canIdMcu) return;

    const mcuid = msg.data[CAN_DATA_BYTE_MCU_ID_LSB] + (msg.data[CAN_DATA_BYTE_MCU_ID_MSB] << 8);

    if (mcuid !== this.args.mcuid) return;

    // the message is for this bootloader session

    switch (this.state) {
      case STATE_INIT:
        switch (msg.data[CAN_DATA_BYTE_CMD]) {
          case CMD_BOOTLOADER_START:
            // check device signature
            if (msg.data[4] !== this.deviceSignature[0] || msg.data[5] !== this.deviceSignature[1] || msg.data[6] !== this.deviceSignature[2]) {
              console.log(`Error: Got bootloader start message but device signature missmatched!`);
              console.log(`Expected ${this.hexString(this.deviceSignature[0])} ${this.hexString(this.deviceSignature[1])} ${this.hexString(this.deviceSignature[2])} for ${this.args.partno}, got ${this.hexString(msg.data[4])} ${this.hexString(msg.data[5])} ${this.hexString(msg.data[6])}`);
              return;
            }

            // check bootloader version
            if (msg.data[7] !== BOOTLOADER_CMD_VERSION) {
              if (this.args.F) {
                console.warn(`WARNING: Bootloader command version of MCU ${this.hexString(msg.data[7])} does not match the version expected by this flash app ${this.hexString(BOOTLOADER_CMD_VERSION)}. You forced flashing anyways. This may lead to an stupid result...`);
              } else {
                console.warn(`ERROR: Bootloader command version of MCU ${this.hexString(msg.data[7])} does not match the version expected by this flash app ${this.hexString(BOOTLOADER_CMD_VERSION)}. To force flashing use the -F argument.`);
                return;
              }
            }

            // enter flash mode
            console.log('Got bootloader start, entering flash mode ...');
            this.flashStartTs = Date.now();
            this.can.send({
              id: this.args.canIdRemote,
              ext: true,
              rtr: false,
              data: Buffer.from([
                this.mcuId[0],
                this.mcuId[1],
                CMD_FLASH_INIT,
                0x00,
                this.deviceSignature[0],
                this.deviceSignature[1],
                this.deviceSignature[2],
                0x00
              ])
            });
            break;

          case CMD_FLASH_READY:
            // flash is ready for first data, read or erase...
            if (this.doRead) {
              console.log('Got flash ready message, reading flash ...');
              this.state = STATE_READING;
              this.can.send({
                id: this.args.canIdRemote,
                ext: true,
                rtr: false,
                data: Buffer.from([
                  this.mcuId[0],
                  this.mcuId[1],
                  CMD_FLASH_READ,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00
                ])
              });
            } else if (this.doErase) {
              console.log('Got flash ready message, erasing flash ...');
              this.can.send({
                id: this.args.canIdRemote,
                ext: true,
                rtr: false,
                data: Buffer.from([
                  this.mcuId[0],
                  this.mcuId[1],
                  CMD_FLASH_ERASE,
                  0x00,
                  0x00,
                  0x00,
                  0x00,
                  0x00
                ])
              });
              this.doErase = false;
            } else {
              console.log('Got flash ready message, begin flashing ...');
              this.state = STATE_FLASHING;
              this.onFlashReady(msg.data);
            }
            break;

          default:
            // something wrong?
            console.warn(`WARNING: Got unexpected message from MCU: ${this.hexString(msg.data[CAN_DATA_BYTE_CMD])}`);
        }

        break;

      case STATE_FLASHING:

        switch (msg.data[CAN_DATA_BYTE_CMD]) {
          case CMD_FLASH_DATA_ERROR:
            console.log('Flash data error!');
            console.log('Maybe there are some CAN bus issues?');
            break;

          case CMD_FLASH_ADDRESS_ERROR:
            console.log('Flash address error!');
            console.log('Maybe the hex file is not for this MCU type?');
            break;

          case CMD_FLASH_READY:
            const bytesFlashed = (msg.data[CAN_DATA_BYTE_LEN_AND_ADDR] >> 5);
            //console.log(`${bytesFlashed} bytes flashed`);
            this.curAddr += bytesFlashed;
            this.memMapCurrentDataIdx += bytesFlashed;
            this.onFlashReady(msg.data);
            break;

          case CMD_START_APP:
            console.log(`Flash done in ${(Date.now() - this.flashStartTs)} ms.`);
            console.log('MCU is starting the app. :-)');
            process.exit(0);
            break;

          default:
            // something wrong?
            console.warn(`WARNING: Got unexpected message from MCU: ${this.hexString(msg.data[CAN_DATA_BYTE_CMD])}`);
        }
        break;

      case STATE_READING:
        switch (msg.data[CAN_DATA_BYTE_CMD]) {
          case CMD_FLASH_DONE_VERIFY:
            // start reading flash to verify
            console.log('Start reading flash to verify ...');
            // TODO
            this.memMapKeys = this.memMap.keys(); // load all keys of the memory map
            this.memMapCurrentKey = null; // set current key to null to begin new key on flash read
            this.memMapCurrentDataIdx = 0;

            this.readForVerify ();

            break;

          case CMD_FLASH_READ_DATA:
            const bytesRead = (msg.data[CAN_DATA_BYTE_LEN_AND_ADDR] >> 5);
            const readAddrPart = msg.data[CAN_DATA_BYTE_LEN_AND_ADDR] & 0b00011111;

            if (this.curAddr & 0b00011111 !== readAddrPart) {
              console.log('Got an unexpected address of read data from MCU!');
              console.log('Will now abort and exit the bootloader ...');
              this.sendStartApp();
              return;
            }

            console.log(`Got flash data for ${this.hexString(this.curAddr, 4)} ...`);

            if (this.doVerify) {
              // verify flash
              for (let i = 0; i < bytesRead; i++) {
                if (this.memMap.get(this.memMapCurrentKey)[this.memMapCurrentDataIdx] !== undefined
                  && this.memMap.get(this.memMapCurrentKey)[this.memMapCurrentDataIdx] !== msg.data[4+i]) {
                  console.log(`ERROR: Verify failed at ${this.hexString(this.curAddr)}!`);
                  console.log('Trying to start the app nevertheless ...');
                  this.sendStartApp();
                  return;
                }
                this.curAddr++;
                this.memMapCurrentDataIdx++;
              }

              this.readForVerify();

            } else {
              // read whole flash
              // cache the data
              for (let i = 0; i < bytesRead; i++) {
                this.readDataArr.push(msg.data[4+i]);
                this.curAddr++;
              }

              if (this.args.r > 0 && this.curAddr > this.args.r) {
                // reached max read address...
                this.readDone();
                return;
              }
              // request next address
              this.can.send({
                id: this.args.canIdRemote,
                ext: true,
                rtr: false,
                data: Buffer.from([
                  this.mcuId[0],
                  this.mcuId[1],
                  CMD_FLASH_READ,
                  0x00,
                  (this.curAddr >> 24) & 0xFF,
                  (this.curAddr >> 16) & 0xFF,
                  (this.curAddr >> 8) & 0xFF,
                  this.curAddr & 0xFF
                ])
              });
            }


            break;

          case CMD_FLASH_READ_ADDRESS_ERROR:
            // we hit the end of the flash
            if (this.doVerify) {
              // hitting the end at verify must be an error...
              console.log(`ERROR: Reading flash failed during verify!`);
              this.sendStartApp();
              return;
            } else {
              // when reading whole flash this is expected
              this.readDone();
            }

            break;

          case CMD_START_APP:
            console.log('MCU ist starting the app. :-)');
            process.exit(0);
            break;

          default:
            // something wrong?
            console.warn(`WARNING: Got unexpected message from MCU: ${this.hexString(msg.data[CAN_DATA_BYTE_CMD])}`);
        }
        break;
    }
  }

  readForVerify () {
    // check memory map and get next map key if we reached the end
    if (!this.memMap.get(this.memMapCurrentKey) || this.memMap.get(this.memMapCurrentKey)[this.memMapCurrentDataIdx] === undefined) {
      // no more data... goto next memory map key...
      const key = this.memMapKeys.next();
      if (key.done) {
        // all keys done... verify complete
        console.log(`Flash and verify done in ${(Date.now() - this.flashStartTs)} ms.`);
        this.sendStartApp();
        return;
      }

      // apply new current address and set data index to 0
      this.memMapCurrentKey = key.value;
      this.memMapCurrentDataIdx = 0;
      this.curAddr = key.value;
    }

    // request next address
    this.can.send({
      id: this.args.canIdRemote,
      ext: true,
      rtr: false,
      data: Buffer.from([
        this.mcuId[0],
        this.mcuId[1],
        CMD_FLASH_READ,
        0x00,
        (this.curAddr >> 24) & 0xFF,
        (this.curAddr >> 16) & 0xFF,
        (this.curAddr >> 8) & 0xFF,
        this.curAddr & 0xFF
      ])
    });
  }

  readDone () {
    // create memory map
    const memMap = new MemoryMap();
    memMap.set(0x0000, Uint8Array.from(this.readDataArr));

    const intelHexString = memMap.asHexString();

    if (this.args.file === '-') {
      // write to stdout
      console.log('Read hex data:');
      console.log(intelHexString);
    } else {
      fs.writeFileSync(this.args.file, intelHexString, 'latin1');
      console.log(`Hex file written to ${this.args.file}.`);
    }

    console.log(`Reading flash done in ${Date.now() - this.flashStartTs} ms.`);

    // start the main application at the MCU
    this.sendStartApp();
  }

  sendStartApp () {
    console.log('Starting the app on the MCU ...');
    this.can.send({
      id: this.args.canIdRemote,
      ext: true,
      rtr: false,
      data: Buffer.from([
        this.mcuId[0],
        this.mcuId[1],
        CMD_START_APP,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
      ])
    });
  }

  onFlashReady (msgData) {
    const curAddrRemote = msgData[7] + (msgData[6] << 8) + (msgData[5] << 16) + (msgData[4] << 24);
    //console.log(`Remote flash address is ${this.hexString(curAddrRemote)}`);

    if (!this.memMap.get(this.memMapCurrentKey) || this.memMap.get(this.memMapCurrentKey)[this.memMapCurrentDataIdx] === undefined) {
      // no more data... goto next memory map key...
      const key = this.memMapKeys.next();
      if (key.done) {
        // all keys done... flash complete
        console.log('All data transmitted. Finalizing ...');
        if (this.doVerify) {
          // we want to verify... send flash done verify and set own state to read
          this.state = STATE_READING;
          this.can.send({
            id: this.args.canIdRemote,
            ext: true,
            rtr: false,
            data: Buffer.from([
              this.mcuId[0],
              this.mcuId[1],
              CMD_FLASH_DONE_VERIFY,
              0x00,
              0x00,
              0x00,
              0x00,
              0x00
            ])
          });

        } else {
          // we don't want to verify... send flash done to start the app
          this.can.send({
            id: this.args.canIdRemote,
            ext: true,
            rtr: false,
            data: Buffer.from([
              this.mcuId[0],
              this.mcuId[1],
              CMD_FLASH_DONE,
              0x00,
              0x00,
              0x00,
              0x00,
              0x00
            ])
          });
        }
        return;
      }

      // apply new current address and set data index to 0
      this.memMapCurrentKey = key.value;
      this.memMapCurrentDataIdx = 0;
      this.curAddr = key.value;
    }

    if (this.curAddr !== curAddrRemote) {
      // need to set the address to flash...
      console.log(`Setting flash address to ${this.hexString(this.curAddr, 4)} ...`);
      this.can.send({
        id: this.args.canIdRemote,
        ext: true,
        rtr: false,
        data: Buffer.from([
          this.mcuId[0],
          this.mcuId[1],
          CMD_FLASH_SET_ADDRESS,
          0x00,
          (this.curAddr >> 24) & 0xFF,
          (this.curAddr >> 16) & 0xFF,
          (this.curAddr >> 8) & 0xFF,
          this.curAddr & 0xFF
        ])
      });
      return;
    }

    // send data to flash...
    const data = Buffer.from([
      this.mcuId[0],
      this.mcuId[1],
      CMD_FLASH_DATA,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00
    ]);

    // add the 4 data bytes if available
    let dataBytes = 0;
    for (let i = 0; i < 4; i++) {
      const byte = this.memMap.get(this.memMapCurrentKey)[this.memMapCurrentDataIdx+i];
      if (byte === undefined) {
        break;
      }
      data[4+i] = byte;
      dataBytes++;
    }

    // set the number of bytes and address
    data[CAN_DATA_BYTE_LEN_AND_ADDR] = (dataBytes << 5) | (this.curAddr & 0b00011111);

    // send data
    console.log(`Sending flash data ${this.hexString(this.curAddr, 4)} ...`);
    this.can.send({
      id: this.args.canIdRemote,
      ext: true,
      rtr: false,
      data: data
    });
  }

  hexString (num, minLength) {
    let hex = num.toString(16);
    if (hex.length % 2 !== 0) {
      hex = '0' + hex;
    }
    if (minLength) {
      while (hex.length < minLength) {
        hex = '0' + hex;
      }
    }
    return '0x' + hex.toUpperCase();
  }

  parseNumber (val) {
    if (typeof(val) === 'string') {
      val = parseInt(val, val.startsWith('0x') ? 16 : 10);
    }
    return val;
  }

  getDeviceSignature (partno) {
    partno = partno.toLowerCase();
    switch (partno) {
      case 'm32':
      case 'mega32':
      case 'atmega32':
        return [0x1E, 0x95, 0x02];
      case 'm328':
      case 'mega328':
      case 'atmega328':
        return [0x1E, 0x95, 0x14];
      case 'm328p':
      case 'mega328p':
      case 'atmega328p':
        return [0x1E, 0x95, 0x0F];
      case 'm64':
      case 'mega64':
      case 'atmega64':
        return [0x1E, 0x96, 0x02];
      case 'm644p':
      case 'mega644p':
      case 'atmega644p':
        return [0x1E, 0x96, 0x0A];
      case 'm128':
      case 'mega128':
      case 'atmega128':
        return [0x1E, 0x97, 0x02];
      case 'm1284p':
      case 'mega1284p':
      case 'atmega1284p':
        return [0x1E, 0x97, 0x05];
      case 'm2560':
      case 'mega2560':
      case 'atmega2560':
        return [0x1E, 0x98, 0x01];
      default:
        return [0, 0, 0];
    }
  }
}

new FlashApp();
