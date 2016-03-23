

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
	VS_STATUS_TYPE_ON_OFF = 1,
	VS_STATUS_TYPE_RO_DOOR = 16,
} VS_STATUS_TYPE_T;

void _vs_status_to_dev_status(uint8_t id, uint16_t val, uint16_t *status_id, uint16_t *status_val)
{
	switch (id) {
		case VS_STATUS_TYPE_ON_OFF:
			*status_id = HSB_STATUS_TYPE_ON_OFF;
			*status_val = val;
			break;
		case VS_STATUS_TYPE_RO_DOOR:
			*status_id = HSB_STATUS_TYPE_SENSOR;
			*status_val = val;
		default:
			hsb_debug("error vs status id: %d\n", id);
			break;
	}
}

void _dev_status_to_vs_status(uint16_t status_id, uint16_t status_val, uint8_t *id, uint16_t *val)
{
	switch (status_id) {
		case HSB_STATUS_TYPE_ON_OFF:
			*id = VS_STATUS_TYPE_ON_OFF;
			*val = status_val;
			break;
		default:
			hsb_debug("error dev status id: %d\n", status_id);
			break;
	}
}

static HSB_DEVICE_DRIVER_CONTEXT_T gl_ctx = { 0 };

static HSB_DEV_DRV_T virtual_switch_drv;
static int _register_device(struct in_addr *addr);
static int _update_status(HSB_DEV_T *pdev, HSB_STATUS_T *status);

static int virtual_switch_probe(void)
{
	struct sockaddr_in servaddr, devaddr;
	socklen_t slen = sizeof(servaddr);
	socklen_t devlen = sizeof(devaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count = 16;

	hsb_debug("probe virtual switch\n");

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
	hsb_debug("probe virtual switch done\n");

	return 0;
}

static int virtual_switch_set_status(struct _HSB_DEV_T *pdev, const HSB_STATUS_T *status, int num)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count;

	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, &pdev->prty.ip, VIRTUAL_SWITCH_LISTEN_PORT);

	memset(sbuf, 0, sizeof(sbuf));
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_SET_STATUS);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);

	uint8_t vs_id;
	uint8_t vs_len = 2;
	uint16_t vs_val;
	int id;
	count = 4;
	for (id = 0; id < num; id++) {
		_dev_status_to_vs_status(status->id, status->val, &vs_id, &vs_val);

		SET_CMD_FIELD(sbuf, 4 + id * 4, uint8_t, vs_id);
		SET_CMD_FIELD(sbuf, 5 + id * 4, uint8_t, vs_len);
		SET_CMD_FIELD(sbuf, 6 + id * 4, uint16_t, vs_val);

		status++;
		count += 4;
	}

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), NULL, NULL, &tv);

	if (ret < count) {
		hsb_critical("set status: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	int len = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || len != ret) {
		hsb_critical("set status: get err pkt, cmd=%x, len=%d\n", cmd, len);
		close(sockfd);
		return -2;
	}

	if (!result) {
		hsb_debug("set status result=%d\n", result);
		close(sockfd);
		return -3;
	}

	hsb_debug("send set status[%d] to device done\n", *status);

	close(sockfd);

	return 0;
}

static int virtual_switch_get_status(struct _HSB_DEV_T *pdev, HSB_STATUS_T *status, int *num)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count = 8;

	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, &pdev->prty.ip, VIRTUAL_SWITCH_LISTEN_PORT);

	memset(sbuf, 0, sizeof(sbuf));
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_GET_STATUS);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);
	SET_CMD_FIELD(sbuf, 4, uint32_t, 0xffffffff);

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), NULL, NULL, &tv);

	if (ret < count) {
		hsb_critical("get status: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	int len = GET_CMD_FIELD(rbuf, 2, uint16_t);

	if (cmd != VS_CMD_GET_STATUS_RESP || ret != len) {
		hsb_critical("get status: get err pkt, cmd=%x, len=%d\n", cmd, len);
		close(sockfd);
		return -2;
	}

	int status_num = 0;
	uint16_t id, val;
	count = 4;

	while (count + 4 <= len) {
		id = GET_CMD_FIELD(rbuf, count, uint8_t);
		val = GET_CMD_FIELD(rbuf, count + 2, uint16_t);

		_vs_status_to_dev_status(id, val, &status->id, &status->val);

		status_num ++;
		status ++;
		count += 4;

		if (status_num >= *num)
			break;
	}

	*num = status_num;

	close(sockfd);
	hsb_debug("send get status to device done, status=%d\n", *status);

	return HSB_E_OK;
}

static int virtual_switch_set_action(struct _HSB_DEV_T *pdev, const HSB_ACTION_T *act)
{
	struct sockaddr_in servaddr;
	socklen_t slen = sizeof(servaddr);
	uint8_t sbuf[64], rbuf[64];
	int ret;
	size_t count;

	int sockfd = open_udp_clientfd();

	make_sockaddr(&servaddr, &pdev->prty.ip, VIRTUAL_SWITCH_LISTEN_PORT);

	memset(sbuf, 0, sizeof(sbuf));
	SET_CMD_FIELD(sbuf, 0, uint16_t, VS_CMD_DO_ACTION);
	SET_CMD_FIELD(sbuf, 2, uint16_t, count);
	SET_CMD_FIELD(sbuf, 4, uint8_t, act->id);
	SET_CMD_FIELD(sbuf, 5, uint8_t, 2);
	SET_CMD_FIELD(sbuf, 6, uint16_t, act->param);

	count = 8;

	sendto(sockfd, sbuf, count, 0, (struct sockaddr *)&servaddr, slen);

	struct timeval tv = { 3, 0 };
	ret = recvfrom_timeout(sockfd, rbuf, sizeof(rbuf), NULL, NULL, &tv);

	if (ret < count) {
		hsb_critical("set action: get err pkt, len=%d\n", ret);
		close(sockfd);
		return -1;
	}

	int cmd = GET_CMD_FIELD(rbuf, 0, uint16_t);
	int len = GET_CMD_FIELD(rbuf, 2, uint16_t);
	uint16_t result = GET_CMD_FIELD(rbuf, 4, uint16_t);

	if (cmd != VS_CMD_RESULT || len != ret) {
		hsb_critical("set action: get err pkt, cmd=%x, len=%d\n", cmd, len);
		close(sockfd);
		return -2;
	}

	if (!result) {
		hsb_debug("set action result=%d\n", result);
		close(sockfd);
		return -3;
	}

	hsb_debug("send set action (%d,%d) to device done\n", act->id, act->param);

	close(sockfd);

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

	hsb_debug("get info done\n");

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

static int _update_status(HSB_DEV_T *pdev, HSB_STATUS_T *status)
{
	dev_status_updated(pdev->id, status);
	
	return 0;
}

static void *_monitor_thread(void *arg)
{
	int fd = open_udp_listenfd(19002);
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

		if (len != ret)
			continue;

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
				HSB_STATUS_T status;
				uint8_t id = GET_CMD_FIELD(rbuf, 4, uint8_t);
				uint16_t val = GET_CMD_FIELD(rbuf, 6, uint16_t);
				
				_vs_status_to_dev_status(id, val, &status.id, &status.val);

				hsb_debug("status %d=%d\n", id, val);
				_update_status(pdev, &status);
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

