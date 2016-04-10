
#ifndef _REMOTE_CTL_H_
#define _REMOTE_CTL_H_

#include <stdint.h>
#include "hsb_device.h"
#include "device.h"

typedef enum {
	HSB_REMOTE_KEY_0 = 0,
	HSB_REMOTE_KEY_1,
	HSB_REMOTE_KEY_2,
	HSB_REMOTE_KEY_3,
	HSB_REMOTE_KEY_4,
	HSB_REMOTE_KEY_5,
	HSB_REMOTE_KEY_6,
	HSB_REMOTE_KEY_7,
	HSB_REMOTE_KEY_8,
	HSB_REMOTE_KEY_9,
	HSB_REMOTE_KEY_OK,
	HSB_REMOTE_KEY_BACK,
	HSB_REMOTE_KEY_INVALID = 255,
} HSB_REMOTE_KEY_T;

int remote_key_mapping(uint16_t drvid, HSB_REMOTE_KEY_T key, HSB_ACTION_T *act);

#endif