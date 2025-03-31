/*
 * msp cmd
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _MSP_CMD_H_
#define _MSP_CMD_H_

#include "serial.h"
#include "esc4way.h"

typedef struct msp {
	serial_handle fd;
	esc4way_t *esc;
} msp_t;

int msp_exec_cmd(msp_t *msp, const char *cmd);
int msp_usage(msp_t *msp, const char *arg);
int esc_usage(esc4way_t *esc, const char *arg);

#endif
