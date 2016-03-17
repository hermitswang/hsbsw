
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <netinet/in.h>
#include "net_protocol.h"

struct _HSB_DEV_T;
struct _HSB_DEV_DRV_T;

typedef struct {
	int (*probe)(void);
	int (*set_status)(struct _HSB_DEV_T *pdev, uint32_t *status);
	int (*get_status)(struct _HSB_DEV_T *pdev, uint32_t *status);
} HSB_DEV_OP_T;

typedef struct _HSB_DEV_DRV_T {
	char			*name;
	uint32_t		drv_id;

	HSB_DEV_CLASS_T		dev_class;
	uint32_T		interface;

	HSB_DEV_OP_T		*op;
} HSB_DEV_DRV_T;

typedef struct _HSB_DEV_T {
	uint32_t		id;

	uint32_t		status;

	uint8_t			mac[6];

	union {
		struct in_addr	ip;
	} prty;

	uint32_t		idle_time;

	HSB_DEV_DRV_T		*driver;

	void			*priv_data;
} HSB_DEV_T;

int init_dev_module(void);
int get_dev_id_list(uint32_t *dev_id, int *dev_num);
int get_dev_info(uint32_t dev_id, HSB_DEV_T *dev);
int get_dev_status(uint32_t dev_id, uint32_t *status);
int set_dev_status(uint32_t dev_id, uint32_t *status);
int probe_dev(uint32_t drv_id);

HSB_DEV_T *create_dev(void);
HSB_DEV_T *find_dev_by_ip(struct in_addr *ip);
int destroy_dev(HSB_DEV_T *dev);
int register_dev(HSB_DEV_T *dev);
int remove_dev(HSB_DEV_T *dev);
int update_dev_status(HSB_DEV_T *dev, uint32_t *status);

int register_dev_drv(HSB_DEV_DRV_T *drv);

int init_virtual_switch_drv(void);

#endif

