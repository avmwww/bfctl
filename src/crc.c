/*
 * CRC
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include "crc.h"

static inline uint8_t crc8_calc(uint8_t crc, uint8_t a, uint8_t poly)
{
	int i;

	crc ^= a;
	for (i = 0; i < 8; i++) {
		if (crc & 0x80)
			crc = (crc << 1) ^ poly;
		else
			crc = crc << 1;
	}
	return crc;
}

uint8_t crc8_cal_buf(const void *data, int len, uint8_t poly)
{
	int i;
	uint8_t crc = 0;
	const uint8_t *p = data;

	for (i = 0; i < len; i++)
		crc = crc8_calc(crc, *p++, poly);

	return crc;
}

static uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
	int i;

	crc = crc ^ ((uint16_t)data << 8);
	for (i = 0; i < 8; i++) {
		if (crc & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc <<= 1;
	}
	return crc;
}

uint16_t crc_xmodem_cal_buf(const void *data, int len, uint16_t crc)
{
	int i;
	const uint8_t *p = data;

	for (i = 0; i < len; i++)
		crc = crc_xmodem_update(crc, *p++);

	return crc;
}

