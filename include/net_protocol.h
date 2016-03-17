
#ifndef _NET_PROTOCOL_H_
#define _NET_PROTOCOL_H_

typedef enum {
	TCP_CMD_GET_DEVICES = 0x8803,
	TCP_CMD_GET_DEVICES_RESP = 0x8804,
	TCP_CMD_GET_DEVICE_INFO = 0x8805,
	TCP_CMD_GET_DEVICE_INFO_RESP = 0x8806,
	TCP_CMD_GET_DEVICE_STATUS = 0x8807,
	TCP_CMD_GET_DEVICE_STATUS_RESP = 0x8808,
	TCP_CMD_GET_DEVICE_CONFIG = 0x8809,
	TCP_CMD_GET_DEVICE_CONFIG_RESP = 0x880a,
	TCP_CMD_SET_DEVICE_STATUS = 0x880b,
	TCP_CMD_SET_DEVICE_CONFIG = 0x880c,
	TCP_CMD_DEL_DEVICE_CONFIG = 0x880d,
	TCP_CMD_PROBE_DEVICE = 0x880e,
	TCP_CMD_DEVICE_EVENT = 0x880f,
	TCP_CMD_RESULT = 0x8810,
	TCP_CMD_LAST,
} TCP_CMD_T;

#define TCP_CMD_VALID(x)	(x >= TCP_CMD_GET_DEVICES && x < TCP_CMD_LAST)

typedef enum {
	HSB_DEV_CLASS_SOCKET = 1,
	HSB_DEV_CLASS_SWITCH,
	HSB_DEV_CLASS_LAMP,
	HSB_DEV_CLASS_SENSOR,
	HSB_DEV_CLASS_REMOTE_CONTROL,
	HSB_DEV_CLASS_CAMERA,
	HSB_DEV_CLASS_SOUND_BOX,
	HSB_DEV_CLASS_DOORBELL,
	HSB_DEV_CLASS_ENTRANCE_GUARD,
	HSB_DEV_CLASS_TV,
	HSB_DEV_CLASS_AIR_CONDITIONING,
	HSB_DEV_CLASS_CURTAIN,
	HSB_DEV_CLASS_FRIDGE,
	HSB_DEV_CLASS_AIR_CLEANER,
	HSB_DEV_CLASS_WATER_HEATER,
	HSB_DEV_CLASS_OVEN,
	HSB_DEV_CLASS_MICROWAVE_OVEN,
	HSB_DEV_CLASS_LAMPBLACK,
	HSB_DEV_CLASS_FAN,
	HSB_DEV_CLASS_WARMING_OVEN,
	HSB_DEV_CLASS_HEATING,
	HSB_DEV_CLASS_STB,
	HSB_DEV_CLASS_BRACELET,
	HSB_DEV_CLASS_CUP,
	HSB_DEV_CLASS_ROUTER,
	HSB_DEV_CLASS_WASHER,
	HSB_DEV_CLASS_ELECTRIC_COOKER,
	HSB_DEV_CLASS_FAX,
	HSB_DEV_CLASS_PRINTER,
	HSB_DEV_CLASS_LAST,
} HSB_DEV_CLASS_T;

typedef enum {
	HSB_DEV_INTERFACE_WIFI = 0,
	HSB_DEV_INTERFACE_ZIGBEE,
	HSB_DEV_INTERFACE_BLUETOOTH,
	HSB_DEV_INTERFACE_IR,
	HSB_DEV_INTERFACE_NFC,
	HSB_DEV_INTERFACE_AM,
	HSB_DEV_INTERFACE_NFC,
	HSB_DEV_INTERFACE_LAST,
} HSB_DEV_INTERFACE_T;

typedef enum {
	HSB_DEV_FUNC_ON_OFF = 1,
	HSB_DEV_FUNC_ADJUST_BRIGHTNESS,
	HSB_DEV_FUNC_SET_COLOR,
	HSB_DEV_FUNC_IR_CONTROL,
	HSB_DEV_FUNC_RECORD_ON_OFF,
	HSB_DEV_FUNC_HUMAN_SENSOR,
	HSB_DEV_FUNC_DOOR_WINDOW_SENSOR,
	HSB_DEV_FUNC_FROG_SENSOR,
	HSB_DEV_FUNC_LIGHT_SENSOR,
	HSB_DEV_FUNC_GAS_SENSOR,
} HSB_DEV_FUNC_T;

typedef enum {
	/* device info value type */
	HSB_VAL_TYPE_DRV_ID = 1,
	HSB_VAL_TYPE_DEV_CLASS,
	HSB_VAL_TYPE_DEV_INTERFACE,
	HSB_VAL_TYPE_DEV_FUNC,
	HSB_VAL_TYPE_MAC,
} HSB_DEV_INFO_VAL_TYPE_T;

typedef enum {
	/* device status value type */
	HSB_VAL_TYPE_ON_OFF = 1,
	HSB_VAL_TYPE_BRIGHTNESS,
	HSB_VAL_TYPE_COLOR,
	HSB_VAL_TYPE_RECORD_ON_OFF,
} HSB_DEV_STATUS_VAL_TYPE_T;

typedef enum {
	/* device configuration value type */
	HSB_VAL_TYPE_TIMER = 1,
	HSB_VAL_TYPE_DELAY,
} HSB_DEV_CONFIG_VAL_TYPE_T;

typedef enum {
	/* device event */
	HSB_VAL_TYPE_EV_ON_OFF = 1,
	HSB_VAL_TYPE_EV_BRIGHTNESS,
	HSB_VAL_TYPE_EV_COLOR,
	HSB_VAL_TYPE_EV_REC_ON_OFF,
	HSB_VAL_TYPE_EV_DEV_ONLINE_OFFLINE,
	HSB_VAL_TYPE_EV_HUMAN_SENSOR,
	HSB_VAL_TYPE_EV_DW_SENSOR,
	HSB_VAL_TYPE_EV_FROG_SENSOR,
	HSB_VAL_TYPE_EV_LIGHT_SENSOR,
	HSB_VAL_TYPE_EV_GAS_SENSOR,
} HSB_DEV_EVENT_VAL_TYPE_T;


#endif

