

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
	uint16_t cmd = GET_CMD_FIELD(buf, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(buf, 2, uint16_t);

	switch (cmd) {
		case TCP_CMD_GET_DEVICES_RESP:
		{
			int cnt;
			uint32_t dev_id;
			printf("get devices response:\n");
			for (cnt = 0; cnt < (len - 4) / 4; cnt++)
			{
				dev_id = GET_CMD_FIELD(buf, 4 + cnt*4, uint32_t);
				printf("%d\n", dev_id);
			}
			break;
			case TCP_CMD_GET_DEVICE_STATUS_RESP:
			{
				uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
				uint32_t status = GET_CMD_FIELD(buf, 8, uint32_t);
				printf("get device status response: dev_id=%d, status=%d\n", dev_id, status);
				break;
			}
			case TCP_CMD_DEVICE_STATUS_UPDATED:
			{
				uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
				uint32_t status = GET_CMD_FIELD(buf, 8, uint32_t);
				printf("device status updated: dev_id=%d, status=%d\n", dev_id, status);
				break;
			}
			case TCP_CMD_DEVICE_ADDED:
			{
				uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
				printf("device added: dev_id=%d\n", dev_id);
				break;
			}
			case TCP_CMD_DEVICE_DELED:
			{
				uint32_t dev_id = GET_CMD_FIELD(buf, 4, uint32_t);
				printf("device deled: dev_id=%d\n", dev_id);
				break;
			}
			case TCP_CMD_RESULT:
			{
				uint16_t result = GET_CMD_FIELD(buf, 4, uint16_t);
				printf("result=%d\n", result);
				break;
			}
			default:
			{
				printf("unknown cmd: %x\n", cmd);
			}
		}
	}

	return 0;
}

static int deal_input_cmd(int fd, void *buf, size_t count)
{
	uint8_t rbuf[16];
	int len = sizeof(rbuf);
	uint32_t dev_id;
	uint32_t status;
	uint16_t drv_id;

	memset(rbuf, 0, sizeof(rbuf));

	if (!strncmp(buf, "get devices", 11)) {
		SET_CMD_FIELD(rbuf, 0, uint16_t, TCP_CMD_GET_DEVICES);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
	} else if (1 == sscanf(buf, "get status %d", &dev_id)) {
		SET_CMD_FIELD(rbuf, 0, uint16_t, TCP_CMD_GET_DEVICE_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, dev_id);
	} else if (2 == sscanf(buf, "set status %d %d", &dev_id, &status)) {
		SET_CMD_FIELD(rbuf, 0, uint16_t, TCP_CMD_SET_DEVICE_STATUS);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint32_t, dev_id);
		SET_CMD_FIELD(rbuf, 8, uint32_t, status);
	} else if (1 == sscanf(buf, "probe %d", &drv_id)) {
		SET_CMD_FIELD(rbuf, 0, uint16_t, TCP_CMD_PROBE_DEVICE);
		SET_CMD_FIELD(rbuf, 2, uint16_t, len);
		SET_CMD_FIELD(rbuf, 4, uint16_t, drv_id);
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

	int send_len = 16;
	SET_CMD_FIELD(sendline, 0, uint16_t, 0x8801);
	SET_CMD_FIELD(sendline, 2, uint16_t, send_len);
	SET_CMD_FIELD(sendline, 4, uint8_t, 0);
	SET_CMD_FIELD(sendline, 5, uint8_t, 1);
	SET_CMD_FIELD(sendline, 8, uint32_t, 1);

	ret = sendto(sockfd, sendline, send_len, 0, (struct sockaddr *)&servaddr, servlen);

	struct timeval tv = { 2, 0 };
	n = recvfrom_timeout(sockfd, recvline, 1024, (struct sockaddr *)&baddr, &blen, &tv);
	if (n <= 0) {
		close(sockfd);
		return -2;
	}

	printf("recv %d bytes\n", n);

	uint16_t cmd = GET_CMD_FIELD(recvline, 0, uint16_t);
	uint16_t len = GET_CMD_FIELD(recvline, 2, uint16_t);
	bid = GET_CMD_FIELD(recvline, 8, uint32_t);

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


