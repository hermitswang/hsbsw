
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include "debug.h"
#include "hsb_error.h"
#include "hsb_config.h"
#include "device.h"
#include "thread_utils.h"
#include "network.h"
#include "network_utils.h"
#include "net_protocol.h"

#define MAKE_CMD_HDR(_buf, _cmd, _len)	do { \
	SET_CMD_FIELD(_buf, 0, uint16_t, _cmd); \
	SET_CMD_FIELD(_buf, 2, uint16_t, _len); \
} while (0)

#define REPLY_OK	(1)
#define REPLY_FAIL	(0)

static int check_tcp_pkt_valid(uint8_t *buf, int len)
{
	if (!buf || len <= 0)
		return -1;

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t length = GET_CMD_FIELD(buf, 2, uint16_t);

	if (!HSB_CMD_VALID(command))
		return -2;

	if (len != length)
		return -3;

	return 0;
}

static int _reply_result(uint8_t *buf, int ok)
{
	int len = 8;

	MAKE_CMD_HDR(buf, HSB_CMD_RESULT, len);

	SET_CMD_FIELD(buf, 4, uint16_t, ok);

	return len;
}

static int _reply_dev_id_list(uint8_t *buf, uint32_t *dev_id, int dev_num)
{
	int len, id;

	len = dev_num * 4 + 4;
	MAKE_CMD_HDR(buf, HSB_CMD_GET_DEVS_RESP, len);

	for (id = 0; id < dev_num; id++) {
		SET_CMD_FIELD(buf, 4 + id * 4, uint32_t, dev_id[id]);
	}

	return len;
}

static int _reply_get_device_info(uint8_t *buf, HSB_DEV_T *dev)
{
	int len = 24;

	MAKE_CMD_HDR(buf, HSB_CMD_GET_INFO_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev->id); /* device id */
	SET_CMD_FIELD(buf, 8, uint32_t, dev->driver->drv_id); /* driver id */
	SET_CMD_FIELD(buf, 12, uint16_t, dev->dev_class); /* device class */
	SET_CMD_FIELD(buf, 14, uint16_t, dev->interface); /* device interface */
	memcpy(buf + 16, dev->mac, 6); /* mac address */

	return len;
}

static int _reply_get_device_status(uint8_t *buf, uint32_t dev_id, HSB_STATUS_T *status, int num)
{
	int len = 8 + num * 4;
	int id;

	MAKE_CMD_HDR(buf, HSB_CMD_GET_STATUS_RESP, len);
	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);

	for (id = 0; id < num; id++) {
		SET_CMD_FIELD(buf, 8 + id * 4, uint16_t, status[id].id);
		SET_CMD_FIELD(buf, 10 + id * 4, uint16_t, status[id].val);
	}

	return len;
}

static int  _reply_get_timer(uint8_t *buf, uint32_t dev_id, HSB_TIMER_T *tm)
{
	int len = 20;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_TIMER_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t, tm->id);
	SET_CMD_FIELD(buf, 10, uint8_t, tm->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t, tm->flag);
	SET_CMD_FIELD(buf, 12, uint8_t, tm->hour);
	SET_CMD_FIELD(buf, 13, uint8_t, tm->min);
	SET_CMD_FIELD(buf, 14, uint8_t, tm->sec);
	SET_CMD_FIELD(buf, 15, uint8_t, tm->wday);
	SET_CMD_FIELD(buf, 16, uint16_t, tm->act_id);
	SET_CMD_FIELD(buf, 18, uint16_t, tm->act_param);

	return len;
}

static int  _reply_get_delay(uint8_t *buf, uint32_t dev_id, HSB_DELAY_T *delay)
{
	int len = 24;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_DELAY_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t, delay->id);
	SET_CMD_FIELD(buf, 10, uint8_t, delay->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t, delay->flag);
	SET_CMD_FIELD(buf, 12, uint8_t, delay->evt_id);
	SET_CMD_FIELD(buf, 13, uint8_t, delay->evt_param1);
	SET_CMD_FIELD(buf, 14, uint16_t, delay->evt_param2);
	SET_CMD_FIELD(buf, 16, uint16_t, delay->act_id);
	SET_CMD_FIELD(buf, 18, uint16_t, delay->act_param);
	SET_CMD_FIELD(buf, 20, uint32_t, delay->delay_sec);

	return len;
}

