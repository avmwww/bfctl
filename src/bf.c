/*
 * Betaflight interface
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "bf.h"
#include "msp_protocol.h"
#include "msp.h"
#include "dshot.h"

#include "dump_hex.h"

#define bit_is_set(v, b)		((v) & (1 << (b)))

static void dump_board_info(const void *data, unsigned int data_len)
{
	const uint8_t *p;
	unsigned int len;
	uint8_t buf[256];

	p = data;
	printf("Board identifier:\t%s\n", p);
	p += strlen((char *)p) + 1;
	printf("Hardware revision:\t%d\n", *p++);
	printf("MAX7456 present:\t%s\n", (*p++ == 2) ? "yes" : "no");
	printf("Target capabilities:\n");
	printf("\t\tVCP: %s\n", bit_is_set(*p, TARGET_HAS_VCP) ? "yes" : "no");
	printf("\t\tSOFTSERIAL: %s\n", bit_is_set(*p, TARGET_HAS_SOFTSERIAL) ? "yes" : "no");
	printf("\t\tFLASH BOOTLOADER: %s\n", bit_is_set(*p, TARGET_HAS_FLASH_BOOTLOADER) ? "yes" : "no");
	printf("\t\tRX BIND: %s\n", bit_is_set(*p,  TARGET_SUPPORTS_RX_BIND) ? "yes" : "no");
	p++;

	len = *p++;
	memcpy(buf, p, len);
	buf[len] = '\0';
	printf("Target name:\t\t%s\n", buf);
	p += len;

	len = *p++;
	memcpy(buf, p, len);
	buf[len] = '\0';
	printf("Board name:\t\t%s\n", buf);
	p += len;

	len = *p++;
	memcpy(buf, p, len);
	buf[len] = '\0';
	printf("Manufacturer ID:\t%s\n", buf);
	p += len;

	dump_hex_prefix("Signature:\t\t", p, SIGNATURE_LENGTH, 0);
	p += SIGNATURE_LENGTH;

	printf("MCU ID:\t\t\t0x%02x\n", *p++);
	printf("Config state:\t\t%d\n", *p++);
	printf("Gyro sample rate Hz:\t%d\n", *(uint16_t *)p);
	p += 2;
	printf("Configuration problems\n");
	printf("\t\tAccelerometer not calibrated: %s\n",
			bit_is_set(*(uint32_t *)p, PROBLEM_ACC_NEEDS_CALIBRATION) ? "yes" : "no");
	printf("\t\tMotor protocol disabled: %s\n",
			bit_is_set(*(uint32_t *)p, PROBLEM_MOTOR_PROTOCOL_DISABLED) ? "yes" : "no");
	p += 4;
	printf("SPI devices:\t%d\n", *p++);
	printf("I2C devices:\t%d\n", *p++);
}


int bf_board_info(serial_handle fd)
{
	int len;
	uint8_t data[256];

	/* Board identifier */
	if ((len = msp_transmit(fd, MSP_BOARD_INFO, MSP_DIR_OUT, data, 0,
				data, sizeof(data))) < 0)
		return -1;

	dump_board_info(data, len);
	return 0;
}

int bf_get_motor_telemetry(serial_handle fd, int *num, motor_tlm_t *tlm)
{
	int len;
	uint8_t data[256];
	int n = *num;

	if ((len = msp_transmit(fd, MSP_MOTOR_TELEMETRY, MSP_DIR_OUT, tlm, 0,
				data, sizeof(data))) < 0)
		return -1;

	if (n > data[0])
		n = data[0];

	*num = n;

	memcpy(tlm, &data[1], sizeof(motor_tlm_t) * n);

	return 0;
}

int bf_get_motor(serial_handle fd, int *num, int *val)
{
	int len;
	uint8_t data[256];
	uint16_t *mval = (uint16_t *)data;
	int i, n = *num;

	if ((len = msp_transmit(fd, MSP_MOTOR, MSP_DIR_OUT, data, 0,
				data, sizeof(data))) < 0)
		return -1;

	for (i = 0; i < len / sizeof(uint16_t) && i < n; i++)
		val[i] = mval[i];

	*num = len / sizeof(uint16_t);

	return 0;
}

int bf_set_motor(serial_handle fd, int num, int *val)
{
	uint16_t mval[BF_MOTOR_MAX_NUM];
	int i;

	if (num > BF_MOTOR_MAX_NUM)
		num = BF_MOTOR_MAX_NUM;

	for (i = 0; i < num; i++)
		mval[i] = *val++;

	return msp_transmit(fd, MSP_SET_MOTOR, MSP_DIR_OUT, mval,
			    num * sizeof(uint16_t), NULL, 0);
}

int bf_reboot(serial_handle fd, int mode)
{
	int len;
	uint8_t data[256];
	uint8_t reboot = (uint8_t)mode;

	if ((len = msp_transmit(fd, MSP_REBOOT, 1, &reboot, 1,
				data, sizeof(data))) < 0)
		return -1;

	return 0;
}

int bf_send_dshot(serial_handle fd, int block, int motor, int num , int *val)
{
	uint8_t data[256];
	int i;

	/* cmd type */
	data[0] = block ? DSHOT_CMD_TYPE_BLOCKING : DSHOT_CMD_TYPE_INLINE;
	/* motor index */
	data[1] = motor;
	/* cmd count */
	data[2] = num;
	/* cmds */
	for (i = 0; i < num; i++)
		data[i + 3] = val[i];

	return msp_transmit(fd, MSP2_SEND_DSHOT_COMMAND, MSP_DIR_OUT, data,
			    num + 3, NULL, 0);
}

