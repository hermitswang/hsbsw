
#ifndef _NETWORK_UTILS_H_
#define _NETWORK_UTILS_H_

#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>

#define ETH_INTERFACE	"eth0"

#define GET_CMD_FIELD(_buf, _off, _type)  *(_type *)(_buf + _off)

#define SET_CMD_FIELD(_buf, _off, _type, _val)	do { \
	*(_type *)(_buf + _off) = (_val); \
} while (0)

int connect_nonb(const char *ip,int port,struct timeval *timeout);
int open_tcp_listenfd(unsigned short port);

int open_udp_listenfd(unsigned short port);
int open_udp_clientfd(void);
int set_broadcast(int sockfd, bool enable);
int get_broadcast_address(int sockfd, const char *eth_intf, struct in_addr *addr);

#endif
