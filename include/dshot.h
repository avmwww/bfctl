/*
 * Dshot interface
 *
 * 2025 Andrey Mitrofanov <avmwww@gmail.com>
 */

#ifndef _DSHOT_H_
#define _DSHOT_H_


/*
 * DshotSettingRequest (KISS24).
 * Spin direction, 3d and save Settings require 10 requests.. and
 * the TLM Byte must always be high if 1-47 are used to send settings
 *
 * 3D Mode:
 * 0 = stop
 * 48   (low) - 1047 (high) -> negative direction
 * 1048 (low) - 2047 (high) -> positive direction
 */

typedef enum {
	DSHOT_CMD_MOTOR_STOP = 0,
	DSHOT_CMD_BEACON1,
	DSHOT_CMD_BEACON2,
	DSHOT_CMD_BEACON3,
	DSHOT_CMD_BEACON4,
	DSHOT_CMD_BEACON5,
	DSHOT_CMD_ESC_INFO, // V2 includes settings
	DSHOT_CMD_SPIN_DIRECTION_1,
	DSHOT_CMD_SPIN_DIRECTION_2,
	DSHOT_CMD_3D_MODE_OFF,
	DSHOT_CMD_3D_MODE_ON,
	DSHOT_CMD_SETTINGS_REQUEST, // Currently not implemented
	DSHOT_CMD_SAVE_SETTINGS,
	DSHOT_CMD_SPIN_DIRECTION_NORMAL = 20,
	DSHOT_CMD_SPIN_DIRECTION_REVERSED = 21,
	DSHOT_CMD_LED0_ON, // BLHeli32 only
	DSHOT_CMD_LED1_ON, // BLHeli32 only
	DSHOT_CMD_LED2_ON, // BLHeli32 only
	DSHOT_CMD_LED3_ON, // BLHeli32 only
	DSHOT_CMD_LED0_OFF, // BLHeli32 only
	DSHOT_CMD_LED1_OFF, // BLHeli32 only
	DSHOT_CMD_LED2_OFF, // BLHeli32 only
	DSHOT_CMD_LED3_OFF, // BLHeli32 only
	DSHOT_CMD_AUDIO_STREAM_MODE_ON_OFF = 30, // KISS audio Stream mode on/Off
	DSHOT_CMD_SILENT_MODE_ON_OFF = 31, // KISS silent Mode on/Off
	DSHOT_CMD_MAX = 47
} dshotCommands_e;

typedef enum {
	DSHOT_CMD_TYPE_INLINE = 0,    // dshot commands sent inline with motor signal (motors must be enabled)
	DSHOT_CMD_TYPE_BLOCKING       // dshot commands sent in blocking method (motors must be disabled)
} dshotCommandType_e;



#endif
