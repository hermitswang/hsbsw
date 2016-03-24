

#include <string.h>
#include <glib.h>
#include "device.h"
#include "network_utils.h"
#include "debug.h"
#include "driver_virtual_switch.h"
#include "thread_utils.h"
#include "device.h"
#include "hsb_error.h"

typedef struct {
	GQueue	queue;
	GMutex	mutex;
} HSB_DEVICE_DRIVER_CONTEXT_T;

typedef struct {
	uint16_t	dev_class;
	uint16_t	interface;
	uint8_t		mac[6];
	uint8_t		resv[2];
	uint32_t	status_bm;
	uint32_t	event_bm;
	uint32_t	action_bm;
} VS_INFO;

typedef enum {
	VS_EVT_TYPE_SENSOR_TRIGGERED,
	VS_EVT_TYPE_SENSOR_RECOVERED,
	VS_EVT_TYPE_KEY_PRESSED,
} VS_EVT_TYPE_T;

typedef enum {
	VS_STATUS_TYPE_ON_OFF = 1,
	VS_STATUS_TYPE_RO_DOOR = 16,
} VS_STATUS_TYPE_T;

static HSB_DEVICE_DRIVER_CONTEXT_T gl_ctx = { 0 };

static HSB_DEV_DRV_T virtual_switch_drv;
static int _register_device(struct in_addr *addr);

static int virtual_switch_probe(void)
{
	struct sockaddr_in servaddr, devaddr;
	socklen_t slen = sizeof(servaddr);
	socklen_t devlen = sizeof(devaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count = 0;

	//hsb_debug("probe virtual switch\n");

	/* 1.create udp socket */
	int sockfd = open_udp_clientfd();

	/* 2.send broadcast packet, 192.168.2.255:19001 */
	if (get_broadcast_address(sockfd, "eth0", &servaddr.sin_addr)) {
		close(sockfd);
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(VIRTUAL_SWITCH_LISTEN_PORT);

	set_broadcast(sockfd, true);

	memset(sbuf, 0, sizeof(sbuf));
	count = 8;
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_DEVICE_DISCOVER);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	/* 3.wait for response in 1 second */
	struct timeval tv = { 3, 0 };
	
	do {
		ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), (struct sockaddr *)&devaddr, &devlen, &tv);
		if (ret < count) {
			hsb_critical("probe: get err pkt, len=%d\n", ret);
			continue;
		}

		int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
		int len = GET_CMD_FIELD(rbuf, 2, uint16_t);

		if (cmd != VS_CMD_DEVICE_DISCOVER_RESP || len != count) {
			hsb_critical("probe: get err pkt, cmd=%x, len=%d\n", cmd, len);
			continue;
		}

		if (find_dev_by_ip(&devaddr.sin_addr))
			continue;

		_register_device(&devaddr.sin_addr);

	} while (tv.tv_sec > 0 && tv.tv_usec > 0);

	close(sockfd);
	//hsb_debug("probe virtual switch done\n");

	return 0;
}

static int _transfer(HSB_DEV_T *pdev, uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	int ret;

	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, &pdev->prty.ip, VIRTUAL_SWITCH_LISTEN_PORT);

	sendto(sockfd, wbuf, wlen, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, rlen, NULL, NULL, &tv);

	if (ret <= 0) {
		hsb_critical("_transfer: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	close(sockfd);

	return ret;
}

static int virtual_switch_set_status(struct _HSB_DEV_T *pdev, const HSB_STATUS_T *status, int num)
{
	int ret, len;
	uint8_t wbuf[64];
	uint8_t rbuf[64];
	HSB_STATUS_T *pstat = (HSB_STATUS_T *)status;

	len = 4 + 4 * num;
	memset(wbuf, 0, sizeof(wbuf));
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_SET_STATUS);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);

	int id;
	for (id = 0; id < num; id++) {
		SET_CMD_FIELD(wbuf, 4 + id * 4, uint16_t, pstat->id);
		SET_CMD_FIELD(wbuf, 6 + id * 4, uint16_t, pstat->val);

		pstat++;
	}

	ret = _transfer(pdev, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("set status: get err pkt, len=%d\n", ret);
		return -1;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || rlen != ret) {
		hsb_critical("set status: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return -2;
	}

	if (!result) {
		hsb_debug("set status result=%d\n", result);
		return -3;
	}

	//hsb_debug("send set status to device done\n");

	return 0;
}

