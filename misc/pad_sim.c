

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "network_utils.h"
#include "net_protocol.h"

static int deal_tcp_pkt(int fd, void *buf, size_t count)
{
	int cnt;
	uint32_t dev_id;

	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(buf, 2, uint16_t);

	printf("get a cmd: %x\n", cmd);

	switch (cmd) {
		case HSB_CMD_GET_DEVS_RESP:
		{
			printf("get devices response:\n");
			for (cnt = 0; cnt < (len - 4) / 4; cnt++)
			{
				dev_id = GET_CMD_FIELD(buf, 4 + cnt*4, uint32_t);
				printf("dev: %d\n", dev_id);
			}
			break;
		}
		case HSB_CMD_GET_INFO_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint32_t drvid = GET_CMD_FIELD(buf, 8, uint32_t);
			uint16_t dev_class = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t interface = GET_CMD_FIELD(buf, 14, uint16_t);

			uint8_t mac[6];
			memcpy(mac, buf + 16, 6);

			printf("dev info: id=%d, drv=%d, class=%d, interface=%d, mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
				dev_id, drvid, dev_class, interface, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			break;
		}
		case HSB_CMD_GET_STATUS_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t id, val;

			for (cnt = 0; cnt < (len - 8) / 4; cnt++) {
				id = GET_CMD_FIELD(buf, 8 + cnt * 4, uint16_t);
				val = GET_CMD_FIELD(buf, 10 + cnt * 4, uint16_t);
				printf("get device %d status response: status %d=%d\n", dev_id, id, val);
			}

			break;
		}
		case HSB_CMD_GET_TIMER_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t timer_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint8_t hour = GET_CMD_FIELD(buf, 12, uint8_t);
			uint8_t min = GET_CMD_FIELD(buf, 13, uint8_t);
			uint8_t sec = GET_CMD_FIELD(buf, 14, uint8_t);
			uint8_t wday = GET_CMD_FIELD(buf, 15, uint8_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 16, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 18, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 20, uint32_t);

			printf("get dev %d timer info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%02x, hour=%d, min=%d, sec=%d, wday=%02x\n",
				timer_id, work_mode, flag, hour, min, sec, wday);
			printf("act: %d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);
			break;
		}
		case HSB_CMD_GET_DELAY_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t delay_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 20, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 22, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 24, uint32_t);
			uint32_t delay_sec = GET_CMD_FIELD(buf, 28, uint32_t);

			printf("get dev %d delay info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%d, delay=%d\n", delay_id, work_mode, flag, delay_sec);
			printf("evt id=%d, param=%d, param2=%d\n", evt_id, evt_param1, evt_param2);
			printf("act id=%d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);
			break;
		}
		case HSB_CMD_GET_LINKAGE_RESP:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);

			uint16_t link_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint8_t work_mode = GET_CMD_FIELD(buf, 10, uint8_t);
			uint8_t flag = GET_CMD_FIELD(buf, 11, uint8_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 12, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 14, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 16, uint32_t);
			uint32_t act_devid = GET_CMD_FIELD(buf, 20, uint32_t);
			uint16_t act_id = GET_CMD_FIELD(buf, 24, uint16_t);
			uint16_t act_param1 = GET_CMD_FIELD(buf, 26, uint16_t);
			uint32_t act_param2 = GET_CMD_FIELD(buf, 28, uint32_t);
	

			printf("get dev %d linkage info:\n", dev_id);
			printf("id=%d, work_mode=%d, flag=%d, link devid=%d\n", link_id, work_mode, flag, act_devid);
			printf("evt id=%d, param1=%d, param2=%d\n", evt_id, evt_param1, evt_param2);
			printf("act id=%d, param1=%d, param2=%x\n", act_id, act_param1, act_param2);

			break;
		}
		case HSB_CMD_EVENT:
		{
			dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
			uint16_t evt_id = GET_CMD_FIELD(buf, 8, uint16_t);
			uint16_t evt_param1 = GET_CMD_FIELD(buf, 10, uint16_t);
			uint32_t evt_param2 = GET_CMD_FIELD(buf, 12, uint32_t);

			printf("dev %d event: id=%d, param1=%d, param2=%x\n", dev_id, evt_id, evt_param1, evt_param2);
			break;
		}
		case HSB_CMD_RESULT:
		{
			uint16_t result = GET_CMD_FIELD(buf, 4, uint16_t);
			printf("result=%d\n", result);
			break;
		}
		default:
		{
			printf("unknown cmd: %x\n", cmd);
			break;
		}
	}

	return 0;
}

