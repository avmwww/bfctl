/*
 * msp cmd interface
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "failure.h"
#include "bf.h"
#include "msp.h"
#include "msp_protocol.h"
#include "msp_cmd.h"
#include "esc4way.h"
#include "cmd_arg.h"
#include "dump_hex.h"

#define xstr(a) str(a)
#define str(a) #a

#ifndef O_BINARY
# define O_BINARY	0
#endif

/* Betaflight baudrates */
static const uint32_t bf_baud_rates[] = {
	0, 9600, 19200, 38400, 57600, 115200, 230400, 250000,
	400000, 460800, 500000, 921600, 1000000, 1500000, 2000000, 2470000
};

#define ARRAYLEN(x)				(sizeof(x) / sizeof((x)[0]))
#define BAUD_RATE_COUNT				ARRAYLEN(bf_baud_rates)

/*
 *
 */
static const uint32_t bf_baud_rate(uint8_t index)
{
	if (index > BAUD_RATE_COUNT)
		index = 0;

	return bf_baud_rates[index];
}

#ifdef __MINGW32__
ssize_t getline(char **lineptr, size_t *n, FILE *stream) 
{
	char *bufptr;
	char *p;
	size_t size;
	int c;

	if (lineptr == NULL || stream  == NULL || n == NULL)
		return -1;

	bufptr = *lineptr;
	size = *n;

	c = fgetc(stream);
	if (c == EOF)
		return -1;

	if (bufptr == NULL) {
		bufptr = malloc(128);
		if (bufptr == NULL)
			return -1;

		size = 128;
	}
	p = bufptr;
	while (c != EOF) {
		unsigned long offt = p - bufptr;
		if (offt > (size - 1)) {
			size = size + 128;
			p = realloc(bufptr, size);
			if (p == NULL) {
				free(bufptr);
				return -1;
			}
			bufptr = p;
			p += offt;
		}
		*p++ = c;
		if (c == '\n')
			break;

		c = fgetc(stream);
	}

	*p++ = '\0';
	*lineptr = bufptr;
	*n = size;

	return p - bufptr - 1;
}
#endif

/*
 *
 */
static void dump_serial_config(const void *data, unsigned int data_len)
{
	int count;
	int i;
	const uint8_t *p = data;

	count = *p++;
	printf("Serial ports count: %d\n", count);
	for (i = 0; i < count; i++) {
		printf("Identifier: %u\n", *p++);
		printf("Functions mask:\t\t\t0x%08x\n", *(uint32_t *)p);
		p += 4;
		printf("MSP baud rate index:\t\t%u, %u\n", *p, bf_baud_rate(*p));
		p++;
		printf("GPS baud rate index:\t\t%u, %u\n", *p, bf_baud_rate(*p));
		p++;
		printf("Telemetry baud rate index:\t%u, %u\n", *p, bf_baud_rate(*p));
		p++;
		printf("Black box baud rate index:\t%u, %u\n", *p, bf_baud_rate(*p));
		p++;
	}
}

/*
 *
 */
static void dump_analog(const void *data, unsigned int data_len)
{
	const uint8_t *p = data;

	printf("Analog data:\n");
	printf("Battery V:\t\t%u\n", *p++);
	printf("Battery mAh:\t\t%u\n", *(uint16_t *)p);
	p += 2;
	printf("RSSI:\t\t\t%u\n", *(uint16_t *)p);
	p += 2;
	printf("Current mA:\t\t%d\n", (int32_t)(*(int16_t *)p) * 10);
	p += 2;
	printf("Battery V:\t\t%u\n", *(uint16_t *)p);
	p += 2;
}

/*
 *
 */
static void dump_tx_info(const void *data, unsigned int data_len)
{
	const uint8_t *p = data;

	printf("TX info:\n");
	printf("RSSI source:\t\t%u\n", *p++);
	printf("Date and time is set:\t%u\n", *p++);
}

/*
 *
 */
static void dump_motor_telemetry(int num, int motor, motor_tlm_t *tlm)
{
	int i;
	int from = 0, to = num;

	if (motor >= num) {
		printf("Invalid motor number: %d\nRange from %d to %d, or -1 for all\n",
				motor, 0, num - 1);
		return;
	}
	if (motor >= 0) {
		from = motor;
		to = motor + 1;
		tlm += from;
	}

	for (i = from; i < to; i++) {
		printf("TELEMETRY motor %d:\n", i);
		printf("RPM: %d\n", tlm->rpm);
		printf("ERR: %d\n", tlm->invalid_pkt);
		printf("T: %d\n", tlm->esc_temperature);
		printf("V: %d\n", tlm->esc_voltage);
		printf("I: %d\n", tlm->esc_current);
		tlm++;
	}
}

