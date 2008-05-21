/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "log.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static void log_msg(int prio, char* pre, char* fmt, va_list ap)
{
	char fbuf[128];
	size_t pre_len = 0;
	if (pre != NULL) {
		pre_len = strlen(pre);
		memcpy(fbuf, pre, pre_len);
	}
	strncpy(fbuf + pre_len, fmt, sizeof(fbuf) - pre_len);
	fbuf[sizeof(fbuf)] = '\0';
	char mbuf[256];
	vsnprintf(mbuf, sizeof(mbuf), fbuf, ap);
	syslog(prio, mbuf);
}


void
log_fatal(char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_msg(LOG_ERR, "fatal error: ", fmt, ap);
	va_end(ap);
	exit(1);
}

void
log_warning(char* fmt, ...)
{ 
	va_list ap;
	va_start(ap, fmt);
	log_msg(LOG_WARNING, "warning: ", fmt, ap);
	va_end(ap);
}

void
log_info(char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_msg(LOG_INFO, NULL, fmt, ap);
	va_end(ap);
}

