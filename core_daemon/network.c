
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

	if (!TCP_CMD_VALID(command))
		return -2;

	if (len != length)
		return -3;

	return 0;
}

static int _reply_result(uint8_t *buf, int ok)
{
	int len = 16;

	MAKE_CMD_HDR(buf, TCP_CMD_RESULT, len);

	SET_CMD_FIELD(buf, 4, uint16_t, ok);

	return len;
}

static int _reply_dev_id_list(uint8_t *buf, uint32_t *dev_id, int dev_num)
{
	int len, id;

	len = dev_num * 4 + 4;
	MAKE_CMD_HDR(buf, TCP_CMD_GET_DEVICES_RESP, len);

	for (id = 0; id < dev_num; id++) {
		SET_CMD_FIELD(buf, 4 + id * 4, uint32_t, dev_id[id]);
	}

	return len;
}

static int _reply_get_device_info(uint8_t *buf, HSB_DEVICE_T *dev)
{
	int len;

	// TODO
	return 0;
}

static int _reply_get_device_status(uint8_t *buf, uint32_t dev_id, uint32_t *status)
{
	int len = 16;

	MAKE_CMD_HDR(buf, TCP_CMD_GET_DEVICE_STATUS_RESP, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint32_t, *status);

	return len;
}

static int _notify_dev_status_updated(uint8_t *buf, uint32_t dev_id, uint32_t *status)
{
	int len = 16;

	MAKE_CMD_HDR(buf, TCP_CMD_DEVICE_STATUS_UPDATED, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);
	SET_CMD_FIELD(buf, 8, uint32_t, *status);

	return len;
}

static int _notify_dev_added(uint8_t *buf, uint32_t dev_id)
{
	int len = 16;

	MAKE_CMD_HDR(buf, TCP_CMD_DEVICE_ADDED, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);

	return len;
}

static int _notify_dev_deled(uint8_t *buf, uint32_t dev_id)
{
	int len = 16;

	MAKE_CMD_HDR(buf, TCP_CMD_DEVICE_DELED, len);

	SET_CMD_FIELD(buf, 4, uint32_t, dev_id);

	return len;
}


int deal_tcp_packet(int fd, uint8_t *buf, int len)
{
	int ret = 0;
	int rlen = 0;
	uint8_t reply_buf[1024];

	if (ret = check_tcp_pkt_valid(buf, len)) {
		hsb_debug("tcp pkt invalid\n");
		return -1;
	}

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	hsb_debug("get tcp cmd %x\n", command);

	switch (command) {
		case TCP_CMD_GET_DEVICES:
		{
			uint32_t dev_id[256];
			int dev_num;
			ret = get_dev_id_list(dev_id, &dev_num);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, REPLY_FAIL);
			else
				rlen = _reply_dev_id_list(reply_buf, dev_id, dev_num);

			break;
		}
		case TCP_CMD_GET_DEVICE_INFO:
		{
			HSB_DEVICE_T dev;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			ret = get_dev_info(dev_id, &dev);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, REPLY_FAIL);
			else
				rlen = _reply_get_device_info(reply_buf, &dev);

			break;
		}
		case TCP_CMD_GET_DEVICE_STATUS:
		{
			uint32_t status;
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			ret = get_dev_status(dev_id, &status);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, REPLY_FAIL);
			else
				rlen = _reply_get_device_status(reply_buf, dev_id, &status);

			break;
		}
		case TCP_CMD_SET_DEVICE_STATUS:
		{
			uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint32_t status = GET_CMD_FIELD(buf, 8, uint32_t);
			ret = set_dev_status(dev_id, &status);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, REPLY_FAIL);
			else
				rlen = _reply_result(reply_buf, REPLY_OK);

			break;
		}
		case TCP_CMD_PROBE_DEVICE:
		{
			uint16_t type = GET_CMD_FIELD(buf, 4, uint16_t);
			hsb_debug("probe\n");
			ret = probe_dev(type);
			if (HSB_E_OK != ret)
				rlen = _reply_result(reply_buf, REPLY_FAIL);
			else
				rlen = _reply_result(reply_buf, REPLY_OK);

			break;
		}
		default:
			rlen = _reply_result(reply_buf, REPLY_FAIL);
			break;
	}

	if (rlen < 0) {
		hsb_debug("rlen%d<0\n", rlen);
		return -2;
	}

	struct timeval tv = { 1, 0 };
	ret = write_timeout(fd, reply_buf, rlen, &tv);
	//hsb_debug("reply %d bytes\n", rlen);

	return 0;
}