static int  _reply_get_linkage(uint8_t *buf, uint32_t dev_id, HSB_LINKAGE_T *link)
{
	int len = 24;
	
	MAKE_CMD_HDR(buf, HSB_CMD_GET_LINKAGE_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint16_t,link->id);
	SET_CMD_FIELD(buf, 10, uint8_t,link->work_mode);
	SET_CMD_FIELD(buf, 11, uint8_t,link->flag);
	SET_CMD_FIELD(buf, 12, uint8_t, link->evt_id);
	SET_CMD_FIELD(buf, 13, uint8_t, link->evt_param1);
	SET_CMD_FIELD(buf, 14, uint16_t, link->evt_param2);
	SET_CMD_FIELD(buf, 16, uint32_t, link->act_devid);
	SET_CMD_FIELD(buf, 20, uint16_t, link->act_id);
	SET_CMD_FIELD(buf, 22, uint16_t, link->act_param);

	return len;
}

static int _get_dev_status(uint8_t *buf, int len, HSB_STATUS_T *status, int *num)
{
	int id, total;

	total = (len - 8) / 4;

	for (id = 0; id < total; id++) {
		status[id].id = GET_CMD_FIELD(buf, 8 + id * 4, uint16_t);
		status[id].val = GET_CMD_FIELD(buf, 10 + id * 4, uint16_t);
		hsb_debug("_get_dev_status: %d=%d\n", status[id].id, status[id].val);
	}

	*num = total;

	return 0;
}

static int _notify_dev_event(uint8_t *buf, HSB_EVT_T *evt)
{
	int len = 12;

	MAKE_CMD_HDR(buf, HSB_CMD_EVENT, len);

	SET_CMD_FIELD(buf, 4, uint32_t, evt->devid);
	SET_CMD_FIELD(buf, 8, uint8_t, evt->id);
	SET_CMD_FIELD(buf, 9, uint8_t, evt->param1);
	SET_CMD_FIELD(buf, 10, uint16_t, evt->param2);

	return len;
}

