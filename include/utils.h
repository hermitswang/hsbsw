/*
 * Home Security Box 
 * 
 * Copyright (c) 2005 by Qubit-Star, Inc.All rights reserved. 
 * falls <bhuang@qubit-star.com>
 *
 * $Id: util.h 1178 2007-04-03 05:18:24Z kf701 $
 * 
 */
#ifndef _UTIL_H
#define _UTIL_H 

#include "hsb_param.h"
#include <glib.h>

int create_pid_file(char *pidfile) ;

int get_pid_from_file(char *pid_file);

size_t encrypt_b64(guchar* dest,guchar* src,size_t size);
size_t decode_b64( guchar *desc ,const guchar *str, int length) ;

ssize_t readline1(int fd, void *vptr, size_t maxlen) ;

gboolean check_cmd_prefix(const char *cmd , const char *prefix);

void hsb_syslog(const char *fmt , ... );

gint hsb_mkstemp (gchar *tmpl);

time_t get_uptime(void);

#endif
