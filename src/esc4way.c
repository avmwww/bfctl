/*
 * esc4way interface
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "zalloc.h"
#include "dump_hex.h"

#include "serial.h"
#include "esc4way.h"
#include "esc_boot.h"
#include "crc.h"

//#define DEBUG


typedef struct esc4way_hdr {
	uint8_t esc;
	uint8_t cmd;
	union {
		uint8_t byte[2];
		uint16_t word;
	} __attribute__((__packed__)) addr;
	uint8_t len;
} __attribute__((__packed__)) esc4way_hdr_t;

typedef struct esc4way_pkt {
	esc4way_hdr_t hdr;
	uint8_t data[0];
} __attribute__((__packed__)) esc4way_pkt_t;

static const char *esc4way_ack_str(int ack)
{
	switch (ack) {
		case ACK_OK:
			return "OK";
		case ACK_I_INVALID_CMD:
			return "invalid CMD";
		case ACK_I_INVALID_CRC:
			return "invalid CRC";
		case ACK_I_VERIFY_ERROR:
			return "verify error";
		case ACK_I_INVALID_CHANNEL:
			return "invalid channel";
		case ACK_I_INVALID_PARAM:
			return "invalid param";
		case ACK_D_GENERAL_ERROR:
			return "general error";
		default:
			return "unknown";
	}
}


#ifdef DEBUG
static void esc4way_dump_reply(const esc4way_pkt_t *pkt)
{
	printf("CMD: %02x\n", pkt->hdr.cmd);
	printf("ADDR: %02x%02x\n", pkt->hdr.addr.byte[0], pkt->hdr.addr.byte[1]);
	printf("LEN: %02x\n", pkt->hdr.len);
	printf("ACK: %02x, %s\n", pkt->data[pkt->hdr.len], esc4way_ack_str(pkt->data[pkt->hdr.len]));
	printf("CRC: %02x%02x\n", pkt->data[pkt->hdr.len + 1], pkt->data[pkt->hdr.len + 2]);
}

#define debug		printf
#define debug_dump	dump_hex
#else
static void esc4way_dump_reply(const esc4way_pkt_t *pkt)
{
	(void)pkt;
}
#define debug(...)
#define debug_dump(...)
#endif


static int esc4way_write(serial_handle fd, const void *data, int len)
{
	debug("esc4way out:\n");
	debug_dump(data, len, 0);

	if (serial_write(fd, data, len) != len)
		return -1;
	return 0;
}

static int esc4way_read(serial_handle fd, void *data, int len)
{
	int n;

	if ((n = serial_read(fd, data, len)) != len) {
		debug("Request read %d, but read %d\n", len, n);
		return -1;
	}
	debug("esc4way in:\n");
	debug_dump(data, n, 1);
	return n;
}

int esc4way_send(esc4way_t *esc, int cmd, int addr, const void *out, int out_len, void *in, int in_len)
{
	uint8_t data[sizeof(esc4way_hdr_t) + out_len + sizeof(uint16_t)];
	uint8_t rd[256 + 3 + sizeof(esc4way_hdr_t)];
	esc4way_pkt_t *pkt = (esc4way_pkt_t *)data;
	esc4way_hdr_t *hdr = &pkt->hdr;
	uint16_t crc, rd_crc;
	int len;

	if (!out || !out_len) {
		out_len = 0;
		out = NULL;
	}

	hdr->esc = cmd_Local_Escape;
	hdr->cmd = cmd;
	hdr->addr.byte[0] = addr >> 8;
	hdr->addr.byte[1] = addr;
	hdr->len = out_len;

	memcpy(pkt->data, out, out_len);

	crc = crc_xmodem_cal_buf(data, sizeof(esc4way_hdr_t) + out_len, 0);

	pkt->data[out_len] = crc >> 8;
	pkt->data[out_len + 1] = crc;
	if (esc4way_write(esc->fd, data, sizeof(esc4way_hdr_t) + out_len + sizeof(uint16_t)) < 0)
		return -1;

	if (!in || !in_len) {
		in_len = 0;
		in = NULL;
	}

	/* read reply header */
	pkt = (esc4way_pkt_t *)rd;

	if (esc4way_read(esc->fd, pkt, sizeof(esc4way_pkt_t)) < 0)
		return -1;

	/* read data + ack + crc */
	len = pkt->hdr.len;
	if (len == 0)
		len = 256;

	if (esc4way_read(esc->fd, pkt->data, len + 3) < 0)
		return -1;

	esc4way_dump_reply(pkt);

	crc = crc_xmodem_cal_buf(pkt, sizeof(esc4way_hdr_t) + len + 1, 0);
	rd_crc = ((uint16_t)pkt->data[len + 1] << 8) | pkt->data[len + 2];

	if (rd_crc != crc) {
		debug("Invalid CRC %04x, should %04x\n", rd_crc, crc);
		return -1;
	}

	if (in) {
		if (len > in_len)
			len = in_len;
		memcpy(in, pkt->data, len);
	}

	if (pkt->data[len] != ACK_OK) {
		printf("ack: %s\n", esc4way_ack_str(pkt->data[len]));
		return -1;
	}

	return len;
}