int deal_tcp_packet(int fd, uint8_t *buf, int len)
{
	int ret = 0;
	int rlen = 0;
	uint8_t reply_buf[1024];
	uint16_t cmd;

	if (check_tcp_pkt_valid(buf, len)) {
		hsb_debug("tcp pkt invalid\n");
		return -1;
	}

	cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	switch (cmd) {
		case HSB_CMD_GET_DEVS:
		{
			uint32_t dev_id[128];
			int dev_num;
			ret = get_dev_id_list(dev_id, &dev_num);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_dev_id_list(reply_buf, dev_id, dev_num);

			break;
		}
		case HSB_CMD_GET_INFO:
		{
			HSB_DEV_T dev;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			ret = get_dev_info(dev_id, &dev);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_get_device_info(reply_buf, &dev);

			break;
		}
		case HSB_CMD_GET_STATUS:
		{
			HSB_STATUS_T status[16];
			int status_num = 16;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			ret = get_dev_status(dev_id, status, &status_num);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_get_device_status(reply_buf, dev_id, status, status_num);

			break;
		}
		case HSB_CMD_SET_STATUS:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			HSB_STATUS_T status[16];
			int status_num;

			_get_dev_status(buf, len, status, &status_num);

			ret = set_dev_status(dev_id, status, status_num);

			rlen = _reply_result(reply_buf, ret);

			break;
		}
		case HSB_CMD_GET_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_TIMER_T tm = { 0 };

			ret = get_dev_timer(dev_id, timer_id, &tm);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_get_timer(reply_buf, dev_id, &tm);

			break;
		}
		case HSB_CMD_SET_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_TIMER_T tm = { 0 };

			tm.id = timer_id;
			tm.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			tm.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			tm.hour = GET_CMD_FIELD(buf, 12, uint8_t);
			tm.min = GET_CMD_FIELD(buf, 13, uint8_t);
			tm.sec = GET_CMD_FIELD(buf, 14, uint8_t);
			tm.wday = GET_CMD_FIELD(buf, 15, uint8_t);
			tm.act_id = GET_CMD_FIELD(buf, 16, uint16_t);
			tm.act_param = GET_CMD_FIELD(buf, 18, uint16_t);

			ret = set_dev_timer(dev_id, &tm);
			if (ret)
				hsb_debug("set_dev_timer ret=%d\n", ret);

			rlen = _reply_result(reply_buf, ret);
			break;
		}
		case HSB_CMD_DEL_TIMER:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_timer(dev_id, timer_id);
			rlen = _reply_result(reply_buf, ret);
			break;
		}
		case HSB_CMD_GET_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_DELAY_T delay = { 0 };

			ret = get_dev_delay(dev_id, delay_id, &delay);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_get_delay(reply_buf, dev_id, &delay);

			break;
		}
		case HSB_CMD_SET_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_DELAY_T delay = { 0 };

			delay.id = delay_id;
			delay.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			delay.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			delay.evt_id = GET_CMD_FIELD(buf, 12, uint8_t);
			delay.evt_param1 = GET_CMD_FIELD(buf, 13, uint8_t);
			delay.evt_param2 = GET_CMD_FIELD(buf, 14, uint16_t);
			delay.act_id = GET_CMD_FIELD(buf, 16, uint16_t);
			delay.act_param = GET_CMD_FIELD(buf, 18, uint16_t);
			delay.delay_sec = GET_CMD_FIELD(buf, 20, uint32_t);

			ret = set_dev_delay(dev_id, &delay);
			rlen = _reply_result(reply_buf, ret);

			break;
		}
		case HSB_CMD_DEL_DELAY:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_delay(dev_id, delay_id);
			rlen = _reply_result(reply_buf, ret);

			break;
		}
		case HSB_CMD_GET_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_LINKAGE_T link = { 0 };

			ret = get_dev_linkage(dev_id, link_id, &link);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, ret);
			else
				rlen = _reply_get_linkage(reply_buf, dev_id, &link);

			break;
		}
		case HSB_CMD_SET_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			HSB_LINKAGE_T link = { 0 };

			link.id = link_id;
			link.work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			link.flag = GET_CMD_FIELD(buf, 11, uint8_t);
			link.evt_id = GET_CMD_FIELD(buf, 12, uint8_t);
			link.evt_param1 = GET_CMD_FIELD(buf, 13, uint8_t);
			link.evt_param2 = GET_CMD_FIELD(buf, 14, uint16_t);
			link.act_devid = GET_CMD_FIELD(buf, 16, uint32_t);
			link.act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			link.act_param = GET_CMD_FIELD(buf, 22, uint16_t);

			ret = set_dev_linkage(dev_id, &link);
			rlen = _reply_result(reply_buf, ret);
			break;
		}
		case HSB_CMD_DEL_LINKAGE:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);

			ret = del_dev_linkage(dev_id, link_id);
			rlen = _reply_result(reply_buf, ret);
			break;
		}
		case HSB_CMD_DO_ACTION:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			HSB_ACTION_T act = { 0 };

			act.devid = dev_id;
			act.id = GET_CMD_FIELD(buf, 8, uint16_t);
			act.param = GET_CMD_FIELD(buf, 10, uint16_t);

			ret = set_dev_action(&act);
			rlen = _reply_result(reply_buf, ret);
			break;
		}
		case HSB_CMD_PROBE_DEV:
		{
			uint16_t type = GET_CMD_FIELD(buf, 4, uint16_t);
			hsb_debug("probe\n");
			ret = probe_dev(type);
			rlen = _reply_result(reply_buf, ret);

			break;
		}
		default:
			rlen = _reply_result(reply_buf, REPLY_FAIL);
			break;
	}

	if (rlen <= 0) {
		hsb_debug("rlen%d<0\n", rlen);
		return -2;
	}

	struct timeval tv = { 1, 0 };
	ret = write_timeout(fd, reply_buf, rlen, &tv);

	return 0;
}

#define UDP_CMD_VALID(x)	(x == HSB_CMD_BOX_DISCOVER || x == HSB_CMD_BOX_DISCOVER_RESP)

static int check_udp_pkt_valid(uint8_t *buf, int len)
{
	if (!buf || len <= 0)
		return -1;

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t length = GET_CMD_FIELD(buf, 2, uint16_t);

	if (!UDP_CMD_VALID(command))
		return -2;

	if (len != length)
		return -3;

	return 0;
}

