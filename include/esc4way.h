/*
 * esc4way
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _ESC4WAY_H_
#define _ESC4WAY_H_

#include <stdint.h>
#include <stdbool.h>

#include "serial.h"

#define ESC_FLASH_FIRMWARE_OFFT		0x1000
#define ESC_FLASH_SETTINGS_OFFT		0x7c00
#define ESC_FLASH_SETTINGS_SIZE		256


#define ESC_SET_DEVICE_NAME_SIZE	12


#define ESC_KV_TO_SET(x)		((x - 20) / 40)
#define ESC_SET_TO_KV(x)		(x * 40 + 20)

#define ESC_SET_TO_SERVO_LOW(x)		((x) * 2 + 750)
#define ESC_SERVO_LOW_TO_SET(x)		(((x) - 750) / 2)

#define ESC_SET_TO_SERVO_HI(x)		((x) * 2 + 1750)
#define ESC_SERVO_HI_TO_SET(x)		(((x) - 1750) / 2)

#define ESC_SET_TO_SERVO_MID(x)		((x) + 1374)
#define ESC_SERVO_MID_TO_SET(x)		((x) - 1374)

#define ESC_SET_TO_CELL_LOW_VOLT(x)	((x) + 250)
#define ESC_CELL_LOW_VOLT_TO_SET(x)	((x) - 250)

#define ESC_SET_TO_CURRENT_LIMIT(x)	((x) * 2)
#define ESC_CURRENT_LIMIT_TO_SET(x)	((x) / 2)

#define ESC_SET_TO_ADVANCE_LEVEL(x)	((x) * 7.5)
#define ESC_ADVANCE_LEVEL_TO_SET(x)	((x) / 7.5)

struct settings {
	uint8_t head;				// 0
	uint8_t layout_version;			// 1
	uint8_t xxx;				// 2
	struct {
		uint8_t major;
		uint8_t minor;
	} __attribute__((packed)) version;	// 3, 4
	uint8_t device_name[ESC_SET_DEVICE_NAME_SIZE];	// 5 .. 16
	uint8_t dir_reversed;			// 17
	uint8_t bi_direction;			// 18
	uint8_t sin_start;			// 19
	uint8_t comp_pwm;			// 20
	uint8_t variable_pwm;			// 21
	uint8_t stuck_rotor_protection;		// 22
	uint8_t advance_level;			// 23
	uint8_t timer_cycle;			// 24
	uint8_t duty_cycle;			// 25
	uint8_t kv;				// 26
	uint8_t poles;				// 27
	uint8_t brake_on_stop;			// 28
	uint8_t stall_protection;		// 29
	uint8_t sound_volume;			// 30
	uint8_t tlm;				// 31
	struct {
		uint8_t low_threshold;		// 32
		uint8_t high_threshold;		// 33
		uint8_t neutral;		// 34
		uint8_t dead_band;		// 35
	} __attribute__((packed)) servo;
	struct {
		uint8_t low_volt;		// 36
		uint8_t cell_low_volt;		// 37
	} __attribute__((packed)) battery;
	uint8_t rc_car_reverse;			// 38
	uint8_t hall_sensor;			// 39
	/* sine mode changeover 5-25 percent throttle */
	uint8_t sine_range;			// 40
	uint8_t drag_brake;			// 41
	uint8_t motor_brake;			// 42
	uint8_t temperature_limit;		// 43
	uint8_t current_limit;			// 44
	uint8_t sine_power;			// 45
	uint8_t input_proto;			// 46
	uint8_t yyyy;				// 47
	struct {
		uint8_t enable;			// 48
		uint8_t a;			// 49
		uint8_t b;			// 50
		uint8_t c;			// 51
		uint8_t data[124];		// 52..173
	} __attribute__((packed)) tune;
} __attribute__((packed));


typedef struct esc4way {
	serial_handle fd;
	struct {
		union {
			uint8_t byte[1024];
			struct settings name;
		} __attribute__((__packed__)) data;
		unsigned int addr;
		unsigned int size;
		bool cached;
	} set;
	struct {
		unsigned int addr;
	} fw;
} esc4way_t;

esc4way_t *esc4way_init(serial_handle fd);
int esc4way_settings_cache(esc4way_t *esc);
int esc4way_read_flash(esc4way_t *esc, int addr, void *buf, int len);
int esc4way_write_flash(esc4way_t *esc, int addr, const void *buf, int len);
int esc4way_select_chan(esc4way_t *esc, int addr, int chan);
int esc4way_mark_flash(esc4way_t *esc, const char *mark);
int esc4way_boot_flash(esc4way_t *esc, int mark);
int esc4way_set_version(esc4way_t *esc, int major, int minor);
int esc4way_exit(esc4way_t *esc);
int esc4way_reset(esc4way_t *esc, int chan);
int esc4way_interface_name(esc4way_t *esc, char *name, int len);

int esc4way_send(esc4way_t *esc, int cmd, int addr,
		const void *out, int out_len, void *in, int in_len);

#endif
