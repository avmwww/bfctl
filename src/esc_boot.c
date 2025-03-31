/*
 * esc_boot
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "serial.h"
#include "dump_hex.h"
#include "crc.h"
#include "esc_boot.h"

static uint16_t crc_calc(const void *buf, int len)
{
	int i;
	uint16_t crc;
	const uint8_t *p = (uint8_t *)buf;

	crc = 0;

	while (len-- > 0) {
		uint8_t b = *p++;
		for (i = 0; i < 8; i++) {
			if (((b & 0x01) ^ (crc & 0x0001)) != 0 ) {
				crc >>= 1;
				crc ^= 0xa001;
			} else {
				crc >>= 1;
			}
			b >>= 1;
		}
	}
	return crc;
}

int esc_boot_send(serial_handle fd, const void *buf, int len)
{
	uint16_t crc = crc_calc(buf, len);

	if (serial_write(fd, buf, len) != len)
		return -1;
	if (serial_write(fd, &crc, 2) != 2)
		return -1;

	return len + 2;
}


