/*
 * crc
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _CRC_H_
#define _CRC_H_

#include <stdint.h>

uint8_t crc8_cal_buf(const void *data, int len, uint8_t poly);

uint16_t crc_xmodem_cal_buf(const void *data, int len, uint16_t crc);

#endif
