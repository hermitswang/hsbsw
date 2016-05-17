
#include <glib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "device.h"
#include "debug.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "thread_utils.h"

typedef struct {
	GQueue			queue;
	GMutex			mutex;

	GQueue			offq;

	GQueue			driverq;
	uint32_t		dev_id;

	HSB_WORK_MODE_T		work_mode;
	uint32_t		sec_today;

	thread_data_control	async_thread_ctl;
} HSB_DEVICE_CB_T;

static HSB_DEVICE_CB_T gl_dev_cb = { 0 };

#define HSB_DEVICE_CB_LOCK()	do { \
	g_mutex_lock(&gl_dev_cb.mutex); \
} while (0)

#define HSB_DEVICE_CB_UNLOCK()	do { \
	g_mutex_unlock(&gl_dev_cb.mutex); \
} while (0)

int get_dev_id_list(uint32_t *dev_id, int *dev_num)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev;
	int num = 0;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
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

static HSB_DEV_T *_find_dev(uint32_t dev_id)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == dev_id)
			return pdev;
	}

	return NULL;
}

static HSB_DEV_DRV_T *_get_dev_drv(uint32_t devid)
{
	HSB_DEV_DRV_T *pdrv = NULL;
	HSB_DEV_T *pdev = NULL;

	HSB_DEVICE_CB_LOCK();

	pdev = _find_dev(devid);

	if (!pdev || !pdev->driver) {
		goto fail;
	}

	pdrv = pdev->driver;

	HSB_DEVICE_CB_UNLOCK();

	return pdrv;
fail:
	return NULL;
}

int get_dev_info(uint32_t dev_id, HSB_DEV_T *dev)
{
	int ret = 0;
	HSB_DEV_T *pdev = NULL;

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

int get_dev_status(HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	HSB_DEV_T *pdev = _find_dev(status->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->get_status)
		return pdev->op->get_status(status);

	return ret;
}

int set_dev_status(const HSB_STATUS_T *status)
{
	int ret = HSB_E_NOT_SUPPORTED;

	if (0 == status->devid && status->id[0] == HSB_STATUS_TYPE_WORK_MODE)
		return set_box_work_mode(status->val[0]);

	HSB_DEV_T *pdev = _find_dev(status->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->set_status)
		return pdev->op->set_status(status);

	return ret;
}

int set_dev_action(const HSB_ACTION_T *act)
{
	int ret = HSB_E_NOT_SUPPORTED;

	HSB_DEV_T *pdev = _find_dev(act->devid);

	if (!pdev)
		return HSB_E_ENTRY_NOT_FOUND;

	if (pdev->op && pdev->op->set_action)
		return pdev->op->set_action(act);

	return ret;
}

static HSB_DEV_DRV_T *_find_drv(uint32_t drv_id)
{
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *pdrv = NULL;
	int len, id;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEV_DRV_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->id == drv_id)
			return pdrv;
	}

	return NULL;

}

int probe_dev(uint32_t drv_id)
{
	HSB_DEV_DRV_T *pdrv = _find_drv(drv_id);

	if (!pdrv)
		return HSB_E_BAD_PARAM;

	if (pdrv->op && pdrv->op->probe)
		return pdrv->op->probe();

	return HSB_E_OK;
}

static uint32_t alloc_dev_id(void)
{
	uint32_t dev_id;

	HSB_DEVICE_CB_LOCK();

	dev_id = gl_dev_cb.dev_id++;

	HSB_DEVICE_CB_UNLOCK();
	
	return dev_id;
}


int register_dev_drv(HSB_DEV_DRV_T *drv)
{
	if (!drv)
		return HSB_E_BAD_PARAM;

	int len, id;
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *pdrv = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdrv = (HSB_DEV_DRV_T *)g_queue_peek_nth(queue, id);
		if (!pdrv) {
			hsb_critical("drv null\n");
			continue;
		}

		if (pdrv->id == drv->id) {
			hsb_critical("%s driver load fail, drv_id alreasy exists.\n", drv->id);
			return HSB_E_ENTRY_EXISTS;
		}
	}

	g_queue_push_tail(queue, drv);

	hsb_debug("%s driver loaded\n", drv->name);

	return HSB_E_OK;
}