static int virtual_switch_get_status(struct _HSB_DEV_T *pdev, HSB_STATUS_T *status, int *num)
{
	uint8_t wbuf[64], rbuf[64];
	int ret, len;

	memset(wbuf, 0, sizeof(wbuf));
	len = 8;
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_GET_STATUS);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);
	SET_CMD_FIELD(wbuf, 4, uint32_t, 0xffffffff);

	ret = _transfer(pdev, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("get status: get err pkt, len=%d\n", ret);
		return -1;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);

	if (cmd != VS_CMD_GET_STATUS_RESP || ret != rlen) {
		hsb_critical("get status: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return -2;
	}

	int status_num = 0;
	uint16_t id, val;
	len = 4;

	while (len + 4 <= rlen) {
		status->id = GET_CMD_FIELD(rbuf, len, uint16_t);
		status->val = GET_CMD_FIELD(rbuf, len + 2, uint16_t);

		status_num ++;
		status ++;
		len += 4;

		if (status_num >= *num)
			break;
	}

	*num = status_num;

	//hsb_debug("send get status to device done, num=%d\n", *num);

	return HSB_E_OK;
}

static int virtual_switch_set_action(struct _HSB_DEV_T *pdev, const HSB_ACTION_T *act)
{
	int ret, len;
	uint8_t wbuf[64];
	uint8_t rbuf[64];
	
	len = 8;
	memset(wbuf, 0, sizeof(wbuf));
	SET_CMD_FIELD(wbuf, 0, uint16_t, VS_CMD_DO_ACTION);
	SET_CMD_FIELD(wbuf, 2, uint16_t, len);
	SET_CMD_FIELD(wbuf, 4, uint16_t, act->id);
	SET_CMD_FIELD(wbuf, 6, uint16_t, act->param);

	ret = _transfer(pdev, wbuf, len, rbuf, sizeof(rbuf));

	if (ret < MIN_VS_CMD_LEN) {
		hsb_critical("set action: get err pkt, len=%d\n", ret);
		return -1;
	}

	uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	uint16_t rlen = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || rlen != ret) {
		hsb_critical("set action: get err pkt, cmd=%x, len=%d\n", cmd, rlen);
		return -2;
	}

	if (!result) {
		hsb_debug("set action result=%d\n", result);
		return -3;
	}

	//hsb_debug("send set action to device done\n");

	return HSB_E_OK;
}

static HSB_DEV_OP_T virtual_switch_op = {
	virtual_switch_probe,
	virtual_switch_get_status,
	virtual_switch_set_status,
	virtual_switch_set_action,
};

static HSB_DEV_DRV_T virtual_switch_drv = {
	"virtual switch",
	1,
	&virtual_switch_op,
};

static int _remove_timeout_dev(void)
{
	GQueue *queue = &gl_ctx.queue;
	int len = g_queue_get_length(queue);
	int id;
	HSB_DEV_T *pdev;

	for (id = 0; id < len; ) {
		pdev = g_queue_peek_nth(queue, id);
		if (!pdev)
			break;

		if (pdev->idle_time > 10) {
			remove_dev(pdev);
			g_queue_pop_nth(queue, id);
			hsb_debug("destroy a device %s\n", inet_ntoa(pdev->prty.ip));
			destroy_dev(pdev);

			continue;
		}

		pdev->idle_time++;

		id++;
	}

	return 0;
}

static HSB_DEV_T *_find_device(struct in_addr *addr)
{
	GQueue *queue = &gl_ctx.queue;
	int len = g_queue_get_length(queue);
	int id;
	HSB_DEV_T *pdev;

	for (id = 0; id < len; id++) {
		pdev = g_queue_peek_nth(queue, id);

		if (pdev->prty.ip.s_addr == addr->s_addr)
			return pdev;
	}

	return NULL;
}

