
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "net_protocol.h"
#include "hsb_device.h"

typedef struct {
	uint32_t	devid;
	uint16_t	id;
	uint16_t	param;
} HSB_ACTION_T;

typedef struct {
	uint32_t	devid;
	uint8_t		id;
	uint8_t		param1;
	uint16_t	param2;
} HSB_EVT_T;

typedef struct {
	uint16_t	id;
	uint16_t	val;
} HSB_STATUS_T;

struct _HSB_DEV_T;
struct _HSB_DEV_DRV_T;

typedef struct {
	int (*probe)(void);
	int (*get_status)(struct _HSB_DEV_T *pdev, HSB_STATUS_T *status, int *num);
	int (*set_status)(struct _HSB_DEV_T *pdev, const HSB_STATUS_T *status, int num);
	int (*set_action)(struct _HSB_DEV_T *pdev, const HSB_ACTION_T *act);
} HSB_DEV_OP_T;

typedef struct _HSB_DEV_DRV_T {
	char			*name;
	uint32_t		drv_id;

	HSB_DEV_OP_T		*op;
} HSB_DEV_DRV_T;

typedef struct _HSB_TIMER_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint8_t		hour;
	uint8_t		min;
	uint8_t		sec;
	uint8_t		wday;
	uint16_t	act_id;
	uint16_t	act_param;
} HSB_TIMER_T;

typedef struct {
	bool		active;
	bool		expired;
} HSB_TIMER_STATUS_T;

typedef struct _HSB_DELAY_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint8_t		evt_id;
	uint8_t		evt_param1;
	uint16_t	evt_param2;
	uint16_t	act_id;
	uint16_t	act_param;
	uint32_t	delay_sec;
} HSB_DELAY_T;

typedef struct {
	bool		active;
	bool		started;
	time_t		start_tm;
} HSB_DELAY_STATUS_T;

typedef struct _HSB_LINKAGE_T {
	uint16_t	id;
	uint8_t		work_mode;
	uint8_t		flag;
	uint8_t		evt_id;
	uint8_t		evt_param1;
	uint16_t	evt_param2;
	uint32_t	act_devid;
	uint16_t	act_id;
	uint16_t	act_param;
} HSB_LINKAGE_T;

typedef struct {
	bool		active;
} HSB_LINKAGE_STATUS_T;

#define HSB_DEV_MAX_TIMER_NUM		(32)
#define HSB_DEV_MAX_DELAY_NUM		(8)
#define HSB_DEV_MAX_LINKAGE_NUM		(8)


typedef struct _HSB_DEV_T {
	uint32_t		id;

	uint32_t		status;

	HSB_DEV_CLASS_T		dev_class;
	uint32_t		interface;
	uint8_t			mac[6];

	HSB_WORK_MODE_T		work_mode;

	HSB_TIMER_T		timer[HSB_DEV_MAX_TIMER_NUM];
	HSB_TIMER_STATUS_T	timer_status[HSB_DEV_MAX_TIMER_NUM];

	HSB_DELAY_T		delay[HSB_DEV_MAX_DELAY_NUM];
	HSB_DELAY_STATUS_T	delay_status[HSB_DEV_MAX_DELAY_NUM];

	HSB_LINKAGE_T		link[HSB_DEV_MAX_LINKAGE_NUM];
	HSB_LINKAGE_STATUS_T	link_status[HSB_DEV_MAX_LINKAGE_NUM];

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
int get_dev_status(uint32_t dev_id, HSB_STATUS_T *status, int *num);
int set_dev_status(uint32_t dev_id, const HSB_STATUS_T *status, int num);
int probe_dev(uint32_t drv_id);


int get_dev_timer(uint32_t dev_id, uint16_t timer_id, HSB_TIMER_T *timer);
int set_dev_timer(uint32_t dev_id, const HSB_TIMER_T *timer);
int del_dev_timer(uint32_t dev_id, uint16_t timer_id);
int get_dev_delay(uint32_t dev_id, uint16_t delay_id, HSB_DELAY_T *delay);
int set_dev_delay(uint32_t dev_id, const HSB_DELAY_T *delay);
int del_dev_delay(uint32_t dev_id, uint16_t delay_id);
int get_dev_linkage(uint32_t dev_id, uint16_t link_id, HSB_LINKAGE_T *link);
int set_dev_linkage(uint32_t dev_id, const HSB_LINKAGE_T *link);
int del_dev_linkage(uint32_t dev_id, uint16_t link_id);

int set_dev_action(const HSB_ACTION_T *act);

HSB_DEV_T *create_dev(void);
HSB_DEV_T *find_dev_by_ip(struct in_addr *ip);
int destroy_dev(HSB_DEV_T *dev);
int register_dev(HSB_DEV_T *dev);
int remove_dev(HSB_DEV_T *dev);

int dev_status_updated(uint32_t devid, HSB_STATUS_T *status);
int dev_updated(uint32_t devid, HSB_DEV_UPDATED_TYPE_T type);
int dev_sensor_triggered(uint32_t devid, HSB_SENSOR_TYPE_T type);
int dev_sensor_recovered(uint32_t devid, HSB_SENSOR_TYPE_T type);
int dev_mode_changed(HSB_WORK_MODE_T mode);

int register_dev_drv(HSB_DEV_DRV_T *drv);

int init_virtual_switch_drv(void);

int check_timer_and_delay(void);

#endif

