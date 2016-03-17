/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: hsb_config.h 1120 2006-11-21 01:57:00Z wangxiaogang $
 * 
 */
#ifndef _HSB_CONFIG_H_
#define _HSB_CONFIG_H_

#define SYSLOG_DIR	"/var/log/"
#define PID_DIR	"/var/run/"
#define DAEMON_WORK_DIR "/tmp/hsb/"
#define EXECUTE_PATH  "/opt/bin/"

#define MAX_TCP_CLIENT_NUM	(5)

#define MAXLINE 1024UL
#define MAXPATH 256UL

typedef struct {
	char *pid_file ;
	char *unix_listen_path;	
	int unix_listen_fd ;
} hsb_daemon_config ;

static hsb_daemon_config hsb_core_daemon_config = {
	.pid_file = "hsb_core_daemon.pid" ,
	.unix_listen_path = "hsb_core_daemon.listen" ,	
	.unix_listen_fd = -1 ,
} ;

static hsb_daemon_config hsb_mm_daemon_config = {
	.pid_file = "hsb_mm_daemon.pid" ,
	.unix_listen_path = "hsb_mm_daemon.listen" ,	
	.unix_listen_fd = -1 ,
} ;

#endif

