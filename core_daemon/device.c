
#include <glib.h>
#include <string.h>
#include "device.h"
#include "debug.h"
#include "hsb_error.h"

typedef struct {
	GQueue		queue;
	GMutex		mutex;

	GQueue		driverq;
	uint32_t	dev_id;
} HSB_DEVICE_CB_T;

static HSB_DEVICE_CB_T gl_device_cb = { 0 };

#define HSB_DEVICE_CB_LOCK()	do { \
	g_mutex_lock(&gl_device_cb.mutex); \
} while (0)

#define HSB_DEVICE_CB_UNLOCK()	do { \
	g_mutex_unlock(&gl_device_cb.mutex); \
} while (0)

int get_dev_id_list(uint32_t *dev_id, int *dev_num)
{
	guint len, id;
	GQueue *queue = &gl_device_cb.queue;
	HSB_DEVICE_T	*pdev;
	int num = 0;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEVICE_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			num = 0;
			break;
		}

		dev_id[num] = pdev->id;
		num++;
	}

	HSB_DEVICE_CB_UNLOCK();

	*dev_num = num;

	return 0;
}

static HSB_DEVICE_T *_find_dev(uint32_t dev_id)
{
	guint len, id;
	GQueue *queue = &gl_device_cb.queue;
	HSB_DEVICE_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEVICE_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == dev_id)
			return pdev;
	}

	return NULL;
}

int get_dev_info(uint32_t dev_id, HSB_DEVICE_T *dev)
{
	int ret = 0;
	HSB_DEVICE_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = _find_dev(dev_id);
	if (pdev) {
		memcpy(dev, pdev, sizeof(*pdev));
	} else {
		ret = -1;
	}

	HSB_DEVICE_CB_UNLOCK();

	return ret;
}

int get_dev_status(uint32_t dev_id, uint32_t *status)
{
	int ret;

	HSB_DEVICE_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = _find_dev(dev_id);

	if (!pdev || !pdev->driver) {
		ret = -1;
		goto fail;
	}

	if (pdev->driver->op->get_status)
		ret = pdev->driver->op->get_status(pdev, status);

	HSB_DEVICE_CB_UNLOCK();

	return ret;

fail:
	HSB_DEVICE_CB_UNLOCK();
	return ret;
}

int set_dev_status(uint32_t dev_id, uint32_t *status)
{
	int ret;

	HSB_DEVICE_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = _find_dev(dev_id);

	if (!pdev || !pdev->driver) {
		ret = -1;
		goto fail;
	}

	if (pdev->driver->op->set_status)
		ret = pdev->driver->op->set_status(pdev, status);

	HSB_DEVICE_CB_UNLOCK();

	return ret;

fail:
	HSB_DEVICE_CB_UNLOCK();
	return ret;
}

static HSB_DEVICE_DRIVER_T *_find_drv(uint32_t drv_id)
{
	GQueue *queue = &gl_device_cb.driverq;
	HSB_DEVICE_DRIVER_T *pdrv = NULL;
	int len, id;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEVICE_DRIVER_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->drv_id == drv_id)
			return pdrv;
	}

	return NULL;

}

int probe_dev(uint32_t drv_id)
{
	HSB_DEVICE_DRIVER_T *pdrv = _find_drv(drv_id);

	if (!pdrv)
		return HSB_E_BAD_PARAMETERS;

	return pdrv->op->probe();
}

static uint32_t alloc_dev_id(void)
{
	uint32_t dev_id;

	HSB_DEVICE_CB_LOCK();

	dev_id = gl_device_cb.dev_id++;

	HSB_DEVICE_CB_UNLOCK();
	
	return dev_id;
}


int register_dev_drv(HSB_DEVICE_DRIVER_T *drv)
{
	if (!drv)
		return HSB_E_BAD_PARAMETERS;

	int len, id;
	GQueue *queue = &gl_device_cb.driverq;
	HSB_DEVICE_DRIVER_T *pdrv = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEVICE_DRIVER_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->drv_id == drv->drv_id) {
			hsb_critical("%s driver load fail, drv_id alreasy exists.\n", drv->drv_id);
			return HSB_E_ENTRY_EXISTS;
		}
	}

	g_queue_push_tail(queue, drv);

	hsb_debug("%s driver loaded\n", drv->name);

	return HSB_E_OK;
}

HSB_DEVICE_T *find_dev_by_ip(struct in_addr *ip)
{
	guint len, id;
	GQueue *queue = &gl_device_cb.queue;
	HSB_DEVICE_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEVICE_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (ip->s_addr == pdev->prty.ip.s_addr)
			return pdev;
	}

	return NULL;
}

HSB_DEVICE_T *create_dev(void)
{
	HSB_DEVICE_T *pdev = g_slice_new0(HSB_DEVICE_T);
	if (pdev)
		pdev->id = alloc_dev_id();

	return pdev;
}

int destroy_dev(HSB_DEVICE_T *dev)
{
	g_slice_free(HSB_DEVICE_T, dev);

	return 0;
}

int register_dev(HSB_DEVICE_T *dev)
{
	guint len, id;
	GQueue *queue = &gl_device_cb.queue;

	HSB_DEVICE_CB_LOCK();
	g_queue_push_tail(queue, dev);
	HSB_DEVICE_CB_UNLOCK();

	notify_dev_added(dev->id);

	return 0;
}

int remove_dev(HSB_DEVICE_T *dev)
{
	GQueue *queue = &gl_device_cb.queue;

	HSB_DEVICE_CB_LOCK();
	g_queue_remove(queue, dev);
	HSB_DEVICE_CB_UNLOCK();

	notify_dev_deled(dev->id);

	return 0;
}

int update_dev_status(HSB_DEVICE_T *dev, uint32_t *status)
{
	return notify_dev_status_updated(dev->id, status);
}

static void probe_all_devices(void)
{
	GQueue *queue = &gl_device_cb.driverq;
	HSB_DEVICE_DRIVER_T *drv;
	int id, len = g_queue_get_length(queue);

	for (id = 0; id < len; id++) {
		drv = g_queue_peek_nth(queue, id);
		drv->op->probe();
	}
}

int init_dev_module(void)
{
	g_queue_init(&gl_device_cb.queue);
	g_mutex_init(&gl_device_cb.mutex);
	g_queue_init(&gl_device_cb.driverq);

	init_virtual_switch_drv();

	probe_all_devices();

	return 0;
}


