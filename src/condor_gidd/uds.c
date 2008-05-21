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
#include "uds.h"
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

// the FD of the listening socket, once initialized
//
static int s_lfd = -1;

int
uds_init(char* path)
{
	if (unlink(path) == -1) {
		if (errno != ENOENT) {
			err_sprintf("failed to unlink %s: %s",
			            path,
			            strerror(errno));
			return -1;
		}
	}	

	s_lfd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (s_lfd == -1) {
		err_sprintf("failed to create socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_un sun;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = PF_UNIX;
	size_t pathlen = strlen(path);
	memcpy(&sun.sun_path, path, pathlen);

	int ret = bind(s_lfd,
	               (struct sockaddr*)&sun,
	               offsetof(struct sockaddr_un, sun_path) + pathlen);
	if (ret == -1) {
		err_sprintf("failed to bind socket to %s: %s",
		            path,
		            strerror(errno));
		close(s_lfd);
		return -1;
	}

	if (listen(s_lfd, 5) == -1) {
		err_sprintf("listen failed on socket: %s", strerror(errno));
		close(s_lfd);
		return -1;
	}

	return 0;
}

int
uds_accept(void)
{
	int cfd = accept(s_lfd, NULL, NULL);
	if (cfd == -1) {
		err_sprintf("accept failed on socket: %s", strerror(errno));
		return -1;
	}
	return cfd;
}

int
uds_authenticate(int fd, uid_t* uid)
{
	int on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) == -1) {
		err_sprintf("setsockopt failed for SO_PASSCRED: %s",
		            strerror(errno));
		close(fd);
	}

	struct msghdr msg;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	struct iovec iov;
	char nul;
	iov.iov_base = 	&nul;
	iov.iov_len = sizeof(nul);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	char ctl[CMSG_SPACE(sizeof(struct ucred))];
	msg.msg_control = ctl;
	msg.msg_controllen = sizeof(ctl);

	msg.msg_flags = 0;

	ssize_t bytes = recvmsg(fd, &msg, 0);
	if (bytes == -1) {
		err_sprintf("recvmsg failure: %s", strerror(errno));
		close(fd);
		return -1;
	}

	if (bytes == 0) {
		err_sprintf("unexpected EOF from client");
		close(fd);
		return -1;
	}
	if (nul != '\0') {
		err_sprintf("protocol error from client: zero byte expected");
		close(fd);
		return -1;
	}
	if (msg.msg_controllen != sizeof(ctl)) {
		err_sprintf("protocol error from client: "
		                "%u of %u expected control bytes received",
		            (unsigned)msg.msg_controllen,
		            (unsigned)sizeof(ctl));
		close(fd);
		return -1;
	}
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	if ((cmsg->cmsg_level != SOL_SOCKET) ||
	    (cmsg->cmsg_type != SCM_CREDENTIALS))
	{
		err_sprintf("protocol error from client: "
		                "invalid control message");
		close(fd);
		return -1;
	}

	struct ucred* cred = (struct ucred*)CMSG_DATA(cmsg);
	*uid = cred->uid;

	return 0;
}
