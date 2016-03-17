

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
#include "driver_virtual_switch.h"

static uint32_t status = 0;
struct sockaddr_in mcastaddr;

static int deal_udp_pkt(int fd, uint8_t *buf, size_t count, struct sockaddr *cliaddr, socklen_t clilen)
{
	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(buf, 2, uint16_t);

	if (len != 16 || len != count) {
		printf("recv pkt size err, count=%d len=%d\n", count, len);
		return -1;
	}

	uint8_t rbuf[16];
	memset(rbuf, 0, sizeof(rbuf));


	switch (cmd) {
		case UDP_CMD_DEVICE_DISCOVER:
		{
			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_DEVICE_DISCOVER_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);

			printf("recv cmd: device discover\n");
			break;
		}
		case UDP_CMD_GET_STATUS:
		{
			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_GET_STATUS_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);
			SET_CMD_FIELD(rbuf, 4, uint32_t, status);

			printf("recv cmd: get status\n");
			break;
		}
		case UDP_CMD_SET_STATUS:
		{
			status = GET_CMD_FIELD(buf, 4, uint32_t);

			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_SET_STATUS_RESP);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 1);

			printf("recv cmd: set status[%d]\n", status);
			break;
		}
		case UDP_CMD_KEEP_ALIVE:
		{
			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_KEEP_ALIVE);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);

			break;
		}
		default:
		{
			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_RESULT);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);
			SET_CMD_FIELD(rbuf, 4, uint16_t, 0);

			printf("recv cmd: unknown %x\n, cmd");
			break;
		}
	}

	sendto(fd, rbuf, 16, 0, cliaddr, clilen);

	if (cmd != UDP_CMD_SET_STATUS)
		return 0;

	memset(rbuf, 0, sizeof(rbuf));
	SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_UPDATE_STATUS);
	SET_CMD_FIELD(rbuf, 2, uint16_t, 16);
	SET_CMD_FIELD(rbuf, 4, uint32_t, status);
	
	sendto(fd, rbuf, 16, 0, &mcastaddr, sizeof(mcastaddr));
	printf("send update status=%d\n", status);

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

			uint32_t val = 0;
			ret = sscanf(buf, "status=%d", &val);
			if (ret < 1 || status == val)
				continue;

			uint8_t rbuf[16];
			memset(rbuf, 0, sizeof(rbuf));

			status = val;

			SET_CMD_FIELD(rbuf, 0, uint16_t, UDP_CMD_UPDATE_STATUS);
			SET_CMD_FIELD(rbuf, 2, uint16_t, 16);
			SET_CMD_FIELD(rbuf, 4, uint32_t, status);

			printf("sendto %s\n", inet_ntoa(mcastaddr.sin_addr));
			sendto(sockfd, rbuf, 16, 0, (struct sockaddr *)&mcastaddr, mcastlen);
		}

	}

	return 0;
}