HSB_DEV_T *find_dev_by_ip(struct in_addr *ip)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T	*pdev = NULL;

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (ip->s_addr == pdev->prty.ip.s_addr)
			return pdev;
	}

	return NULL;
}

HSB_DEV_T *create_dev(void)
{
	HSB_DEV_T *pdev = g_slice_new0(HSB_DEV_T);
	if (!pdev)
		return NULL;

	/* set default value */
	pdev->id = alloc_dev_id();
	pdev->work_mode = HSB_WORK_MODE_ALL;

	return pdev;
}

int destroy_dev(HSB_DEV_T *dev)
{
	g_slice_free(HSB_DEV_T, dev);

	return 0;
}

int register_dev(HSB_DEV_T *dev)
{
	guint len, id;
	GQueue *queue = &gl_dev_cb.queue;

	HSB_DEVICE_CB_LOCK();

	g_queue_push_tail(queue, dev);

	HSB_DEVICE_CB_UNLOCK();

	dev_updated(dev->id, HSB_DEV_UPDATED_TYPE_NEW_ADD);

	return 0;
}

int remove_dev(HSB_DEV_T *dev)
{
	GQueue *queue = &gl_dev_cb.queue;

	HSB_DEVICE_CB_LOCK();
	g_queue_remove(queue, dev);
	HSB_DEVICE_CB_UNLOCK();

	dev_updated(dev->id, HSB_DEV_UPDATED_TYPE_OFFLINE);

	return 0;
}

int dev_online(uint32_t drvid, HSB_DEV_INFO_T *info, uint32_t *devid, HSB_DEV_OP_T *op)
{
	int ret = HSB_E_OK;
	GQueue *offq = &gl_dev_cb.offq;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(offq);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(offq, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->driver->id == drvid &&
		    0 == memcmp(pdev->info.mac, info->mac, 6)) {
			break;
		}
	}

	HSB_DEVICE_CB_UNLOCK();

	if (id == len) { /* not found in offq */
		pdev = create_dev();
		pdev->driver = _find_drv(drvid);
		pdev->op = op;

		memcpy(&pdev->info, info, sizeof(*info));

		HSB_DEVICE_CB_LOCK();
		g_queue_push_tail(queue, pdev);
		HSB_DEVICE_CB_UNLOCK();

		dev_updated(pdev->id, HSB_DEV_UPDATED_TYPE_NEW_ADD);
	} else {
		g_queue_pop_nth(offq, id);

		HSB_DEVICE_CB_LOCK();
		g_queue_push_tail(queue, pdev);
		HSB_DEVICE_CB_UNLOCK();

		dev_updated(pdev->id, HSB_DEV_UPDATED_TYPE_ONLINE);
	}

	*devid = pdev->id;

	return ret;
}

int dev_offline(uint32_t devid)
{
	int ret = HSB_E_OK;
	GQueue *offq = &gl_dev_cb.offq;
	GQueue *queue = &gl_dev_cb.queue;
	HSB_DEV_T *pdev = NULL;
	guint len, id;

	HSB_DEVICE_CB_LOCK();

	len = g_queue_get_length(queue);
	for (id = 0; id < len; id++) {
		pdev = (HSB_DEV_T *)g_queue_peek_nth(queue, id);
		if (!pdev) {
			hsb_critical("device null\n");
			continue;
		}

		if (pdev->id == devid)
			break;
	}

	if (id == len) {
		hsb_critical("dev %d not found in queue\n", devid);
		HSB_DEVICE_CB_UNLOCK();
		return HSB_E_OTHERS;
	}

	g_queue_pop_nth(queue, id);

	g_queue_push_tail(offq, pdev);

	HSB_DEVICE_CB_UNLOCK();

	dev_updated(devid, HSB_DEV_UPDATED_TYPE_OFFLINE);

	return ret;
}