/*
 *
 */
static int msp_enter_to_cli(msp_t *msp, const char *arg)
{
	char data[256];

	return msp_cmd_transmit(msp->fd, "#", data, sizeof(data), 1);
}

/*
 *
 */
static int msp_send_cli_cmd(msp_t *msp, const char *cmd)
{
	char data[4096];

	if (msp_cmd_transmit(msp->fd, cmd, data, sizeof(data), 0) < 0)
		return -1;

	return msp_cmd_transmit(msp->fd, "\n", data, sizeof(data), 1);
}

/*
 *
 */
static int msp_send_cli_file(msp_t *msp, const char *file)
{
	char data[1024];
	FILE *fdr;
	char *line = NULL;
	size_t rd = 0;
	int n;
	int err = 0;

	if ((fdr = fopen(file, "r")) == NULL)
		failure(errno, "Can't open input file %s", file);

	/* set polling mode */
	serial_set_timeout(msp->fd, 0.);

	while ((n = getline(&line, &rd, fdr)) > 0) {
		if (msp_cmd_transmit(msp->fd, line, data, sizeof(data), 1) < 0) {
			err = -1;
			break;
		}
	}
	if (n < 0)
		err = -1;

	serial_set_timeout(msp->fd, 0.2);
	/* new line is finish */
	msp_cmd_transmit(msp->fd, "\n", data, sizeof(data), 1);

	free(line);
	fclose(fdr);
	return err;
}

/*****************************************************************************/

static void dump_motors(int num, int *val)
{
	int i;
	printf("Motor values:\n");
	for (i = 0; i < num; i++)
		printf("%d ", val[i]);
	printf("\n");
}

static int msp_set_motor(msp_t *msp, const char *arg)
{
	int num;
	int val[BF_MOTOR_MAX_NUM];

	num = cmd_arg_to_int(arg, val, BF_MOTOR_MAX_NUM);

	dump_motors(num, val);

	return bf_set_motor(msp->fd, num, val);
}

static int msp_get_motor(msp_t *msp, const char *arg)
{
	int val[BF_MOTOR_MAX_NUM];
	int num = BF_MOTOR_MAX_NUM;

	if (bf_get_motor(msp->fd, &num, val) < 0)
		return -1;

	dump_motors(num, val);
	return 0;
}

static int msp_set_passthrough(msp_t *msp, const char *arg)
{
	return msp_send_cli_cmd(msp, "serialpassthrough 0 420000");
}


/* ESC interface commands  */

typedef struct {
	int d;
	float f;
} esc_set_val_t;

struct esc_set {
	char *name;
	char *desc;
	int min;
	int max;
	int disable;
	int ext;
	enum {
		ESC_DATA_INT = 0,
		ESC_DATA_FLT,
		ESC_DATA_STR,
	} type;
	esc_set_val_t (*conv)(esc_set_val_t, int);
	const char *units;
};

static int esc_interface_name(esc4way_t *esc, const char *arg)
{
	int len;
	char name[256];

	memset(name, 0, sizeof(name));
	len = esc4way_interface_name(esc, name, sizeof(name) - 1);
	if (len < 0)
		return -1;
	printf("Interface name: %s\n", name);
	return 0;
}

static int esc_write(esc4way_t *esc, const char *arg)
{
	int val[32];
	int len;
	int chan;
	int addr;
	uint8_t byte;
	uint8_t data[256];

	len = cmd_arg_to_int(arg, val, sizeof(val)/sizeof(val[0]));
	if (len < 3)
		return 0;

	chan = val[0];
	addr = val[1];
	byte = val[2];

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	if (esc4way_read_flash(esc, addr & ~0xff, data, sizeof(data)) < 0)
		return -1;

	printf("Write %02x to %04x\n", byte, addr);
	data[addr & 0xff] = byte;

	return esc4way_write_flash(esc, addr & ~0xff, data, sizeof(data));
}

static esc_set_val_t esc_conv_kv(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_KV_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_KV(val.d);

	return val;
}

static esc_set_val_t esc_conv_advanve_level(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_ADVANCE_LEVEL_TO_SET(val.f);
	else
		val.f = ESC_SET_TO_ADVANCE_LEVEL(val.d);

	return val;
}

static esc_set_val_t esc_conv_servo_low(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_SERVO_LOW_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_SERVO_LOW(val.d);

	return val;
}

