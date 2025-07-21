#include "crc16.h"

uint16_t crc16_ccitt(const uint8_t *buf, uint16_t size, const uint16_t start)
{
    uint16_t crc = start;
    //while (--size >= 0) {
    while (size--) {
        int i;
        crc ^= (uint16_t) *buf++ << 8;
        for (i = 0; i < 8; i++)
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc <<= 1;
    }
    return crc;
}