static int deal_input_cmd(int fd, void *buf, size_t count)
{
	uint8_t rbuf[16];
	int len;
	uint32_t dev_id;
	uint16_t status;
	uint16_t val;
	uint16_t drv_id;
	uint16_t timer_id;
	uint8_t evt_id;
	uint8_t evt_param1;
	uint16_t evt_param2;
	uint16_t act_id;
	uint16_t act_param;
	int val1, val2, val3;
	uint32_t val4;

	memset(rbuf, 0, sizeof(rbuf));

	if (!strncmp(buf, "get devices", 11)) {
		len = 4;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_DEVS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
	} else if (1 == sscanf(buf, "get info %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_INFO);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (1 == sscanf(buf, "get status %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
	} else if (3 == sscanf(buf, "set status %d %d %d", &val1, &val2, &val3)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
		SET_CMD_FIELD(rbuf, 10, uint16_t, val3);
	} else if (2 == sscanf(buf, "get timer %d %d", &val1, &val2)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_GET_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
	} else if (2 == sscanf(buf, "set timer %d %d", &val1, &val2)) {
		len = 24;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_SET_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
		SET_CMD_FIELD(rbuf, 10, uint8_t, 0xff);
		SET_CMD_FIELD(rbuf, 11, uint8_t, 0);
		SET_CMD_FIELD(rbuf, 12, uint8_t, 21); /* hour */
		SET_CMD_FIELD(rbuf, 13, uint8_t, 49); /* min */
		SET_CMD_FIELD(rbuf, 14, uint8_t, 0);  /* sec */
		SET_CMD_FIELD(rbuf, 15, uint8_t, 0x7F);
		SET_CMD_FIELD(rbuf, 16, uint16_t, 0); /* on_off */
		SET_CMD_FIELD(rbuf, 18, uint16_t, 1);
	} else if (2 == sscanf(buf, "del timer %d %d", &val1, &val2)) {
		len = 12;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DEL_TIMER);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2);
	} else if (1 == sscanf(buf, "probe %d", &val1)) {
		len = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_PROBE_DEV);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint16_t, val1);
	} else if (4 == sscanf(buf, "action %d %d %d %x", &val1, &val2, &val3, &val4)) {
		len = 16;
		SET_CMD_FIELD(rbuf, 0, uint16_t, HSB_CMD_DO_ACTION);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, val1);	/* devid */
		SET_CMD_FIELD(rbuf, 8, uint16_t, val2); /* action type */
		SET_CMD_FIELD(rbuf, 10, uint16_t, val3); /* action param */
		SET_CMD_FIELD(rbuf, 12, uint32_t, val4); /* action param2 */

		printf("action devid=%d\n", val1);
	} else {
		printf("invalid cmd\n");
		return -1;
	}

	write(fd, rbuf, len);

	return 0;
}

static int control_box(struct in_addr *addr)
{
	char ip[32];
	inet_ntop(AF_INET, addr, ip, 16);
	struct timeval tv = { 1, 0 };
	int sockfd = connect_nonb(ip, 18002, &tv);
	if (sockfd < 0) {
		printf("connect %s:18002 fail\n", ip);
		return -1;
	}

	/******************************************************/

	int ret, nread, inputfd = 0;
	fd_set readset;
	uint8_t buf[1024];
	while (1) {
		FD_ZERO(&readset);
		FD_SET(inputfd, &readset);
		FD_SET(sockfd, &readset);
		ret = select(sockfd+1, &readset, NULL, NULL, NULL);
		if (ret <= 0) {
			printf("connfd select error %m\n");
			continue;
		}

		if (FD_ISSET(sockfd, &readset)) {
			nread = read(sockfd, buf, sizeof(buf));

			if (nread <= 0) {
				printf("read err %d\n", nread);
				break;
			}

			deal_tcp_pkt(sockfd, buf, nread);
		}

		if (FD_ISSET(inputfd, &readset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			deal_input_cmd(sockfd, buf, nread);
		}
	}
	
	close(sockfd);

	return 0;
}

static int probe_box(struct in_addr *addr)
{
	int sockfd, bid;
	struct sockaddr_in servaddr, baddr;

	sockfd = open_udp_clientfd();

	if (get_broadcast_address(sockfd, "eth0", &servaddr.sin_addr) < 0)
	{
		close(sockfd);
		return -1;
	}
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(18000);

	set_broadcast(sockfd, true);

	int n, ret;
	uint8_t sendline[16], recvline[16];
	socklen_t blen = sizeof(baddr);
	socklen_t servlen = sizeof(servaddr);

	memset(sendline, 0, sizeof(sendline));

	int send_len = 8;
	SET_CMD_FIELD(sendline, 0, uint16_t, HSB_CMD_BOX_DISCOVER);
	SET_CMD_FIELD(sendline, 2, uint16_t, send_len);
	SET_CMD_FIELD(sendline, 4, uint8_t, 0);
	SET_CMD_FIELD(sendline, 5, uint8_t, 1);
	SET_CMD_FIELD(sendline, 6, uint16_t, 1);

	ret = sendto(sockfd, sendline, send_len, 0, (struct sockaddr *)&servaddr, servlen);

	struct timeval tv = { 2, 0 };
	n = recvfrom_timeout(sockfd, recvline, sizeof(recvline), (struct sockaddr *)&baddr, &blen, &tv);
	if (n <= 0) {
		close(sockfd);
		return -2;
	}

	printf("recv %d bytes\n", n);

	uint16_t cmd = GET_CMD_FIELD(recvline, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(recvline, 2, uint16_t);
	bid = GET_CMD_FIELD(recvline, 6, uint16_t);

	printf("cmd: %x, len: %d, bid: %d\n", cmd, len, bid);

	char addstr[32];
	inet_ntop(AF_INET, &baddr.sin_addr, addstr, 16);

	printf("box address: %s\n", addstr);

	close(sockfd);

	memcpy(addr, &baddr.sin_addr, sizeof(*addr));

	return 0;
}

int main(int argc, char *argv[])
{
	struct in_addr addr;

	while (1) {
		if (probe_box(&addr))
			continue;

		control_box(&addr);
	}
	
	return 0;
}