int esc4way_set_version(esc4way_t *esc, int major, int minor)
{
	if (!esc->set.cached) {
		if (esc4way_settings_cache(esc) < 0)
			return -1;
	}

	esc->set.data.name.version.major = major;
	esc->set.data.name.version.minor = minor;

	return esc4way_write_flash(esc, esc->set.addr, esc->set.data.byte, esc->set.size);
}

int esc4way_boot_flash(esc4way_t *esc, int mark)
{
	if (!esc->set.cached) {
		if (esc4way_settings_cache(esc) < 0)
			return -1;
	}

	esc->set.data.name.head = mark;

	return esc4way_write_flash(esc, esc->set.addr, esc->set.data.byte, esc->set.size);
}

int esc4way_mark_flash(esc4way_t *esc, const char *mark)
{
	int len;

	len = strlen(mark);
	if (len > ESC_SET_DEVICE_NAME_SIZE)
		len = ESC_SET_DEVICE_NAME_SIZE;

	if (!esc->set.cached) {
		if (esc4way_settings_cache(esc) < 0)
			return -1;
	}

	memset(esc->set.data.name.device_name, 0, ESC_SET_DEVICE_NAME_SIZE);
	memcpy(esc->set.data.name.device_name, mark, len);

	return esc4way_write_flash(esc, esc->set.addr, esc->set.data.byte, esc->set.size);
}

int esc4way_select_chan(esc4way_t *esc, int addr, int chan)
{
	uint8_t c = chan;
	uint8_t dev_info[4];
	int err;
	/* uint8_t connected;  // > 0
	 * uint8_t ;
	 * uint8_t ;
	 * uint8_t mode; // imC2 0, imSIL_BLB 1, imATM_BLB 2, imSK 3, imARM_BLB 4
	 * */

	err = esc4way_send(esc, cmd_DeviceInitFlash, addr, &c, 1, &dev_info, sizeof(dev_info));
	debug("esc4way device info\n");
	debug_dump(dev_info, sizeof(dev_info), 1);
	return err;
}

int esc4way_write_flash(esc4way_t *esc, int addr, const void *buf, int len)
{
	const uint8_t *p = buf;
	int n;

	while (len > 0) {
		if (len > 256)
			n = 256;
		else
			n = len;

		if (esc4way_send(esc, cmd_DeviceWrite, addr, p, n, NULL, 0) < 0)
			return -1;

		len -= n;
		p += n;
		addr += n;
	}
	return 0;

}

int esc4way_read_flash(esc4way_t *esc, int addr, void *buf, int len)
{
	uint8_t num;
	uint8_t *p = buf;
	int n;

	while (len > 0) {
		if (len > 256)
			n = 256;
		else
			n = len;

		num = n;
		if (esc4way_send(esc, cmd_DeviceRead, addr, &num, 1, p, n) < 0)
			return -1;

		len -= n;
		p += n;
		addr += n;
	}
	return 0;
}

int esc4way_settings_cache(esc4way_t *esc)
{
	int err;
	err = esc4way_read_flash(esc, esc->set.addr, &esc->set.data.name, sizeof(esc->set.data.name));
	if (err < 0)
		return err;

	esc->set.cached = true;
	return 0;
}

int esc4way_interface_name(esc4way_t *esc, char *name, int len)
{
	uint8_t data = 0;
	name[0] = '\0';
	return esc4way_send(esc, cmd_InterfaceGetName, 0, &data, 1, name, len);
}

int esc4way_exit(esc4way_t *esc)
{
	uint8_t data = 0;
	return esc4way_send(esc, cmd_InterfaceExit, 0, &data, 1, NULL, 0);
}

int esc4way_reset(esc4way_t *esc, int chan)
{
	uint8_t data = chan;
	return esc4way_send(esc, cmd_DeviceReset, 0, &data, 1, NULL, 0);
}

esc4way_t *esc4way_init(serial_handle fd)
{
	esc4way_t *esc;

	esc = zalloc(sizeof(esc4way_t));
	if (!esc)
		return NULL;

	esc->fd = fd;
	esc->set.addr = ESC_FLASH_SETTINGS_OFFT;
	esc->set.size = ESC_FLASH_SETTINGS_SIZE;
	esc->fw.addr = ESC_FLASH_FIRMWARE_OFFT;
	return esc;
}