static int _get_device_info(struct in_addr *addr, VS_INFO *info)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count = 4;
	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, addr, VIRTUAL_SWITCH_LISTEN_PORT);

	memset(sbuf, 0, sizeof(sbuf));
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_GET_INFO);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), NULL, NULL, &tv);

	if (ret < count) {
		hsb_critical("get info: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	int len = GET_CMD_FIELD(rbuf, 2, uint16_t);

	if (cmd != VS_CMD_GET_INFO_RESP || len != 28) {
		hsb_critical("get info: get err pkt, cmd=%x, len=%d\n", cmd, len);
		close(sockfd);
		return -2;
	}

	memcpy(info, rbuf + 4, 24);

	close(sockfd);

	//hsb_debug("get info done\n");

	return HSB_E_OK;
}

static int _register_device(struct in_addr *addr)
{
	GQueue *queue = &gl_ctx.queue;
	HSB_DEV_T *pdev = create_dev();
	if (!pdev) {
		hsb_critical("create dev fail\n");
		return -1;
	}

	memcpy(&pdev->prty.ip, addr, sizeof(struct in_addr));
	pdev->driver = &virtual_switch_drv;

	VS_INFO info;

	if (_get_device_info(addr, &info)) {
		hsb_debug("dev get info fail\n");
		destroy_dev(pdev);
		return -2;
	}

	pdev->dev_class = info.dev_class;
	pdev->interface = info.interface;
	memcpy(pdev->mac, info.mac, 6);

	register_dev(pdev);
	hsb_debug("register a device %s\n", inet_ntoa(*addr));

	g_queue_push_tail(queue, pdev);

	return 0;
}

static int _refresh_device(HSB_DEV_T *pdev)
{
	pdev->idle_time = 0;

	return 0;
}

static int _status_updated(uint32_t devid, uint16_t id, uint16_t val)
{
	HSB_STATUS_T status;
	// TODO
	status.id = id;
	status.val = val;

	return dev_status_updated(devid, &status);
}

static int _key(uint16_t key)
{
	int ret = HSB_E_OK;

	switch (key) {
		case HSB_WORK_MODE_HOME:
		case HSB_WORK_MODE_OUTSIDE:
		case HSB_WORK_MODE_GUARD:
			ret = set_box_work_mode(key);
			break;
		default:
			hsb_debug("get undefined key %d\n", key);
			ret = HSB_E_OTHERS;
			break;
	}

	return ret;
}

static int _event(uint32_t devid, uint16_t id, uint16_t param)
{
	int ret = HSB_E_OK;

	switch (id) {
		case VS_EVT_TYPE_SENSOR_TRIGGERED:
			ret = dev_sensor_triggered(devid, param);
			break;
		case VS_EVT_TYPE_SENSOR_RECOVERED:
			ret = dev_sensor_recovered(devid, param);
			break;
		case VS_EVT_TYPE_KEY_PRESSED:
			ret = _key(param);
			break;
		default:
			hsb_debug("unknown event %x\n", id);
			ret = HSB_E_OTHERS;
			break;
	}

	return ret;
}

static void *_monitor_thread(void *arg)
{
	int fd = open_udp_listenfd(VIRTUAL_SWITCH_BOX_LISTEN_PORT);
	fd_set rset;
	int ret;
	struct timeval tv;
	struct sockaddr_in mc_addr, dev_addr;
	socklen_t mc_len = sizeof(mc_addr);
	socklen_t dev_len = sizeof(dev_addr);
	uint8_t sbuf[16], rbuf[16];
	int cmd_len;

	set_broadcast(fd, true);

	get_broadcast_address(fd, ETH_INTERFACE, &mc_addr.sin_addr);
	mc_addr.sin_family = AF_INET;
	mc_addr.sin_port = htons(VIRTUAL_SWITCH_LISTEN_PORT);

	while (1) {
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		ret = select(fd+1, &rset, NULL, NULL, &tv);

		if (ret < 0)
			continue;

		if (0 == ret) {	/* timeout, send keep alive */

			_remove_timeout_dev();

			memset(sbuf, 0, sizeof(sbuf));

			cmd_len = 4;
			SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_KEEP_ALIVE);
			SET_CMD_FIELD(sbuf, 2, uint16_t, cmd_len);
			
			sendto(fd, sbuf, cmd_len, 0, (struct sockaddr *)&mc_addr, mc_len);
			continue;
		}

		cmd_len = 16;
		/* get message */
		dev_len = sizeof(struct sockaddr_in);
		ret = recvfrom(fd, rbuf, cmd_len, 0, (struct sockaddr *)&dev_addr, &dev_len);

		if (ret < 4)
			continue;

		uint16_t cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
		uint16_t len = GET_CMD_FIELD(rbuf, 2, uint16_t);

		if (len != ret) {
			hsb_debug("error cmd: %d, %d\n", len, ret);
			continue;
		}

		//hsb_debug("get a cmd: %x\n", cmd);

		HSB_DEV_T *pdev = find_dev_by_ip(&dev_addr.sin_addr);
		if (!pdev) {
			_register_device(&dev_addr.sin_addr);
			pdev = find_dev_by_ip(&dev_addr.sin_addr);
		}

		switch (cmd) {
			case VS_CMD_KEEP_ALIVE:
			{
				break;
			}
			case VS_CMD_STATUS_CHANGED:
			{
				uint16_t id = GET_CMD_FIELD(rbuf, 4, uint16_t);
				uint16_t val = GET_CMD_FIELD(rbuf, 6, uint16_t);

				_status_updated(pdev->id, id, val);
				break;
			}
			case VS_CMD_EVENT:
			{
				uint16_t id = GET_CMD_FIELD(rbuf, 4, uint16_t);
				uint16_t param = GET_CMD_FIELD(rbuf, 6, uint16_t);

				_event(pdev->id, id, param);
				break;
			}
			default:
				break;
		}
	
		_refresh_device(pdev);
	}

	return NULL;
}

int init_virtual_switch_drv(void)
{
	g_queue_init(&gl_ctx.queue);
	g_mutex_init(&gl_ctx.mutex);

	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)_monitor_thread, NULL))
		return -1;

	return register_dev_drv(&virtual_switch_drv);
}