static esc_set_val_t esc_conv_servo_high(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_SERVO_HI_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_SERVO_HI(val.d);

	return val;
}

static esc_set_val_t esc_conv_servo_mid(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_SERVO_MID_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_SERVO_MID(val.d);

	return val;
}

static esc_set_val_t esc_conv_cell_low_volt(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_CELL_LOW_VOLT_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_CELL_LOW_VOLT(val.d);

	return val;
}

static esc_set_val_t esc_conv_current_limit(esc_set_val_t val, int set)
{
	if (set)
		val.d = ESC_CURRENT_LIMIT_TO_SET(val.d);
	else
		val.d = ESC_SET_TO_CURRENT_LIMIT(val.d);

	return val;
}

static const struct esc_set esc_settings[] = {
	{"head"    , ""},
	{"layout"  , "Layout ver"},
	{""},
	{"major", "Firmware ver major"},
	{"minor", "Firmware ver minor"},
	{"name"    , "Device name", 0, 0, 0, 11, ESC_DATA_STR},
	{""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
	{"dir"     , "Reverse direction",  0, 1, 0},
	{"bidir"   , "Bi direction / 3D" , 0, 1, 0},
	{"sin"     , "Sinusoidal startup", 0, 1, 0},
	{"cpwm"    , "Complementary PWM" , 0, 1, 0},
	{"vpwm"    , "Variable PWM"      , 0, 1, 0},
	{"rprotect", "Rotor protect"     , 0, 1, 0},
	{"alevel"  , "Advance level"     , 0, 42, -1, 0, ESC_DATA_FLT, esc_conv_advanve_level, "°"},
	{"tcycle"  , "Timer cycle"       , 8, 48, -1, 0, 0, NULL, "kHz"},
	{"dcycle"  , "Duty cycle"        , 50, 150, -1},
	{"kv"      , "KV"                , -1, -1, -1, 0, 0, esc_conv_kv},
	{"poles"   , "Poles"             , -1, -1, -1},
	{"bstop"   , "Brake on stop"     , 0, 1, 0},
	{"stall"   , "Stall protect"     , 0, 1, 0},
	{"volume"  , "Sound volume"      , 0, 11, 0},
	{"tlm"     , "Telemetry"         , 0, 1, 0},
	{"slow"    , "Servo low"         , -1, -1, -1, 0, 0, esc_conv_servo_low},
	{"shigh"   , "Servo high"        , -1, -1, -1, 0, 0, esc_conv_servo_high},
	{"smid"    , "Servo neutral"     , -1, -1, -1, 0, 0, esc_conv_servo_mid},
	{"dband"   , "Dead band"         , -1, -1, -1},
	{"lvolt"   , "Low volt protect"  , 0, 1, 0},
	{"cvolt"   , "Cell low volt"     , 0, 0, 0, 0, 0, esc_conv_cell_low_volt},
	{"rcrev"   , "RC car reverse"    , 0 , 1, 0},
	{"hall"    , "Hall sensor"       , 0 , 1, 0},
	{"srange"  , "Sine range"        , 5 , 25, 0},
	{"dbrake"  , "Drag brake"        , 1 , 10, 0},
	{"mbrake"  , "Driving brake"     , 1 , 9, 0},
	{"temp"    , "Temperature limit" , 70, 140, 141},
	{"curr"    , "Current limit"     , 0 , 99, 0, 0, 0, esc_conv_current_limit},
	{"spower"  , "Sine power"        , 1 , 10, 0},
	{"proto"   , "Input protocol"    , 0 , 9, -1},
	{"dtime"   , "Dead time"         , 0 , 255, -1},
	{""},
};

static const char *esc_bool_value(int value)
{
	return value ? "ON" : "OFF";
}

static int esc_parse_set(const char *arg, char *name, int name_len, esc_set_val_t *val)
{
	int i = 0;

	while (arg[i] != '\0' && arg[i] != ' ' && i < (name_len - 1)) {
		name[i] = arg[i];
		i++;
	}

	name[i] = '\0';

	arg = cmd_arg_next(arg);
	if (!arg)
		return -1;

	val->d = strtol(arg, NULL, 0);
	val->f = strtof(arg, NULL);

	return 0;
}

static const struct esc_set *esc_find_set(const char *name, int *index)
{
	const struct esc_set *es;
	int i;

	for (i = 0; i < sizeof(esc_settings) / sizeof(esc_settings[0]); i++) {
		es = &esc_settings[i];
		if (!es->name)
			continue;

		if (!strcmp(es->name, name)) {
			*index = i;
			return es;
		}
	}

	return NULL;
}

static int esc_set_read_from_file(const char *fname, uint8_t *set, int size)
{
	int fl;
	struct stat stat;

	if (!strlen(fname)) {
		printf("Invalid file name: <%s>\n", fname);
		return -1;
	}

	fl = open(fname, O_RDONLY | O_BINARY);
	if (fl < 0) {
		fprintf(stderr, "Can't open file %s, %s\n", fname, strerror(errno));
		return -1;
	}

	if (fstat(fl, &stat) < 0) {
		fprintf(stderr, "Can't get file %s size, %s\n", fname, strerror(errno));
		close(fl);
		return -1;
	}

	if (stat.st_size != size)
		printf("Warning: settings file size is invalid, %ld, should be %d\n",
				stat.st_size, size);

	if (read(fl, set, size) < 0) {
		fprintf(stderr, "Can't read file %s, %s\n", fname, strerror(errno));
		close(fl);
		return -1;
	}
	close(fl);
	return 0;
}

static int esc_set_write_to_file(const char *fname, uint8_t *set, int size)
{
	int fl;

	if (!strlen(fname)) {
		printf("Invalid file name: <%s>\n", fname);
		return -1;
	}

	fl = open(fname, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fl < 0) {
		fprintf(stderr, "Can't open file %s, %s\n", fname, strerror(errno));
		return -1;
	}

	if (write(fl, set, size) < 0) {
		fprintf(stderr, "Can't write file %s, %s\n", fname, strerror(errno));
		close(fl);
		return -1;
	}
	close(fl);
	return 0;
}

static void esc_settings_printf(uint8_t *set, int chan)
{
	const struct esc_set *es;
	int i;

	printf("ESC channel %d settings:\n", chan);
#define PRE		"\t(%d)%-10s%-24s"
	for (i = 0; i < sizeof(esc_settings) / sizeof(esc_settings[0]); i++) {
		es = &esc_settings[i];
		if (es->name[0]) {
			esc_set_val_t val;

			val.d = set[i];
			if (es->conv)
				val = es->conv(val, 0);

			printf(PRE, i, es->name, es->desc);
			if (es->type == ESC_DATA_STR) {
				int k;
				for (k = 0; k < es->ext; k++)
					printf("%c", set[i + k]);
			} else {
				if (es->type == ESC_DATA_FLT)
					printf("%.3f", val.f);
				else
					printf("%d", val.d);
				if (es->units)
					printf("%s", es->units);
				printf(" (%d) ", set[i]);
				if (val.d != set[i])
					printf(" (%d)", set[i]);
				if (es->min == 0 && es->max == 1) /* boolean */
					printf(" (%s)", esc_bool_value(val.d));
				else if (es->min >= 0 && es->max > 0) {
					printf(" [%d..%d]", es->min, es->max);
					if (es->disable >= 0)
						printf(" [off = %d]", es->disable);
				}
			}
			printf("\n");
		}
		i += es->ext;
	}
}

static int esc_sset(esc4way_t *esc, const char *arg)
{
	char name[256];
	int chan = strtol(arg, NULL, 0);
	esc_set_val_t  val;
	uint8_t *set = esc->set.data.byte;
	const struct esc_set *es;
	int index;

	arg = cmd_arg_next(arg);
	if (!arg)
		return -1;

	if (esc_parse_set(arg, name, sizeof(name), &val) < 0)
		return -1;

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	if (esc4way_settings_cache(esc) < 0)
		return -1;

	if (!(es = esc_find_set(name, &index))) {
		fprintf(stderr, "Invalid setting name: %s\n", name);
		return -1;
	}

	if (es->conv)
		val = es->conv(val, 1);

	printf("Set %s to value %d\n", es->name, val.d);
	set[index] = val.d;

	return esc4way_write_flash(esc, esc->set.addr, set, esc->set.size);
}

static int esc_sfset(esc4way_t *esc, const char *arg)
{
	uint8_t *set = esc->set.data.byte;
	unsigned int size = sizeof(esc->set.data.name);
	char name[256];
	const struct esc_set *es;
	esc_set_val_t  val;
	char fname[256];
	int index;

	cmd_name_copy(arg, fname, sizeof(fname));

	arg = cmd_arg_next(arg);
	if (!arg)
		return -1;

	if (esc_set_read_from_file(fname, set, size) < 0)
		return -1;

	if (esc_parse_set(arg, name, sizeof(name), &val) < 0)
		return -1;

	if (!(es = esc_find_set(name, &index))) {
		fprintf(stderr, "Invalid setting name: %s\n", name);
		return -1;
	}

	if (es->conv)
		val = es->conv(val, 1);

	printf("Set %s to value %d\n", es->name, val.d);
	set[index] = val.d;

	return esc_set_write_to_file(fname, set, size);
}

static int esc_sdump(esc4way_t *esc, const char *arg)
{
	int chan = strtol(arg, NULL, 0);
	uint8_t *set = esc->set.data.byte;

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	if (esc4way_settings_cache(esc) < 0)
		return -1;

	esc_settings_printf(set, chan);

	return 0;
}

static int esc_sfdump(esc4way_t *esc, const char *arg)
{
	const char *fname;
	uint8_t *set = esc->set.data.byte;
	unsigned int size = sizeof(esc->set.data.name);

	fname = arg;

	if (esc_set_read_from_file(fname, set, size) < 0)
		return -1;

	esc_settings_printf(set, 0);

	return 0;
}

static int esc_read_from_flash_to_file(esc4way_t *esc, int chan, int addr, int len, const char *fname)
{
	uint8_t data[256];
	int fl;
	int offt, size = len;
	int n;

	if (!strlen(fname)) {
		printf("Invalid file name: <%s>\n", fname);
		return -1;
	}

	fl = open(fname, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0644);
	if (fl < 0) {
		fprintf(stderr, "Can't open file %s, %s\n", fname, strerror(errno));
		return -1;
	}

	if (esc4way_select_chan(esc, 0, chan) < 0) {
		fprintf(stderr, "esc4way init flash on channel %d failure\n", chan);
		close(fl);
		return -1;
	}

	offt = 0;
	while (len) {
		n = len;
		if (n > sizeof(data))
			n = sizeof(data);

		if (esc4way_read_flash(esc, addr + offt, data, n) < 0) {
			fprintf(stderr, "esc4way read flash error\n");
			close(fl);
			return -1;
		}

		if (write(fl, data, n) < 0) {
			fprintf(stderr, "esc4way write to flash file error, %s\n", strerror(errno));
			return -1;
		}

		offt += n;
		len -= n;
		printf("Progress %d%%\taddr: %d\r", (offt * 100) / size, addr + offt);
		fflush(stdout);
		fflush(stderr);
	}
	printf("\nSuccess\n");

	close(fl);
	return 0;
}

static int esc_write_file_to_flash(esc4way_t *esc, int chan, int addr, const char *fname)
{
	uint8_t data[256];
	int len;
	int fl;
	int offt;
	struct stat stat;

	if (!strlen(fname)) {
		printf("Invalid file name: <%s>\n", fname);
		return -1;
	}

	fl = open(fname, O_RDONLY | O_BINARY);
	if (fl < 0) {
		fprintf(stderr, "Can't open file %s, %s\n", fname, strerror(errno));
		return -1;
	}

	if (fstat(fl, &stat) < 0) {
		fprintf(stderr, "Can't get file %s size, %s\n", fname, strerror(errno));
		close(fl);
		return -1;
	}

	if (esc4way_select_chan(esc, 0, chan) < 0) {
		fprintf(stderr, "esc4way init flash on channel %d failure\n", chan);
		close(fl);
		return -1;
	}

	offt = 0;
	while ((len = read(fl, data, sizeof(data))) > 0) {
		if (esc4way_write_flash(esc, addr + offt, data, len) < 0) {
			fprintf(stderr, "esc4way write flash error\n");
			close(fl);
			return -1;
		}

		offt += len;
		printf("Progress %ld%%\taddr: %d\r", (offt * 100) / stat.st_size, addr + offt);
		fflush(stdout);
		fflush(stderr);
	}
	printf("\nSuccess\n");

	close(fl);
	return 0;
}

static int esc_init(esc4way_t *esc, const char *arg)
{
	int chan = strtol(arg, NULL, 0);
	return esc4way_select_chan(esc, 0, chan);
}

static int esc_reset(esc4way_t *esc, const char *arg)
{
	int chan = strtol(arg, NULL, 0);
	return esc4way_reset(esc, chan);
}

static int esc_exit(esc4way_t *esc, const char *arg)
{
	return esc4way_exit(esc);
}

static int esc_version(esc4way_t *esc, const char *arg)
{
	int val[32];
	int len;
	int chan;
	int major, minor;

	len = cmd_arg_to_int(arg, val, sizeof(val)/sizeof(val[0]));
	if (len < 3)
		return 0;

	chan = val[0];
	major = val[1];
	minor = val[2];

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	return esc4way_set_version(esc, major, minor);
}

static int esc_swrite(esc4way_t *esc, const char *arg)
{
	char *end, *fname;
	int chan = strtol(arg, &end, 0);

	while (*end == ' ' && *end != '\0') end++;

	fname = end;

	return esc_write_file_to_flash(esc, chan, esc->set.addr, fname);
}

static int esc_sread(esc4way_t *esc, const char *arg)
{
	char *end, *fname;
	int chan = strtol(arg, &end, 0);

	while (*end == ' ' && *end != '\0') end++;

	fname = end;

	return esc_read_from_flash_to_file(esc, chan, esc->set.addr, esc->set.size, fname);
}

static int esc_flash(esc4way_t *esc, const char *arg)
{
	char *end, *fname;
	int chan = strtol(arg, &end, 0);

	while (*end == ' ' && *end != '\0') end++;

	fname = end;

	return esc_write_file_to_flash(esc, chan, esc->fw.addr, fname);
}

static int esc_set_passthrough(serial_handle fd, int chan)
{
	int len;
	uint8_t data[8];

	data[0] = MSP_PASSTHROUGH_ESC_4WAY;
	data[1] = chan;
	len = msp_transmit(fd, MSP_SET_PASSTHROUGH, MSP_DIR_OUT, data, 2, data, sizeof(data));
#ifdef DEBUG
	if (len > 0) {
		printf("esc passthrough reply: %d bytes\n", len);
		dump_hex(data, len, 1);
	}
#endif
	if (len > 0)
		printf("Passthrough mode set success, esc has %d channels\n", data[0]);
	else
		printf("Passthrough mode set failed, maybe already set\n");
	return len;
}

#define MARK_FAIL		"FLASH FAIL  "
#define MARK_NOT_READY		"NOT READY   "
static int esc_flashall(esc4way_t *esc, const char *arg)
{
	char *end, *fname;
	int chan = strtol(arg, &end, 0);

	while (*end == ' ' && *end != '\0') end++;
	fname = end;

	//esc_set_passthrough(esc->fd, chan);
	if (esc_interface_name(esc, arg) < 0)
		return -1;

	usleep(500000);
	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	if (esc4way_boot_flash(esc, 0) < 0)
		return -1;

	if (esc4way_mark_flash(esc, MARK_FAIL) < 0)
		return -1;

	if (esc4way_set_version(esc, 0, 0) < 0)
		return -1;

	if (esc_write_file_to_flash(esc, chan, esc->fw.addr, fname) < 0)
		return -1;

	if (esc4way_mark_flash(esc, MARK_NOT_READY) < 0)
		return -1;

	if (esc4way_boot_flash(esc, 1) < 0)
		return -1;

	return esc4way_exit(esc);
}

static int msp_set_esc_passthrough(msp_t *msp, const char *arg)
{
	int chan = strtol(arg, NULL, 0);

	return esc_set_passthrough(msp->fd, chan);
}

static int esc_send(esc4way_t *esc, const char *arg)
{
	char data[256];
	int val[32];
	int len;
	int addr = 0;
	int cmd;
	int i;

	len = cmd_arg_to_int(arg, val, sizeof(val)/sizeof(val[0]));
	if (len == 0)
		return 0;

	len--;
	if (len == 0)
		len = 1;

	cmd = val[0];
	for (i = 0; i < len; i++)
		data[i + 1] = val[i + 1];

	len = esc4way_send(esc, cmd, addr, &data[1], len, data, sizeof(data));

	if (len > 0) {
		printf("esc reply: %d bytes\n", len);
		dump_hex(data, len, 1);
	}
	return len;
}

static int esc_boot(esc4way_t *esc, const char *arg)
{
	int val[32];
	int len;
	int chan, byte;

	len = cmd_arg_to_int(arg, val, sizeof(val)/sizeof(val[0]));
	if (len < 2)
		return 0;

	chan = val[0];
	byte = val[1];

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	return esc4way_boot_flash(esc, byte);
}

static int esc_mark(esc4way_t *esc, const char *arg)
{
	int chan = strtol(arg, NULL, 0);

	arg = cmd_arg_next(arg);
	if (!arg)
		return -1;

	if (esc4way_select_chan(esc, 0, chan) < 0)
		return -1;

	return esc4way_mark_flash(esc, arg);
}

static int esc_dump(esc4way_t *esc, const char *arg)
{
	int len;
	int addr;
	int chan;
	int val[32];
	uint8_t *data;

	/* chan addr size */
	len = cmd_arg_to_int(arg, val, sizeof(val)/sizeof(val[0]));
	if (len < 3)
		return 0;

	chan = val[0];
	addr = val[1];
	len = val[2];

	if (esc4way_select_chan(esc, addr, chan) < 0)
		return -1;

	data = malloc(len);
	if (!data)
		return -1;

	if (esc4way_read_flash(esc, addr, data, len) < 0) {
		free(data);
		return -1;
	}

	printf("EEPROM:\n");
	dump_hex(data, len, 1);
	free(data);
	return 0;
}

struct esc_command {
	const char *cmd;
	const char *help;
	int (*handle)(esc4way_t *, const char *);
	bool no_need_dev;
};

static const struct esc_command esc_commands[] = {
	{"sset", "<channel> <name> <value> set esc setting", esc_sset},
	{"sfset", "<file> <name> <value> set esc setting in bin file", esc_sfset, true},
	{"sdump", "<channel> esc settings dump", esc_sdump},
	{"sfdump", "<file> esc settings dump from bin file", esc_sfdump, true},
	{"swrite", "<channel> <file> flash settings bin file to esc", esc_swrite},
	{"sread", "<channel> <file> read settings to bin file from esc", esc_sread},
	{"iname", "esc interface name", esc_interface_name},
	{"write", "<channel> <addr> <byte>write byte to esc flash at address", esc_write},
	{"init", "<channel> esc initalize", esc_init},
	{"reset", "<channel> reset esc", esc_reset},
	{"exit", "exit from esc mode", esc_exit},
	{"version", "<channel> <major> <minor>set esc firmware version", esc_version},
	{"flash", "<channel> <file> flash firmware bin file to esc", esc_flash},
	{"flashall", "<channel> <file> alias of commands: init, mark, version, flash, mark, boot, exit", esc_flashall},
	{"send", "<cmd args> send to esc", esc_send},
	{"dump", "<channel> <addr> <size> esc flash dump, addr for firmware "
		xstr(ESC_FLASH_FIRMWARE_OFFT) ", for settings "
		xstr(ESC_FLASH_SETTINGS_OFFT), esc_dump},
	{"mark", "<channel> <string> esc set device_info to string", esc_mark},
	{"boot", "<channel> <byte> write byte at 0 position of settings, 1 is valid firmware", esc_boot},
	{"help", "help usage", esc_usage, true},
	{NULL} /* last */
};

int esc_usage(esc4way_t *esc, const char *arg)
{
	const struct esc_command *ec = esc_commands;

	printf("ESC commands:\n");
	while (ec->cmd) {
		printf("\t%-10s   %s\n", ec->cmd, ec->help);
		ec++;
	}
	if (esc)
		exit(EXIT_SUCCESS);
	return 0;
}

static int esc_command_handle(esc4way_t *esc, const char *cmd, const char *arg)
{
	const struct esc_command *ec = esc_commands;
	int err;

	while (ec->cmd) {
		if (!strcmp(cmd, ec->cmd)) {
			if (!ec->no_need_dev && esc->fd < 0)
				return -2;

			if (!ec->no_need_dev) {
				/* For esc ops set timeout of serial port to 1s */
				if (serial_set_timeout(esc->fd, 1.0) < 0)
					failure(errno, "Can't set serial port timeout");
			}

			err = ec->handle(esc, arg);
			if (err < 0)
				return -1;
			return 1;
		}
		ec++;
	}
	return 0;
}

static int esc_command(msp_t *msp, const char *args)
{
	char cmd[256];
	char arg[256];
	int len;

	len = cmd_arg_copy(args, cmd, sizeof(cmd), arg, sizeof(arg));

	len = esc_command_handle(msp->esc, cmd, arg);
	if (len == 0) {
		printf("Unknown esc command: %s\n", cmd);
		esc_usage(msp->esc, arg);
	}
	return len;
}

static int msp_get_motor_telemetry(msp_t *msp, const char *arg)
{
	motor_tlm_t tlm[BF_MOTOR_MAX_NUM];
	int num = BF_MOTOR_MAX_NUM;
	int motor = -1;

	if (bf_get_motor_telemetry(msp->fd, &num, tlm) < 0)
		return -1;

	if (strlen(arg))
		motor = strtol(arg, NULL, 0);

	dump_motor_telemetry(num, motor, tlm);
	return 0;
}

static int msp_board_info(msp_t *msp, const char *arg)
{
	return bf_board_info(msp->fd);
}

static int msp_print_serial_config(msp_t *msp, const char *arg)
{
	int len;
	uint8_t data[256];

	if ((len = msp_transmit(msp->fd, MSP2_COMMON_SERIAL_CONFIG, 1,
				data, 0, data, sizeof(data)) < 0))
		return -1;

	dump_serial_config(data, len);
	return 0;
}

static int msp_print_analog(msp_t *msp, const char *arg)
{
	int len;
	uint8_t data[256];

	if ((len = msp_transmit(msp->fd, MSP_ANALOG, 1, data, 0,
				data, sizeof(data))) < 0)
		return -1;

	dump_analog(data, len);
	return 0;
}

static int msp_tx_info(msp_t *msp, const char *arg)
{
	int len;
	uint8_t data[256];

	if ((len = msp_transmit(msp->fd, MSP_TX_INFO, 1, data, 0, data, sizeof(data))) < 0)
		return -1;

	dump_tx_info(data, len);
	return 0;
}

static int msp_reboot(msp_t *msp, const char *arg)
{
	return bf_reboot(msp->fd, atoi(arg));
}

static int msp_exit_from_cli(msp_t *msp, const char *arg)
{
	return msp_send_cli_cmd(msp, "exit");
}

static int msp_send_dshot(msp_t *msp, const char *arg)
{
	int num = 0;
	int val[BF_MOTOR_MAX_NUM];

	while (*arg && num < BF_MOTOR_MAX_NUM) {
		char *end;
		val[num++] = strtol(arg, &end, 0);
		while (*end == ' ' && *end != '\0') end++;
		arg = end;
	}
	dump_motors(num, val);

	return bf_send_dshot(msp->fd, 0, val[0], num - 1 , &val[1]);
}

struct msp_command {
	const char *cmd;
	const char *help;
	int (*handle)(msp_t *, const char *);
	bool no_need_dev;
};

static const struct msp_command msp_commands[] = {
	{"info", "print board info", msp_board_info},
	{"help", "help usage", msp_usage, true},
	{"tx_info", "print tx info", msp_tx_info},
	{"serial", "print serial config", msp_print_serial_config},
	{"analog", "print analog", msp_print_analog},
	{"cli", "enter to cli mode", msp_enter_to_cli},
	{"dshot", "<cmd> send dshot command", msp_send_dshot},
	{"exit", "exit from cli mode", msp_exit_from_cli},
	{"send", "<cmd> send cli command", msp_send_cli_cmd},
	{"sendfile", "<filename> send file", msp_send_cli_file},
	{"reboot", "<tag> reboot, tag=1 to DFU mode, tag=0 reboot firmware", msp_reboot},
	{"gmotor", "get motors values", msp_get_motor},
	{"smotor", "<values>, set motor values, maximum number of motors is "
		   xstr(BF_MOTOR_MAX_NUM), msp_set_motor},
	{"pass", "set passthrough serial mode", msp_set_passthrough},
	{"tlm", "<motor or empty to all> get motor telemetry", msp_get_motor_telemetry},
	{"esc_pass", "<channel or 255 for all> set esc passthrough", msp_set_esc_passthrough},
	{"esc", "esc commands, try help to view available commands", esc_command, true},
	{NULL} /* last */
};

int msp_usage(msp_t *msp, const char *arg)
{
	const struct msp_command *mc = msp_commands;

	printf("MSP commands:\n");
	while (mc->cmd) {
		printf("\t%-10s   %s\n", mc->cmd, mc->help);
		mc++;
	}
	if (msp)
		exit(EXIT_SUCCESS);
	return 0;
}

static int msp_command_handle(msp_t *msp, const char *cmd, const char *arg)
{
	const struct msp_command *mc = msp_commands;
	int err;

	while (mc->cmd) {
		if (!strcmp(cmd, mc->cmd)) {
			if (!mc->no_need_dev && msp->fd < 0)
				return -2;

			err = mc->handle(msp, arg);
			if (err < 0)
				return err;
			return 1;
		}
		mc++;
	}
	return 0;
}

int msp_exec_cmd(msp_t *msp, const char *cmd)
{
	char data[256];
	char arg[256];
	int len;
	const char *p = cmd;

	while (*p) {
		len = cmd_arg_copy(p, data, sizeof(data), arg, sizeof(arg));
		p += len;

		if ((len = msp_command_handle(msp, data, arg)) == 0) {
			printf("unknown command: %s\n", data);
			msp_usage(msp, arg);
		}
		if (len < 0) {
			if (len == -2)
				return len;

			printf("transfer command <%s %s> error\n", data, arg);
			exit(EXIT_FAILURE);
		}
	}
	return 0;
}


