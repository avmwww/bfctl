/*
 * msp
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _MSP_H_
#define _MSP_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
	uint8_t size;
	uint8_t cmd;
} mspHeaderV1_t;

typedef struct __attribute__((packed)) {
	uint16_t size;
} mspHeaderJUMBO_t;

typedef struct __attribute__((packed)) {
	uint8_t  flags;
	uint16_t cmd;
	uint16_t size;
} mspHeaderV2_t;

#define MSP_CRC_POLY				0xd5

#define MSP_PASSTHROUGH_SERIAL_ID		0xFD
#define MSP_PASSTHROUGH_SERIAL_FUNCTION_ID	0xFE
#define MSP_PASSTHROUGH_ESC_4WAY		0xFF

#define MSP2_COMMON_SERIAL_CONFIG		0x1009
#define MSP2_COMMON_SET_SERIAL_CONFIG		0x100A

#define MSP2_BETAFLIGHT_BIND			0x3000
#define MSP2_MOTOR_OUTPUT_REORDERING		0x3001
#define MSP2_SET_MOTOR_OUTPUT_REORDERING	0x3002
#define MSP2_SEND_DSHOT_COMMAND			0x3003
#define MSP2_GET_VTX_DEVICE_STATUS		0x3004
#define MSP2_GET_OSD_WARNINGS			0x3005  // returns active OSD warning message text


#endif