static int check_linkage(uint32_t devid, HSB_EVT_T *evt)
{
	HSB_DEV_T *pdev = _find_dev(devid);
	if (!pdev)
		return HSB_E_OTHERS;

	HSB_WORK_MODE_T work_mode = gl_dev_cb.work_mode;
	HSB_LINKAGE_T *link = pdev->link;
	HSB_LINKAGE_STATUS_T *status = pdev->link_status;

	if (!CHECK_BIT(pdev->work_mode, work_mode))
		return HSB_E_OTHERS;

	int id;
	uint8_t flag;

	for (id = 0; id < HSB_DEV_MAX_LINKAGE_NUM; id++, link++, status++) {
		if (!status->active)
			continue;

		if (!CHECK_BIT(link->work_mode, work_mode))
			continue;

		if (evt->id != link->evt_id ||
		    evt->param1 != link->evt_param1 ||
		    evt->param2 != link->evt_param2)
			continue;

		flag = link->flag;
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = link->act_devid;
			action.id = link->act_id;
			action.param1 = link->act_param1;
			action.param2 = link->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = link->act_devid;
			stat.num = 1;
			stat.id[0] = link->act_id;
			stat.val[0] = link->act_param1;
			set_dev_status_async(&stat, NULL);
		}
	}

	return HSB_E_OK;
}


static _dev_event(uint32_t devid, HSB_EVT_TYPE_T type, uint16_t param1, uint32_t param2)
{
	HSB_RESP_T resp = { 0 };

	resp.type = HSB_RESP_TYPE_EVENT;
	resp.reply = NULL;
	resp.u.event.devid = devid;
	resp.u.event.id = type;
	resp.u.event.param1 = param1;
	resp.u.event.param2 = param2;

	check_linkage(devid, &resp.u.event);

	hsb_debug("get event: %d, %d, %d, %x\n", devid, type, param1, param2);

	return notify_resp(&resp);
}

int dev_status_updated(uint32_t devid, HSB_STATUS_T *status)
{
	return _dev_event(devid, HSB_EVT_TYPE_STATUS_UPDATED, status->id[0], status->val[0]);
}

int dev_updated(uint32_t devid, HSB_DEV_UPDATED_TYPE_T type)
{
	return _dev_event(devid, HSB_EVT_TYPE_DEV_UPDATED, type, 0);
}

int dev_sensor_triggered(uint32_t devid, HSB_SENSOR_TYPE_T type)
{
	return _dev_event(devid, HSB_EVT_TYPE_SENSOR_TRIGGERED, type, 0);
}

int dev_sensor_recovered(uint32_t devid, HSB_SENSOR_TYPE_T type)
{
	return _dev_event(devid, HSB_EVT_TYPE_SENSOR_RECOVERED, type, 0);
}

int dev_ir_key(uint32_t devid, uint16_t param1, uint32_t param2)
{
	return _dev_event(devid, HSB_EVT_TYPE_IR_KEY, param1, param2);
}

int dev_mode_changed(HSB_WORK_MODE_T mode)
{
	return _dev_event(0, HSB_EVT_TYPE_MODE_CHANGED, mode, 0);
}

int set_box_work_mode(HSB_WORK_MODE_T mode)
{
	int ret = HSB_E_OK;
	if (!HSB_WORK_MODE_VALID(mode))
		return HSB_E_BAD_PARAM;

	gl_dev_cb.work_mode = mode;

	dev_mode_changed(mode);

	return ret;
}

HSB_WORK_MODE_T get_box_work_mode(void)
{
	return gl_dev_cb.work_mode;
}

static void probe_all_devices(void)
{
	GQueue *queue = &gl_dev_cb.driverq;
	HSB_DEV_DRV_T *drv;
	int id, len = g_queue_get_length(queue);

	for (id = 0; id < len; id++) {
		drv = g_queue_peek_nth(queue, id);
		drv->op->probe();
	}
}

