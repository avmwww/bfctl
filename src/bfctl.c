/*
 * Betaflight cli configuration tool
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "failure.h"
#include "serial.h"
#include "progopt.h"
#include "esc4way.h"
#include "msp_serial.h"
#include "msp_cmd.h"

#define BFCTL_VERSION_MAJOR		1
#define BFCTL_VERSION_MINOR		0

struct bfctl_conf {
	char *dev;
	unsigned int baud;
	serial_handle fd;
	int info;
	int help;
	char *send_raw;
	char *msp_cmd;
	msp_t msp;
};

#define BAUD_RATE_DEFAULT		115200

#define BFCTL_LINUX_DEVICE_DEFAULT		"/dev/ttyUSB0"
#define BFCTL_WINDOWS_DEVICE_DEFAULT		"COM1"
#define MSP_LINUX_DEVICE_DEFAULT		"/dev/ttyACM0"
#define MSP_WINDOWS_DEVICE_DEFAULT		BFCTL_WINDOWS_DEVICE_DEFAULT

#ifdef __MINGW32__
#define BFCTL_DEVICE_DEFAULT			BFCTL_WINDOWS_DEVICE_DEFAULT
#define MSP_DEVICE_DEFAULT			MSP_WINDOWS_DEVICE_DEFAULT
#else
#define BFCTL_DEVICE_DEFAULT			BFCTL_LINUX_DEVICE_DEFAULT
#define MSP_DEVICE_DEFAULT			MSP_LINUX_DEVICE_DEFAULT
#endif

#define INTSTR(s)				#s
#define XINTSTR(s)				INTSTR(s)

/* <short> <long> <description> <type> <offset> <value> */
#define BFCTL_OPT(s, l, d, t, o, v) \
		PROG_OPT(s, l, d, t, struct bfctl_conf, o, v)

#define BFCTL_OPT_NO(s, l, d, o, v)		BFCTL_OPT(s, l, d, OPT_NO, o, v)
#define BFCTL_OPT_INT(s, l, d, o)		BFCTL_OPT(s, l, d, OPT_INT, o, 0)
#define BFCTL_OPT_HEX(s, l, d, o)		BFCTL_OPT(s, l, d, OPT_HEX, o, 0)
#define BFCTL_OPT_STR(s, l, d, o)		BFCTL_OPT(s, l, d, OPT_STRING, o, 0)

static struct prog_option bfctl_options[] = {
	BFCTL_OPT_INT('b', "baud", "baud rate, default "
				   XINTSTR(BAUD_RATE_DEFAULT), baud),
	BFCTL_OPT_STR('d', "device", "serial device, default "
				     BFCTL_LINUX_DEVICE_DEFAULT " or "
				     MSP_LINUX_DEVICE_DEFAULT " on Linux"
				     "\n\t\t" BFCTL_WINDOWS_DEVICE_DEFAULT
			" on Windows", dev),
	BFCTL_OPT_NO ('h', "help", "help usage", help, 1),
	BFCTL_OPT_STR ('\0', "raw", "send raw data, ; or space delimiter", send_raw),
	BFCTL_OPT_STR ('\0', "msp", "use MSP protocol for serial commucations,"
				    " usually to FC, try \"help\" to show help", msp_cmd),
	PROG_END,
};

#define OPT_LEN		(sizeof(bfctl_options) / sizeof(bfctl_options[0]))


static void usage(char *prog, struct prog_option *opt)
{
	printf("Betaflight control utility, version %d.%02d, build on %s, %s\n",
			BFCTL_VERSION_MAJOR, BFCTL_VERSION_MINOR,
			__DATE__, __TIME__);
	printf("Usage: %s [options]\n", prog);
	prog_option_printf(stderr, opt);
}

/*
 *
 */
static int bfctl_com_port_name_validate(const char *dev, char *name)
{
#ifdef __MINGW32__
	/* COMx or \\\\.\COMx */
	unsigned int num = 0;

	name[0] = '\0';
	if (strncasecmp(dev, "COM", 3) == 0) {
		num = strtol(&dev[3], NULL, 0);
		printf("Port number: %d\n", num);
		if (num > 9)
			sprintf(name, "\\\\.\\COM%u", num);
	}
	printf("Port name: %s, device %s\n", name, dev);
#else
	name[0] = '\0';
#endif
	return 0;
}

/*
 *
 */
int main(int argc, char **argv)
{
	struct option opt[OPT_LEN + 1];
	char optstr[2 * OPT_LEN + 1];
	struct bfctl_conf conf;
	char port_name[256];
	int err_dev = 0;
	int err;

	memset(&conf, 0, sizeof(struct bfctl_conf));
#ifdef __MINGW32__
	conf.fd = INVALID_HANDLE_VALUE;
#else
	conf.fd = -1;
#endif

	/* set default values */
	conf.dev = NULL;
	conf.baud = BAUD_RATE_DEFAULT;

	if (prog_option_make(bfctl_options, opt, optstr, OPT_LEN) < 0)
		failure(0, "Invalid options");

	if (prog_option_load(argc, argv, bfctl_options, opt, optstr, &conf) < 0) {
		usage(argv[0], bfctl_options);
		exit(EXIT_FAILURE);
	}

	if (conf.help) {
		usage(argv[0], bfctl_options);
		msp_usage(NULL, NULL);
		esc_usage(NULL, NULL);
		exit(EXIT_SUCCESS);
	}

	if (conf.dev == NULL) {
		if (conf.msp_cmd)
			conf.dev = MSP_DEVICE_DEFAULT;
		else
			conf.dev = BFCTL_DEVICE_DEFAULT;
	}

	bfctl_com_port_name_validate(conf.dev, port_name);
	if (strlen(port_name))
		conf.dev = port_name;

	if ((conf.fd = serial_open(conf.dev)) < 0) {
		err_dev = errno;
	} else {
		if (serial_setup(conf.fd, conf.baud) < 0)
			failure(errno, "Can't set serial port %s parameters", conf.dev);

		serial_set_timeout(conf.fd, 0.2);
	}

	if (conf.msp_cmd) {
		conf.msp.fd = conf.fd;
		conf.msp.esc = esc4way_init(conf.fd);
		/* if passthrough mode, set first passthrough over MSP protocol */
		if ((err = msp_exec_cmd(&conf.msp, conf.msp_cmd)) < 0) {
			if (err == -2)
				failure(err_dev, "Can't open serial port %s", conf.dev);
			else
				failure(errno, "Can't execute MSP command");
		}

		exit(EXIT_SUCCESS);
	}


	exit(EXIT_SUCCESS);
}

