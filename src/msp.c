/*
 * MSP serial interface
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include <string.h>
#include <stdio.h>

#include "msp.h"
#include "msp_serial.h"
#include "crc.h"
#include "dump_hex.h"

//#define DEBUG
#include "debug.h"
#include "failure.h"

//#define VERBOSE_DEBUG

#ifdef VERBOSE_DEBUG
# define verbose_msg	printf
# define vdump_hex	dump_hex
# define verrmsg_errno	errmsg_errno
#else
# define verbose_msg(...)
# define vdump_hex(...)
# define verrmsg_errno(...)
#endif

int msp_raw_transmit(serial_handle fd, const void *out, int out_size,
			void *in, int in_size)
{
	int len;

	if ((len = serial_write(fd, out, out_size)) < 0) {
		verrmsg_errno("Serial write faled %d", len);
		return -1;
	}
	verbose_msg("msp transmited %d\n", len);

	if ((len = serial_read(fd, in, in_size)) < 0) {
		verrmsg_errno("Serial read faled %d", len);
		return -1;
	}

	verbose_msg("msp received %d\n", len);
	return len;
}

int msp_transmit(serial_handle fd, uint16_t cmd, int dir, const void *out, int out_size,
		 void *in, int in_size)
{
	mspHeaderV2_t *mh;
	uint8_t *arg;
	uint8_t buf[256];
	int len = 0;
	uint8_t crc;

	buf[0] = '$';
	buf[1] = 'X';
	buf[2] = dir ? '<' : '>';
	len += 3;

	mh = (mspHeaderV2_t *)&buf[3];
	arg = &buf[3 + sizeof(mspHeaderV2_t)];

	mh->flags = 0;
	mh->cmd = cmd;
	memcpy(arg, out, out_size);
	mh->size = out_size;

	len += sizeof(mspHeaderV2_t) + mh->size;

	arg[mh->size] = crc8_cal_buf(mh, sizeof(mspHeaderV2_t) + mh->size, MSP_CRC_POLY);
	len += 1;

	verbose_msg("Write\n");
	vdump_hex(buf, len, 1);

	if ((len = msp_raw_transmit(fd, buf, len, buf, sizeof(buf))) < 1)
		return -1;

	verbose_msg("Read\n");
	vdump_hex(buf, len, 1);

	if (buf[0] != '$') {
		verbose_msg("Invalid reply header 0x%02x\n", buf[0]);
		return -1;
	}

	if (len != sizeof(mspHeaderV2_t) + mh->size + 1 + 3) {
		verbose_msg("Invalid reply len %d\n", len);
		return -1;
	}

	crc = crc8_cal_buf(mh, sizeof(mspHeaderV2_t) + mh->size, MSP_CRC_POLY);
	if (crc != arg[mh->size]) {
		verbose_msg("Invalid received crc 0x%02x, should 0x%02x\n",
			    arg[mh->size], crc);
		return -1;
	}

	if (in_size > mh->size)
		in_size = mh->size;

	if (in)
		memcpy(in, arg, in_size);
	else
		in_size = 0;

	return in_size;

}

int msp_cmd_transmit(serial_handle fd, const char *out, char *in, int in_size, int pr)
{
	int len;
	char temp_in[1024];
	char *pin = in;
	int sz;

	/* write out */
	verbose_msg("cmd send: %s\n", out);
	if ((len = serial_write(fd, out, strlen(out))) < 0)
		return -1;

	sz = in_size - 1;
	while (sz && (len = serial_read(fd, pin, sz)) > 0) {
		pin += len;
		sz -= len;
	}
	pin[0] = '\0';

	if (len < 0)
		return -1;

	verbose_msg("reply\n%s\n", in);
	vdump_hex(in, in_size - sz, 1);

	if (pr)
		printf("%s", in);

	/* flush serial input */
	while ((len = serial_read(fd, temp_in, sizeof(temp_in) - 1)) > 0) {
		temp_in[len] = '\0';
		if (pr)
			printf("%s", temp_in);
	}

	if (pr)
		printf("\n");

	return 0;
}