static int _reply_box_discover(uint8_t *buf, uint16_t bid)
{
	int len = 8;

	MAKE_CMD_HDR(buf, HSB_CMD_BOX_DISCOVER_RESP, len);

	SET_CMD_FIELD(buf, 4, uint8_t, 0);	/* Minor Version */
	SET_CMD_FIELD(buf, 5, uint8_t, 1);	/* Major Version */

	SET_CMD_FIELD(buf, 6, uint16_t, bid);

	return len;
}

static int deal_udp_packet(int fd, uint8_t *buf, int len, struct sockaddr *cliaddr, socklen_t slen)
{
	int rlen = 0;
	uint8_t reply_buf[32];
	uint32_t bid = 1;

	if (check_udp_pkt_valid(buf, len)) {
		hsb_critical("get invalid udp pkt\n");
		return -1;
	}

	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	switch (cmd) {
		case HSB_CMD_BOX_DISCOVER:
		{
			uint16_t uid = GET_CMD_FIELD(buf, 6, uint16_t);
			rlen = _reply_box_discover(reply_buf, bid);
		}
		default:
			break;
	}

	if (rlen > 0) {
		sendto(fd, reply_buf, rlen, 0, (const struct sockaddr *)cliaddr, slen);
	}

	return 0;
}

static void *udp_listen_thread(void *arg)
{
	struct sockaddr_in cliaddr;
	int sockfd = open_udp_listenfd(CORE_UDP_LISTEN_PORT);
	if (sockfd < 0)
		return NULL;

	int nread;
	socklen_t len;
	uint8_t buf[1024];

	while (1) {
		len = sizeof(cliaddr);
		nread = recvfrom(sockfd,
				buf,
				sizeof(buf),
				0,
				(struct sockaddr *)&cliaddr,
				&len);

		deal_udp_packet(sockfd, buf, nread, (struct sockaddr *)&cliaddr, len);
	}

	return NULL;
}

/* tcp client pool */

typedef struct {
	int tcp_sockfd;
	int un_sockfd;
	char listen_path[64];
	GMutex mutex;
	GQueue queue;
	int using;
} tcp_client_context;

typedef struct {
	tcp_client_context	context[MAX_TCP_CLIENT_NUM];
	GMutex			mutex;

	GThreadPool		*pool;
} tcp_client_pool;

static tcp_client_pool	client_pool;

static void tcp_client_handler(gpointer data, gpointer user_data);

static int init_client_pool(void)
{
	int cnt;
	tcp_client_context *pctx = NULL;

	memset(&client_pool, 0, sizeof(client_pool));

	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];

		g_mutex_init(&pctx->mutex);
		g_queue_init(&pctx->queue);

		snprintf(pctx->listen_path,
			sizeof(pctx->listen_path),
			DAEMON_WORK_DIR"core_client_%d.listen",
			cnt);

		//pctx->un_sockfd = unix_socket_new_listen((const char *)pctx->listen_path);
	}

	g_mutex_init(&client_pool.mutex);
	
	GError *error = NULL;

	client_pool.pool = g_thread_pool_new(tcp_client_handler, NULL, 5, TRUE, &error);

	return 0;
}

static int release_client_pool(void)
{
	// TODO
	return 0;
}

static tcp_client_context *get_client_context(int sockfd)
{
	int cnt;
	tcp_client_context *pctx;

	g_mutex_lock(&client_pool.mutex);

	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];
		if (!pctx->using)
			break;
	}

	if (cnt == MAX_TCP_CLIENT_NUM) {
		g_mutex_unlock(&client_pool.mutex);
		return NULL;
	}

	pctx->using = 1;
	pctx->tcp_sockfd = sockfd;
	pctx->un_sockfd = unix_socket_new_listen((const char *)pctx->listen_path);

	g_mutex_unlock(&client_pool.mutex);

	return pctx;
}

static int put_client_context(tcp_client_context *pctx)
{
	
	g_mutex_lock(&client_pool.mutex);

	pctx->using = 0;

	// TODO: clean queue
	unix_socket_free(pctx->un_sockfd);

	g_mutex_unlock(&client_pool.mutex);

	return 0;
}

