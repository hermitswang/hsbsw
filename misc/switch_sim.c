

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include "network_utils.h"
#include "../core_daemon/driver_virtual_switch.h"

static uint16_t status_on_off = 0;
struct sockaddr_in mcastaddr;

uint8_t mac[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

static int deal_udp_pkt(int fd, uint8_t *buf, size_t count, struct sockaddr *cliaddr, socklen_t clilen)
{
	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(buf, 2, uint16_t);

	if (len != count) {
		printf("recv pkt size err, count=%d len=%d\n", count, len);
		return -1;
	}

	uint8_t rbuf[64];
	int rlen = 0;
	bool status_updated = false;

	memset(rbuf, 0, sizeof(rbuf));

	switch (cmd) {
		case VS_CMD_DEVICE_DISCOVER:
		{
			rlen = 32;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_DEVICE_DISCOVER_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint8_t, 0);
			SET_CMD_FIELD(rbuf, 5, uint8_t, 1);
			SET_CMD_FIELD(rbuf, 8, uint16_t, 0);
			SET_CMD_FIELD(rbuf, 10, uint16_t, 0);

			memcpy(rbuf + 12, mac, 6);

			printf("recv cmd: device discover\n");
			break;
		}
		case VS_CMD_GET_INFO:
		{
			rlen = 28;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_GET_INFO_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 0);
			SET_CMD_FIELD(rbuf, 6, uint16_t, 0);

			memcpy(rbuf + 8, mac, 6);

			printf("recv cmd: get info\n");
			break;
		}
		case VS_CMD_GET_STATUS:
		{
			rlen = 8;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_GET_STATUS_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 0);
			SET_CMD_FIELD(rbuf, 6, uint16_t, status_on_off);

			printf("recv cmd: get status\n");
			break;
		}
		case VS_CMD_SET_STATUS:
		{
			uint16_t status_id = GET_CMD_FIELD(buf, 4, uint16_t);
			uint16_t status = GET_CMD_FIELD(buf, 6, uint16_t);
			if (0 != status_id) {
				printf("not supported status %d\n", status_id);
				break;
			}

			if (status != status_on_off) {
				status_updated = true;
				status_on_off = status;
			}
	
			rlen = 8;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_RESULT);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 1);

			printf("recv cmd: set status[%d]\n", status);
			break;
		}
		case VS_CMD_DO_ACTION:
		{
			uint16_t act_id = GET_CMD_FIELD(buf, 4, uint16_t);
			uint16_t act_val = GET_CMD_FIELD(buf, 6, uint16_t);

			if (act_id == 0) {
				printf("alarming...\n");
			} else if (act_id == 1) {
				printf("IR key: %d\n", act_val);
			}

			rlen = 8;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_RESULT);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 1);
			break;
		}
		case VS_CMD_KEEP_ALIVE:
		{
			rlen = 4;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_KEEP_ALIVE);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);

			break;
		}
		default:
		{
			rlen = 8;
			SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_RESULT);
			SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 0);

			printf("recv cmd: unknown %x\n, cmd");
			break;
		}
	}

	sendto(fd, rbuf, rlen, 0, cliaddr, clilen);

	if (!status_updated)
		return 0;

	memset(rbuf, 0, sizeof(rbuf));
	rlen = 8;
	SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_STATUS_CHANGED);
	SET_CMD_FIELD(rbuf, 2, uint16_t, 8);
	SET_CMD_FIELD(rbuf, 4, uint16_t, 0);
	SET_CMD_FIELD(rbuf, 6, uint16_t, status_on_off);

	
	sendto(fd, rbuf, rlen, 0, (struct sockaddr *)&mcastaddr, sizeof(mcastaddr));
	printf("send status changed=%d\n", status_on_off);

	return 0;
}

static int deal_input_cmd(int sockfd, uint8_t *buf, struct sockaddr_in *addr)
{
	uint32_t val = 0;
	uint8_t rbuf[16];
	int rlen = 0;
	memset(rbuf, 0, sizeof(rbuf));

	if (1 == sscanf(buf, "status=%d", &val)) {
		if (status_on_off == val)
			return 0;

		status_on_off = val;

		rlen = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_STATUS_CHANGED);
		SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
		SET_CMD_FIELD(rbuf, 4, uint16_t, 0);
		SET_CMD_FIELD(rbuf, 6, uint16_t, val);

	} else if (1 == sscanf(buf, "sensor=%d", &val)) {
		rlen = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_EVENT);
		SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
		if (val == 0)
			SET_CMD_FIELD(rbuf, 4, uint16_t, 1);
		else
			SET_CMD_FIELD(rbuf, 4, uint16_t, 0);
	
		SET_CMD_FIELD(rbuf, 6, uint16_t, 0);

	} else if (1 == sscanf(buf, "key=%d", &val)) {

		rlen = 8;
		SET_CMD_FIELD(rbuf, 0, uint16_t, VS_CMD_EVENT);
		SET_CMD_FIELD(rbuf, 2, uint16_t, rlen);
		SET_CMD_FIELD(rbuf, 4, uint16_t, 2);
		SET_CMD_FIELD(rbuf, 6, uint16_t, val);
	} else {
		return -1;
	}

	printf("sendto %s\n", inet_ntoa(addr->sin_addr));
	sendto(sockfd, rbuf, rlen, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));

	return 0;
}

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(struct sockaddr_in);
	socklen_t mcastlen = sizeof(struct sockaddr_in);

	sockfd = open_udp_listenfd(19001);
	if (sockfd < 0) {
		printf("open listenfd fail\n");
		return -1;
	}

	set_broadcast(sockfd, true);
	get_broadcast_address(sockfd, "eth0", &mcastaddr.sin_addr);
	mcastaddr.sin_family = AF_INET;
	mcastaddr.sin_port = htons(19002);

	int nread, inputfd = 0;
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
			clilen = sizeof(struct sockaddr_in);
			nread = recvfrom(sockfd,
					 buf,
					 sizeof(buf),
					 0,
					 (struct sockaddr *)&cliaddr,
					 &clilen);

			if (nread <= 0) {
				printf("nread=%d", nread);
				continue;
			}

			deal_udp_pkt(sockfd, buf, nread, (struct sockaddr *)&cliaddr, clilen);
		}

		if (FD_ISSET(inputfd, &readset)) {
			nread = read(inputfd, buf, sizeof(buf));
			buf[nread] = 0;

			deal_input_cmd(sockfd, buf, &mcastaddr);
		}
	}

	return 0;
}

