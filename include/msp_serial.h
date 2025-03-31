/*
 * msp serial
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */
#ifndef _MSP_SERIAL_H_
#define _MSP_SERIAL_H_

#include "serial.h"

#define MSP_DIR_IN		0
#define MSP_DIR_OUT		1

int msp_raw_transmit(serial_handle fd, const void *out, int out_size,
		     void *in, int in_size);

int msp_transmit(serial_handle fd, uint16_t cmd, int dir, const void *out, int out_size,
		 void *in, int in_size);

int msp_cmd_transmit(serial_handle fd, const char *out, char *in, int in_size, int pr);

#endif
