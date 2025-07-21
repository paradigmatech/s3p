#ifndef _CRC16_H
#define _CRC16_H

#include <stdint.h>

// SPI-FUJITSU, AUG-CCITT
#define     CRC_START_CCITT_1D0F    0x1D0F
// IBM-3740, AUTOSAR, CCITT-FALSE
#define     CRC_START_CCITT_FFFF    0xFFFF
// XMODEM, ZMODEM, ACORN, LTE, V-41-MSB
#define     CRC_START_XMODEM        0x0000

extern uint16_t crc16_ccitt(const uint8_t *buf, uint16_t size, const uint16_t start);

#endif // _CRC16_H

