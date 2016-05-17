

#include <string.h>
#include <glib.h>
#include "device.h"
#include "network_utils.h"
#include "debug.h"
#include "driver_cg_zigbee.h"
#include "thread_utils.h"
#include "device.h"
#include "hsb_error.h"

typedef struct {
	GQueue	queue;
	GMutex	mutex;

	int	fd;
} HSB_DEVICE_DRIVER_CONTEXT_T;

typedef struct {
	uint16_t	dev_class;
	uint16_t	interface;
	uint8_t		mac[6];
	uint8_t		resv[2];
	uint32_t	status_bm;
	uint32_t	event_bm;
	uint32_t	action_bm;
} CZ_INFO_T;

typedef enum {
	CZ_EVT_TYPE_SENSOR_TRIGGERED = 0,
	CZ_EVT_TYPE_SENSOR_RECOVERED,
	CZ_EVT_TYPE_KEY_PRESSED,
	CZ_EVT_TYPE_IR_KEY,
} CZ_EVT_TYPE_T;

typedef enum {
	CZ_STATUS_TYPE_ON_OFF = 1,
	CZ_STATUS_TYPE_RO_DOOR = 16,
} CZ_STATUS_TYPE_T;

typedef struct {
	uint32_t	id;
	struct in_addr	ip;
	uint32_t	idle_time;
	CZ_INFO_T	info;
} CZ_DEV_T;

static HSB_DEVICE_DRIVER_CONTEXT_T gl_ctx = { 0 };

static HSB_DEV_DRV_T cz_drv;

static int cz_probe(void)
{
	return HSB_E_OK;
}

static int sample_set_status(const HSB_STATUS_T *status)
{
	return HSB_E_OK;
}

static int sample_get_status(HSB_STATUS_T *status)
{
	return HSB_E_OK;
}

static int sample_set_action(const HSB_ACTION_T *act)
{
	return HSB_E_OK;
}

static HSB_DEV_OP_T sample_op = {
	sample_get_status,
	sample_set_status,
	sample_set_action,
};

static HSB_DEV_DRV_OP_T cz_drv_op = {
	cz_probe,
};

static HSB_DEV_DRV_T cz_drv = {
	"cg zigbee",
	2,
	&cz_drv_op,
};

#define SERIAL_INTERFACE	"/dev/ttyUSBS0"

int init_cz_drv(void)
{
	g_queue_init(&gl_ctx.queue);
	g_mutex_init(&gl_ctx.mutex);

	int fd = open_serial_fd(SERIAL_INTERFACE, 115200);
	if (fd < 0) {
		hsb_critical("open %s failed\n", SERIAL_INTERFACE);
		return HSB_E_OTHERS;
	}

	gl_ctx.fd = fd;

	return register_dev_drv(&cz_drv);
}

