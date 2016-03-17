
#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>

#define CORE_UDP_LISTEN_PORT		(18000)
#define CORE_TCP_LISTEN_PORT		(18002)


int notify_dev_status_updated(uint32_t dev_id, uint32_t *status);
int notify_dev_added(uint32_t dev_id);
int notify_dev_deled(uint32_t dev_id);

int init_network_module(void);

#endif