typedef enum {
	UDP_CMD_BOX_DISCOVER = 0x8801,
	UDP_CMD_BOX_DISCOVER_RESP = 0x8802,
	UDP_CMD_LAST,
} UDP_CMD_T;

#define UDP_CMD_VALID(x)	(x >= UDP_CMD_BOX_DISCOVER && x < UDP_CMD_LAST)

int check_udp_pkt_valid(uint8_t *buf, int len)
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

static int _reply_box_discover(uint8_t *buf, uint32_t bid)
{
	int len = 16;

	MAKE_CMD_HDR(buf, UDP_CMD_BOX_DISCOVER_RESP, len);

	SET_CMD_FIELD(buf, 4, uint8_t, 0);
	SET_CMD_FIELD(buf, 5, uint8_t, 1);

	SET_CMD_FIELD(buf, 8, uint32_t, bid);

	return len;
}

static int deal_udp_packet(int fd, uint8_t *buf, int len, struct sockaddr *cliaddr, socklen_t slen)
{
	int ret = 0;
	int rlen = 0;
	uint8_t reply_buf[1024];
	uint32_t bid = 1;

	//hsb_debug("[udp]get %d bytes\n", len);

	if (ret = check_udp_pkt_valid(buf, len))
		return -1;

	uint16_t command = GET_CMD_FIELD(buf, 0, uint16_t);
	memset(reply_buf, 0, sizeof(reply_buf));

	switch (command) {
		case UDP_CMD_BOX_DISCOVER:
		{
			uint32_t uid = GET_CMD_FIELD(buf, 8, uint32_t);
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

typedef enum {
	NOTIFY_TYPE_DEVICE_STATUS_UPDATED = 0,
	NOTIFY_TYPE_DEVICE_ADDED,
	NOTIFY_TYPE_DEVICE_DELED,
	NOTIFY_TYPE_LAST,
} NOTIFY_TYPE_T;

typedef struct {
	NOTIFY_TYPE_T	type;
	uint32_t	dev_id;
	uint32_t	status;
} notify_msg;

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

	close(listenfd);
	return NULL;
}

static int _process_notify(int fd, tcp_client_context *pctx)
{
	notify_msg *notify;
	uint8_t buf[16];
	int ret, len = sizeof(buf);

	while (!g_queue_is_empty(&pctx->queue)) {
		notify = g_queue_peek_head(&pctx->queue);

		memset(buf, 0, sizeof(buf));
		switch (notify->type) {
			case NOTIFY_TYPE_DEVICE_STATUS_UPDATED:
			{
				_notify_dev_status_updated(buf, notify->dev_id, &notify->status);
				break;
			}
			case NOTIFY_TYPE_DEVICE_ADDED:
			{
				_notify_dev_added(buf, notify->dev_id);
				break;
			}
			case NOTIFY_TYPE_DEVICE_DELED:
			{
				_notify_dev_deled(buf, notify->dev_id);
				break;
			}
			default:
				break;
		}

		ret = write(fd, buf, len);
		if (ret <= 0)
			return ret;

		g_queue_pop_head(&pctx->queue);
		g_slice_free(notify_msg, notify);
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

static void _notify(notify_msg *msg)
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
		hsb_debug("notify to %s\n", pctx->listen_path);
		unix_socket_free(fd);
	}
}

int notify_dev_status_updated(uint32_t dev_id, uint32_t *status)
{
	notify_msg *notify = g_slice_new0(notify_msg);
	if (!notify) {
		hsb_critical("alloc notify fail\n");
		return -1;
	}

	notify->type = NOTIFY_TYPE_DEVICE_STATUS_UPDATED;
	notify->dev_id = dev_id;
	notify->status = *status;

	_notify(notify);

	return 0;
}

int notify_dev_added(uint32_t dev_id)
{
	notify_msg *notify = g_slice_new0(notify_msg);
	if (!notify) {
		hsb_critical("alloc notify fail\n");
		return -1;
	}

	notify->type = NOTIFY_TYPE_DEVICE_ADDED;
	notify->dev_id = dev_id;

	_notify(notify);

	return 0;
}

int notify_dev_deled(uint32_t dev_id)
{
	notify_msg *notify = g_slice_new0(notify_msg);
	if (!notify) {
		hsb_critical("alloc notify fail\n");
		return -1;
	}

	notify->type = NOTIFY_TYPE_DEVICE_DELED;
	notify->dev_id = dev_id;

	_notify(notify);

	return 0;
}

int init_network_module(void)
{
	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (thread_entry_func)udp_listen_thread, NULL))
		return -1;

	init_client_pool();

	if (pthread_create(&thread_id, NULL, (thread_entry_func)tcp_listen_thread, NULL))
		return -1;

	return 0;
}