int probe_dev_async(const HSB_PROBE_T *probe, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_PROBE;
	act->reply = reply;
	act->u.probe.drvid = probe->drvid;

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int set_dev_status_async(const HSB_STATUS_T *status, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_SET_STATUS;
	act->reply = reply;
	memcpy(&act->u.status, status, sizeof(*status));

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int get_dev_status_async(uint32_t devid, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_GET_STATUS;
	act->reply = reply;
	act->u.status.devid = devid;

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

int set_dev_action_async(const HSB_ACTION_T *action, void *reply)
{
	HSB_ACT_T *act = g_slice_new0(HSB_ACT_T);
	if (!act)
		return HSB_E_NO_MEMORY;

	act->type = HSB_ACT_TYPE_DO_ACTION;
	act->reply = reply;
	memcpy(&act->u.action, action, sizeof(*action));

	thread_control_push_data(&gl_dev_cb.async_thread_ctl, act);

	return HSB_E_OK;
}

void _process_dev_act(HSB_ACT_T *act)
{
	int ret;
	HSB_ACT_TYPE_T type = act->type;
	HSB_RESP_T resp = { 0 };
	void *reply = act->reply;
	resp.reply = reply;

	switch (type) {
		case HSB_ACT_TYPE_PROBE:
		{
			ret = probe_dev(act->u.probe.drvid);
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.ret_val = ret;

			break;
		}
		case HSB_ACT_TYPE_SET_STATUS:
		{
			ret = set_dev_status(&act->u.status);
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.ret_val = ret;

			break;
		}
		case HSB_ACT_TYPE_GET_STATUS:
		{
			ret = get_dev_status(&act->u.status);

			if (HSB_E_OK != ret) {
				resp.type = HSB_RESP_TYPE_RESULT;
				resp.u.result.ret_val = ret;
			} else {
				resp.type = HSB_RESP_TYPE_STATUS;
				memcpy(&resp.u.status, &act->u.status, sizeof(HSB_STATUS_T));
			}
			
			break;
		}
		case HSB_ACT_TYPE_DO_ACTION:
		{
			ret = set_dev_action(&act->u.action);
			if (!reply)
				return;

			if (act->u.action.id == HSB_ACT_TYPE_REMOTE_CONTROL)
				usleep(200000);

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.ret_val = ret;

			break;
		}
		default:
			hsb_debug("unkown act type\n");
			if (!reply)
				return;

			resp.type = HSB_RESP_TYPE_RESULT;
			resp.u.result.ret_val = HSB_E_NOT_SUPPORTED;
			break;
	}

	notify_resp(&resp);

	return;
}

static void *async_process_thread(thread_data_control *thread_data)
{
	HSB_ACT_T *data = NULL;

	pthread_mutex_lock(&thread_data->mutex);
	
	while (thread_data->active)
	{
		while (0 == g_queue_get_length(thread_data->data_queue) &&
			thread_data->active)
		{
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec++;
			int error_no = pthread_cond_timedwait(&thread_data->cond, &thread_data->mutex, &ts);
			if (ETIMEDOUT == error_no)
				continue;
		}

		if (thread_data->active == FALSE)
			break;

		data = (HSB_ACT_T *)g_queue_pop_head(thread_data->data_queue);
		pthread_mutex_unlock(&thread_data->mutex);
		if (data == NULL)
		{
			pthread_mutex_lock(&thread_data->mutex);
			continue;
		}

		/* process data */
		_process_dev_act(data);

		/* free data */
		g_slice_free(HSB_ACT_T, data);

		pthread_mutex_lock(&thread_data->mutex);
	}

	pthread_mutex_unlock(&thread_data->mutex);

	return NULL;
}

static int init_private_thread(void)
{
	thread_data_control *tptr;

	tptr = &gl_dev_cb.async_thread_ctl;

 	thread_control_init(tptr);
	thread_control_activate(tptr);

	pthread_t thread_id;
	if (pthread_create(&tptr->thread_id, NULL, (thread_entry_func)async_process_thread, tptr))
	{
		hsb_critical("create async thread failed\n");
		return -1;
	}

	return 0;
}

int init_dev_module(void)
{
	g_queue_init(&gl_dev_cb.queue);
	g_mutex_init(&gl_dev_cb.mutex);
	g_queue_init(&gl_dev_cb.driverq);
	g_queue_init(&gl_dev_cb.offq);

	/* reserve hsb id=0 */
	gl_dev_cb.dev_id = 1;

	init_private_thread();

	init_virtual_switch_drv();

	probe_all_devices();

	return 0;
}

int get_dev_timer(uint32_t dev_id, uint16_t timer_id, HSB_TIMER_T *timer)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memcpy(timer, tm, sizeof(*tm));

_out:
	return ret;
}

int set_dev_timer(uint32_t dev_id, const HSB_TIMER_T *timer)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t timer_id = timer->id;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	memcpy(tm, timer, sizeof(*tm));
	memset(status, 0, sizeof(*status));
	status->active = true;
	status->expired = false;

_out:
	return ret;
}

int del_dev_timer(uint32_t dev_id, uint16_t timer_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (timer_id >= (sizeof(dev->timer) / sizeof(dev->timer[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_TIMER_T *tm = &dev->timer[timer_id];
	HSB_TIMER_STATUS_T *status = &dev->timer_status[timer_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memset(tm, 0, sizeof(*tm));
	memset(status, 0, sizeof(*status));

_out:
	return ret;
}

int get_dev_delay(uint32_t dev_id, uint16_t delay_id, HSB_DELAY_T *delay)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memcpy(delay, dl, sizeof(*dl));

_out:
	return ret;
}

int set_dev_delay(uint32_t dev_id, const HSB_DELAY_T *delay)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t delay_id = delay->id;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	memcpy(dl, delay, sizeof(*dl));
	status->active = true;

_out:
	return ret;
}

int del_dev_delay(uint32_t dev_id, uint16_t delay_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (delay_id >= (sizeof(dev->delay) / sizeof(dev->delay[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_DELAY_T *dl = &dev->delay[delay_id];
	HSB_DELAY_STATUS_T *status = &dev->delay_status[delay_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memset(dl, 0, sizeof(*dl));
	status->active = false;

_out:
	return ret;
}

int get_dev_linkage(uint32_t dev_id, uint16_t link_id, HSB_LINKAGE_T *link)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memcpy(link, lk, sizeof(*lk));

_out:
	return ret;

}

int set_dev_linkage(uint32_t dev_id, const HSB_LINKAGE_T *link)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;
	uint16_t link_id = link->id;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	memcpy(lk, link, sizeof(*lk));
	status->active = true;

_out:
	return ret;
}


int del_dev_linkage(uint32_t dev_id, uint16_t link_id)
{
	HSB_DEV_T *dev;
	int ret = HSB_E_OK;

	dev = _find_dev(dev_id);

	if (!dev) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	if (link_id >= (sizeof(dev->link) / sizeof(dev->link[0]))) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	HSB_LINKAGE_T *lk = &dev->link[link_id];
	HSB_LINKAGE_STATUS_T *status = &dev->link_status[link_id];

	if (!status->active) {
		ret = HSB_E_BAD_PARAM;
		goto _out;
	}

	memset(lk, 0, sizeof(*lk));
	status->active = false;

_out:
	return ret;
}

static void _check_dev_timer_and_delay(void *data, void *user_data)
{
	HSB_DEV_T *pdev = (HSB_DEV_T *)data;

	if (!pdev) {
		hsb_critical("null device\n");
		return;
	}

	HSB_WORK_MODE_T work_mode = gl_dev_cb.work_mode;

	time_t now = time(NULL);
	struct tm tm_now;
	localtime_r(&now, &tm_now);
	uint32_t sec_today = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;
	uint32_t sec_timer;
	bool bnextday = false;
	int cnt;
	uint8_t flag, weekday;
	HSB_TIMER_T *ptimer = pdev->timer;
	HSB_TIMER_STATUS_T *tstatus = pdev->timer_status;
	HSB_DELAY_T *pdelay = pdev->delay;
	HSB_DELAY_STATUS_T *dstatus = pdev->delay_status;

	if (sec_today < gl_dev_cb.sec_today && sec_today < 10) {
		bnextday = true;
		
		for (cnt = 0; cnt < HSB_DEV_MAX_TIMER_NUM; cnt++, ptimer++, tstatus++) {
			if (!tstatus->active)
				continue;

			weekday = ptimer->wday;
			if (!CHECK_BIT(weekday, tm_now.tm_wday))
				tstatus->expired = true;
			else
				tstatus->expired = false;
		}
	}

	gl_dev_cb.sec_today = sec_today;

	if (!CHECK_BIT(pdev->work_mode, work_mode))
		return;

	ptimer = pdev->timer;
	tstatus = pdev->timer_status;

	for (cnt = 0; cnt < HSB_DEV_MAX_TIMER_NUM; cnt++, ptimer++, tstatus++) {
		/* 1.chekc active & expired */
		if (!tstatus->active || tstatus->expired)
			continue;

		/* 2.check work mode */
		if (!CHECK_BIT(ptimer->work_mode, work_mode))
			continue;

		/* 3.check flag & weekday */
		flag = ptimer->flag;
		weekday = ptimer->wday;
		if (!CHECK_BIT(weekday, tm_now.tm_wday)) {
			continue;
		}

		/* 4.check time */
		sec_timer = ptimer->hour * 3600 + ptimer->min * 60 + ptimer->sec;

		if (sec_timer > sec_today || sec_today - sec_timer > 5)
			continue;

		/* 5.do action */
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = pdev->id;
			action.id = ptimer->act_id;
			action.param1 = ptimer->act_param1;
			action.param2 = ptimer->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = pdev->id;
			stat.num = 1;
			stat.id[0] = ptimer->act_id;
			stat.val[0] = ptimer->act_param1;
			set_dev_status_async(&stat, NULL);
		}

		if (CHECK_BIT(weekday, 7)) { /* One shot */
			memset(ptimer, 0, sizeof(*ptimer));
			memset(tstatus, 0, sizeof(*tstatus));
			continue;
		}

		tstatus->expired = true;
	}

	pdelay = pdev->delay;
	dstatus = pdev->delay_status;

	for (cnt = 0; cnt < HSB_DEV_MAX_DELAY_NUM; cnt++, pdelay++, dstatus++) {
		/* check active & started */
		if (!dstatus->active || !dstatus->started)
			continue;

		/* check work mode */
		if (!CHECK_BIT(pdelay->work_mode, work_mode))
			continue;

		/* check time */
		if (now - dstatus->start_tm < pdelay->delay_sec)
			continue;

		/* do action */
		flag = pdelay->flag;
		if (CHECK_BIT(flag, 0)) {
			HSB_ACTION_T action;
			action.devid = pdev->id;
			action.id = pdelay->act_id;
			action.param1 = pdelay->act_param1;
			action.param2 = pdelay->act_param2;
			set_dev_action_async(&action, NULL);
		} else  {
			HSB_STATUS_T stat;
			stat.devid = pdev->id;
			stat.num = 1;
			stat.id[0] = pdelay->act_id;
			stat.val[0] = pdelay->act_param1;
			set_dev_status_async(&stat, NULL);
		}

		dstatus->started = false;
	}

	return;
}

int check_timer_and_delay(void)
{
	GQueue *queue = &gl_dev_cb.queue;
	g_queue_foreach(queue, _check_dev_timer_and_delay, NULL);

	return HSB_E_OK;
}



