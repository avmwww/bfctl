/*
 * bf
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _BF_H_
#define _BF_H_

#include <stdint.h>

#include "msp_serial.h"

#define TARGET_HAS_VCP				0
#define TARGET_HAS_SOFTSERIAL			1
#define TARGET_HAS_FLASH_BOOTLOADER		3
#define TARGET_SUPPORTS_RX_BIND			6
#define SIGNATURE_LENGTH			32
#define PROBLEM_ACC_NEEDS_CALIBRATION		0
#define PROBLEM_MOTOR_PROTOCOL_DISABLED		1

#define BF_MOTOR_MAX_NUM		16

enum {
	MSP_REBOOT_FIRMWARE = 0,
	MSP_REBOOT_BOOTLOADER_ROM,
	MSP_REBOOT_MSC,
	MSP_REBOOT_MSC_UTC,
	MSP_REBOOT_BOOTLOADER_FLASH,
	MSP_REBOOT_COUNT,
};

typedef struct bf_board_info {
	uint8_t hw_rev;
	uint8_t max7456_present;
	uint8_t capabilities;
} __attribute__((__packed__)) bf_board_info_t ;


int bf_board_info(serial_handle fd);
int bf_reboot(serial_handle fd, int mode);
int bf_set_motor(serial_handle fd, int num, int *val);
int bf_get_motor(serial_handle fd, int *num, int *val);
int bf_send_dshot(serial_handle fd, int block, int motor, int num , int *val);

typedef struct motor_tlm {
	uint32_t rpm;
	uint16_t invalid_pkt;      // 100.00% == 10000
	uint8_t esc_temperature;   // degrees celcius
	uint16_t esc_voltage;      // 0.01V per unit
	uint16_t esc_current;      // 0.01A per unit
	uint16_t esc_consumption;  // mAh
} __attribute__((__packed__)) motor_tlm_t;

int bf_get_motor_telemetry(serial_handle fd, int *num, motor_tlm_t *tlm);

#endif
