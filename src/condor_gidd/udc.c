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

#include "err.h"
#include "udc.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int
udc_connect(char* path)
{
	int fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		err_sprintf("failed to create socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_un sun;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = PF_UNIX;
	size_t pathlen = strlen(path);
	memcpy(&sun.sun_path, path, pathlen);

	int ret = connect(fd,
	                  (struct sockaddr*)&sun,
	                  offsetof(struct sockaddr_un, sun_path) + pathlen);
	if (ret == -1) {
		err_sprintf("failed to connect socket to %s: %s",
		            path,
		            strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

int
udc_authenticate(int fd)
{
	struct msghdr msg;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	char nul = '\0';
	struct iovec iov;
	iov.iov_base = &nul;
	iov.iov_len = 1;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	char ctl[CMSG_SPACE(sizeof(struct ucred))];
	msg.msg_control = ctl;
	msg.msg_controllen = sizeof(ctl);
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDENTIALS;
	struct ucred* cred = (struct ucred*)CMSG_DATA(cmsg);
	cred->pid = getpid();
	cred->uid = geteuid();
	cred->gid = getegid();

	msg.msg_flags = 0;

	ssize_t bytes = sendmsg(fd, &msg, 0);
	if (bytes == -1) {
		err_sprintf("sendmsg failure: %s", strerror(errno));
		close(fd);
		return -1;
	}

	// TODO: checks on sendmsg return values

	return 0;
}