static void *tcp_listen_thread(void *arg)
{
	fd_set readset;
        int ret, sockfd = 0;
        int listenfd = open_tcp_listenfd(CORE_TCP_LISTEN_PORT);
        if (listenfd <= 0) {
                hsb_critical("open listen fd error\n");
                return NULL;
        }

        signal(SIGPIPE, SIG_IGN);
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);

	tcp_client_context *pctx = NULL;

	while (1) {
		bzero(&addr, addrlen);
		sockfd = accept(listenfd, &addr, &addrlen);
		if (sockfd < 0) {
			hsb_critical("accept error\n");
			close(listenfd);
			return NULL;
		}

		pctx = get_client_context(sockfd);
		if (!pctx) {
			close(sockfd);
			continue;
		}

		g_thread_pool_push(client_pool.pool, (gpointer)pctx, NULL);
	}

	hsb_critical("tcp listen thread closed\n");
	close(listenfd);
	return NULL;
}

static int _process_notify(int fd, tcp_client_context *pctx)
{
	HSB_EVT_T *evt;
	uint8_t buf[32];
	int ret, len;

	while (!g_queue_is_empty(&pctx->queue)) {
		evt = g_queue_peek_head(&pctx->queue);

		memset(buf, 0, sizeof(buf));

		len = _notify_dev_event(buf, evt);

		ret = write(fd, buf, len);
		if (ret <= 0)
			return ret;

		g_queue_pop_head(&pctx->queue);
		g_slice_free(HSB_EVT_T, evt);
	}

	return 0;
}

static void tcp_client_handler(gpointer data, gpointer user_data)
{
	tcp_client_context *pctx = (tcp_client_context *)data;
	fd_set readset;
	int ret, nread;
	int sockfd = pctx->tcp_sockfd;
	int unfd = pctx->un_sockfd;
	int maxfd = (sockfd > unfd) ? sockfd : unfd;
	uint8_t msg[1024];

        signal(SIGPIPE, SIG_IGN);

	while (1) {
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		FD_SET(unfd, &readset);

		ret = select(maxfd+1, &readset, NULL, NULL, NULL);

		if (ret <= 0) {
			hsb_critical("select error\n");
			break;
		}

		if (FD_ISSET(sockfd, &readset)) {
			nread = read(sockfd, msg, sizeof(msg));	
			if (nread <= 0) {
				break;
			}

			ret = deal_tcp_packet(sockfd, msg, nread);
			if (ret) {
				break;
			}
		}

		if (FD_ISSET(unfd, &readset)) {
			nread = recvfrom(unfd, msg, sizeof(msg), 0, NULL, NULL);
			hsb_debug("get notify\n");

			_process_notify(sockfd, pctx);
		}
	}

	close(sockfd);

	put_client_context(pctx);
}

#define NOTIFY_MESSAGE	"notify"

static void _notify(HSB_EVT_T *msg)
{
	int cnt, fd;
	tcp_client_context *pctx = NULL;

	for (cnt = 0; cnt < MAX_TCP_CLIENT_NUM; cnt++) {
		pctx = &client_pool.context[cnt];
		if (!pctx->using)
			continue;

		g_mutex_lock(&pctx->mutex);

		g_queue_push_tail(&pctx->queue, msg);

		g_mutex_unlock(&pctx->mutex);

		fd = unix_socket_new();
		unix_socket_send_to(fd, pctx->listen_path, NOTIFY_MESSAGE, strlen(NOTIFY_MESSAGE));
		unix_socket_free(fd);
	}
}

int notify_dev_event(HSB_EVT_T *evt)
{
	HSB_EVT_T *notify = g_slice_dup(HSB_EVT_T, evt);
	if (!notify) {
		hsb_critical("alloc notify fail\n");
		return -1;
	}

	_notify(notify);

	return 0;
}

int init_network_module(void)
{
	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)udp_listen_thread, NULL))
	{
		hsb_critical("create udp listen thread failed\n");
		return -1;
	}

	init_client_pool();

	if (pthread_create(&thread_id, NULL, (thread_entry_func)tcp_listen_thread, NULL))
	{
		hsb_critical("create tcp listen thread failed\n");
		return -2;
	}

	return 0;
}



