/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more information
 * contact: CONDOR Team, Attention: Professor Miron Livny,
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure
 * by the U.S. Government is subject to restrictions as set forth in
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison,
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_config.h"
#include "condor_constants.h"
#include "condor_debug.h"

/*
#include <arpa/inet.h>
#include <math.h>
*/
#include "reliUDP.h"

#define R_UDP_TAG 0x89ABCDEF
#define DATA_PACKET 'd'
#define ACK_PACKET 'a'
#ifndef OPEN_MAX
#define OPEN_MAX 512
#endif

int _rudp_initialized = 0;
uint32_t _rudp_myAddr = 0;
uint32_t _rudp_gwAddr = 0;
struct timeval _rudp_tinit = {0, 0};
uint32_t _rudp_seqno = 0;
struct _RUDP_FDinfo _rudp_fdInfo[OPEN_MAX];

static int
getGWaddr(uint32_t *gwAddr)
{
	char *gwStr;
	struct in_addr sAddr;

	if ((gwStr = getenv("R_UDP_GW_ADDR")) == NULL) {
		// I decided not to use param, at least at this moment, because user processes
		// must be able to link with this file, but can't call param
		//if ((gwStr = param("R_UDP_GW_ADDR")) == NULL) {
			dprintf(D_ALWAYS, "getGWaddr: Gateway address variable not defined\n");
			return -1;
		//}
	}

	if (!inet_aton(gwStr, &sAddr)) {
		dprintf(D_ALWAYS, "getGWaddr: invalid Gateway ip address specified\n");
		return -1;
	}

	*gwAddr = sAddr.s_addr;
	return 0;
}

static int
initialize()
{
	int ret;

	// get ip address of this machine
	if ((ret = _getIPaddr(&_rudp_myAddr)) < 0) {
		dprintf(D_ALWAYS, "initialize: could not find a valid ip address\n");
		return -1;
	}

	// if ip address returned is private one, we need to get the ip of the
	// gateway. We need to do this because we have to uniquely identify each
	// process network-wide to detect duplicated messages
	if (ret == 1) {
		if (getGWaddr(&_rudp_gwAddr) < 0) {
			dprintf(D_ALWAYS, "initialize: failed to get ip of gateway\n");
			return -1;
		}
	}

	// get the current time
	if (gettimeofday(&_rudp_tinit, NULL) < 0) {
		dprintf(D_ALWAYS, "initialize: gettimeofday - %s\n", strerror(errno));
		return -1;
	}

	_rudp_initialized = 1;

	return 0;
}

// @args:	buf-	the buffer where the striped msg will be stored
//			bufLen- value-result argument. The caller must set this value
//					with the length of 'buffer' allocated. And this function
//					will set this arg with the length of the striped msg
//			opcode- op code of the msg being striped
//			seqNo-	sequence number of the msg being striped
//			msg-	the buffer containing the msg being striped
//			msgLen-	the length of 'msg'
// @return:	0, if succeed
//			-1, if failed to stripe
static int
stripeMsg(
		char *buf,
		int *bufLen,
		char opcode,
		unsigned int seqNo,
		const char *msg,
		int msgLen)
{
	int i;
	unsigned long chksum = 0;
	unsigned short size;
	uint32_t net_uint, *ptr;
	uint16_t net_short;
	int myPID = getpid();

	ptr = (uint32_t *)buf;
	*ptr = R_UDP_TAG;
	switch(opcode) {
		case DATA_PACKET:
			if (*bufLen < 35 + msgLen) {
				dprintf(D_ALWAYS, "stripeMsg: insufficient buffer allocated\n");
				return -1;
			}
			memcpy(&buf[6], &opcode, 1);
			memcpy(&buf[7], &_rudp_myAddr, 4);
			memcpy(&buf[11], &_rudp_gwAddr, 4);
			net_uint = htonl(myPID);
			memcpy(&buf[15], &net_uint, 4);
			net_uint = htonl(_rudp_tinit.tv_sec);
			memcpy(&buf[19], &net_uint, 4);
			net_uint = htonl(_rudp_tinit.tv_usec);
			memcpy(&buf[23], &net_uint, 4);
			net_uint = htonl(seqNo);
			memcpy(&buf[27], &net_uint, 4);
			memcpy(&buf[31], msg, msgLen);
			size = msgLen + 31;
			break;
		case ACK_PACKET:
			if (*bufLen < 15) {
				dprintf(D_ALWAYS, "stripeMsg: insufficient buffer allocated\n");
				return -1;
			}
			memcpy(&buf[6], &opcode, 1);
			net_uint = htonl(seqNo);
			memcpy(&buf[7], &net_uint, 4);
			size = 11;
			break;
		default:
			dprintf(D_ALWAYS, "stripeMsg: invalid opcode\n");
			return -1;
	}
	net_short = htons(size+4);
	memcpy(&buf[4], &net_short, 2);

	// Calculate checksum
	for(i=0; i<size; i++) {
		chksum += (unsigned char)buf[i];
	}
	net_uint = htonl(chksum);
	memcpy(&buf[size], &net_uint, 4);

	*bufLen = size + 4;

	return 0;
}

// @args:	buf-	buffer containing a reliable udp message
//			bufLen- the length of buf. This should match to the 'size' field of
//					the msg in buf
//			opcode,ip, gip, pid, tv_sec, tv_usec, seqNo- obvious
//			msg, msgLen- msg that application actually sent and its size
// @return:	1, if rudp message is unstriped successfully
//			0, if non rudp message is received
//			-1, if failed
static int
unstripeMsg(char *buf, int bufLen,
		char *opcode, uint32_t *ip, uint32_t *gip, pid_t *pid,
		unsigned long *tv_sec, unsigned long *tv_usec, unsigned int *seqNo,
		char **msg, int *msgLen)
{
	int i;
	unsigned long chksum;
	unsigned short size;
	uint32_t net_uint, *ptr;
	uint16_t net_short;

	// check the magic number
	ptr = (uint32_t *)buf;
	if (*ptr != R_UDP_TAG) {
		return 0;
	}

	// do checksum
	memcpy(&net_uint, &buf[bufLen-4], 4);
	chksum = ntohl(net_uint);
	for(i = 0; i < bufLen - 4; i++) {
		chksum -= (unsigned char)buf[i];
	}
	if (chksum != 0) {
		dprintf(D_ALWAYS, "unstripeMsg: checksum failure\n");
		return -1;
	}

	// get size field and compare it against bufLen
	memcpy(&net_short, &buf[4], 2);
	size = ntohs(net_short);
	if (size != bufLen) {
		dprintf(D_ALWAYS, "unstripeMsg: size field does not match\n");
		return -1;
	}

	// get opcode
	*opcode = buf[6];

	switch(*opcode) {
		case DATA_PACKET:
			memcpy(ip, &buf[7], 4);
			memcpy(gip, &buf[11], 4);
			memcpy(&net_uint, &buf[15], 4);
			*pid = ntohl(net_uint);
			memcpy(&net_uint, &buf[19], 4);
			*tv_sec = ntohl(net_uint);
			memcpy(&net_uint, &buf[23], 4);
			*tv_usec = ntohl(net_uint);
			memcpy(&net_uint, &buf[27], 4);
			*seqNo = ntohl(net_uint);
			*msg = &buf[31];
			*msgLen = size - 35;
			break;
		case ACK_PACKET:
			memcpy(&net_uint, &buf[7], 4);
			*seqNo = ntohl(net_uint);
			break;
		default:
			dprintf(D_ALWAYS, "unstripeMsg: invalid opcode\n");
			return -1;
	}

	return 1;
}


static struct _RUDP_senderInfo *
getSender(
		struct _RUDP_senders *senders,
		uint32_t ip,
		uint32_t gip,
		pid_t pid,
		unsigned long sec,
		unsigned long usec)
{
	int index;
	struct _RUDP_senderInfo *sptr, *ptr;

	// Hash into bucket
	index = (ip + gip + pid) % _RUDP_BUCKET_SIZE;
	sptr = ptr = senders->senderArr[index];

	// Follow the chain of the bucket
	while(ptr) {
		if (ptr->ip == ip && ptr->gip == gip && ptr->pid == pid) {
			if (ptr->sec != sec || ptr->usec != usec) {
				// Reuse the data structure
				ptr->sec = sec;
				ptr->usec = usec;
				ptr->seq = 0;
				// We do not delete received data, if any
			}
			return ptr;
		}
		ptr = ptr->next;
	}

	// Not found, we need to allocate a data structure for this sender
	ptr = (struct _RUDP_senderInfo *)malloc(sizeof(struct _RUDP_senderInfo));
	ptr->ip = ip;
	ptr->gip = gip;
	ptr->pid = pid;
	ptr->sec = sec;
	ptr->usec = usec;
	ptr->seq = 0;
	ptr->next = sptr;
	senders->senderArr[index] = ptr;

	return ptr;
}

static int
queueDataPkt(int fd, char *pkt, int pktlen, int seqNo, struct sockaddr_in *from)
{
	struct _RUDP_rmsg *msg;

	msg = (struct _RUDP_rmsg *)malloc(sizeof(struct _RUDP_rmsg));
	if (!msg) {
		dprintf(D_ALWAYS, "queueDataPkt: malloc - %s\n", strerror(errno));
		return -1;
	}
	msg->msg = (char *)malloc(pktlen);
	if (!(msg->msg)) {
		dprintf(D_ALWAYS, "queueDataPkt: malloc - %s\n", strerror(errno));
		return -1;
	}
	memcpy(msg->msg, pkt, pktlen);
	msg->seq = seqNo;
	msg->bytes = pktlen;
	if (_rudp_fdInfo[fd].conn == 0) {
		memcpy(&(msg->from), from, sizeof(*from));
	}
	msg->next = NULL;
	if (_rudp_fdInfo[fd].qtail) {
		_rudp_fdInfo[fd].qtail->next = msg;
	} else {
		_rudp_fdInfo[fd].qhead = msg;
	}
	_rudp_fdInfo[fd].qtail = msg;

	return 0;
}

// @ret: ACK_PACKET if the expected ACK packet received
//       DATA_PACKET if a valid data packet is received and queued
//       0, if any of the following is true:
//       	- nothing to read there
//       	- delayed DATA packet arrived
//       	- delayed ACK packet arrived
//       -1, if error occurred
static char
procUDPmsg(int fd)
{
	char *dptr; 
	char rcvBuf[1040], opcode;
	int ackLen, result, addrLen, rst, retval;
	unsigned int seqNo, dLen;
	struct sockaddr_in from;
	uint32_t ip, gip;
	unsigned long sec, usec;
	pid_t pid;
	struct _RUDP_senderInfo *sender;
	struct _RUDP_rmsg *msg;

	if (_rudp_fdInfo[fd].conn == 0) {
		addrLen = sizeof(from);
		result = recvfrom(fd, rcvBuf, 1040, 0, (struct sockaddr *)&from, &addrLen);
	} else {
		result = recv(fd, rcvBuf, 1040, 0);
	}
	if (result < 0) {
		dprintf(D_ALWAYS, "procUDPmsg: recv(from) - %s\n", strerror(errno));
		return -1;
	}
	
	// Unstripe the received msg
	rst = unstripeMsg(rcvBuf, result, &opcode, &ip, &gip,
						&pid, &sec, &usec, &seqNo, &dptr, &dLen);
	if (rst <= 0) {
		if (rst < 0) {
			dprintf(D_ALWAYS, "procUDPmsg: unstripeMsg failed\n");
			return -1;
		}
		if (_rudp_fdInfo[fd].conn) {
			dprintf(D_FULLDEBUG, "\t\t\tnon-rudp DATA through fd = %d\n", fd);
		} else {
			dprintf(D_FULLDEBUG, "\t\t\tnon-rudp DATA from %s\n", sin_to_string(&from));
		}
		if (queueDataPkt(fd, rcvBuf, result, -1, &from) < 0) {
			return -1;
		}
		dprintf(D_FULLDEBUG, "\t\t\t\tqueued to rbuf\n");
		return DATA_PACKET;
	}
	
	switch(opcode) {
		case DATA_PACKET:
			if (_rudp_fdInfo[fd].conn) {
				dprintf(D_FULLDEBUG, "\t\t\tDATA(%d) through fd = %d\n", seqNo, fd);
			} else {
				dprintf(D_FULLDEBUG, "\t\t\tDATA(%d) from %s\n", seqNo, sin_to_string(&from));
			}
			// Get the sender info
			sender = getSender(_rudp_fdInfo[fd].senders, ip, gip, pid, sec, usec);
			dprintf(D_FULLDEBUG, "\t\t\tsender->seq = %d\n", sender->seq);

			// if the msg is not a delayed msg, queue the msg
			retval = 0;
			if ((sender->seq <= 2.147483e+09 &&
						sender->seq < seqNo &&
						seqNo < sender->seq + 2.147483e+09) ||
				(sender->seq > 2.147483e+09 &&
						(sender->seq < seqNo ||
						seqNo < sender->seq - 2.147483e+09))) {

				// queue the data packet
				dprintf(D_FULLDEBUG, "\t\t\t\tValid data packet - queue the msg to rbuf\n");
				if (queueDataPkt(fd, dptr, dLen, seqNo, &from) < 0) {
					return -1;
				}
				sender->seq = seqNo;
				retval = DATA_PACKET;
			}

			// reply ACK
			ackLen = sizeof(rcvBuf);
			if (stripeMsg(rcvBuf, &ackLen, ACK_PACKET, seqNo, NULL, 0) < 0) {
				dprintf(D_ALWAYS, "procUDPmsg: stripeMsg failed\n");
				return -1;
			}
			if (_rudp_fdInfo[fd].conn) {
				rst = send(fd, rcvBuf, ackLen, 0);
			} else {
				rst = sendto(fd, rcvBuf, ackLen, 0, (struct sockaddr *)&from, sizeof(from));
			}
			if (rst != ackLen) {
				dprintf(D_ALWAYS, "procUDPmsg: send(to) - %s\n", strerror(errno));
				return -1;
			}
			if (_rudp_fdInfo[fd].conn) {
				dprintf(D_FULLDEBUG, "\t\t\tACK(%d) fd = %d\n", seqNo, fd);
			} else {
				dprintf(D_FULLDEBUG, "\t\t\tACK(%d) to %s\n", seqNo,
						sin_to_string((struct sockaddr_in *)&from));
			}
			return retval;
		case ACK_PACKET:
			if (_rudp_fdInfo[fd].conn) {
				dprintf(D_FULLDEBUG, "\t\t\tACK(%d) fd = %d\n", seqNo, fd); 
			} else {
				dprintf(D_FULLDEBUG, "\t\t\tACK(%d) from %s\n", seqNo, sin_to_string(&from));
			}
			if (seqNo != _rudp_seqno) {
				dprintf(D_FULLDEBUG, "\t\t\tdelayed ACK\n"); 
				return 0;
			}
			dprintf(D_FULLDEBUG, "\t\t\texpected ACK\n"); 
			return ACK_PACKET;
		default:
			dprintf(D_ALWAYS, "procUDPmsg: invalid opcode\n");
			return -1;
	}
}

static void
cleanSenders(struct _RUDP_senders **senders)
{
	int i;
	struct _RUDP_senderInfo *ptr, *pptr;

	if (--((*senders)->ref) > 0) {
		*senders = (struct _RUDP_senders *)calloc(sizeof(struct _RUDP_senders), 1);
		if (*senders == NULL) {
			dprintf(D_ALWAYS, "cleanSenders: calloc - %s\n", strerror(errno));
			return;
		}
		(*senders)->ref = 1;
		return;
	}

	for (i=0; i < _RUDP_BUCKET_SIZE; i++) {
		ptr = ((*senders)->senderArr)[i];
		while (ptr) {
			pptr = ptr;
			ptr = ptr->next;
			free(pptr);
		}
		senders[i] = NULL;
	}

	return;
}

static void
delSenders(struct _RUDP_senders *senders)
{
	int i;
	struct _RUDP_senderInfo *ptr, *pptr;

	if (--(senders->ref) <= 0) {
		for (i=0; i < _RUDP_BUCKET_SIZE; i++) {
			ptr = (senders->senderArr)[i];
			while (ptr) {
				pptr = ptr;
				ptr = ptr->next;
				free(pptr);
			}
		}
		free(senders);
	}
	return;
}

static void
delQueue(struct _RUDP_rmsg *qhead)
{
	struct _RUDP_rmsg *ptr, *pptr;

	ptr = qhead;
	while (ptr) {
		pptr = ptr;
		ptr = ptr->next;
		free(pptr->msg);
		free(pptr);
	}

	return;
}

int
rudp_socket(void)
{
	int fd;

	if (!_rudp_initialized) {
		if (initialize() < 0) {
			dprintf(D_ALWAYS, "rudp_socket: failed to initialize data structure\n");
			return -1;
		}
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		dprintf(D_ALWAYS, "rudp_socket: socket - %s\n", strerror(errno));
		return fd;
	}

	if (rudp_register(fd) < 0) {
		dprintf(D_ALWAYS, "rudp_socket: register failed\n");
		return -1;
	}

	return fd;
}

int
rudp_register(int fd)
{

	if (_rudp_fdInfo[fd].senders) {
		dprintf(D_ALWAYS, "rudp_socket: the slot[%d] is taken by another socket\n", fd);
		return -1;
	}

	_rudp_fdInfo[fd].senders = (struct _RUDP_senders *)calloc(sizeof(struct _RUDP_senders), 1);
	if (!_rudp_fdInfo[fd].senders) {
		dprintf(D_ALWAYS, "rudp_socket: calloc - %s\n", strerror(errno));
		return -1;
	}
	_rudp_fdInfo[fd].qhead = _rudp_fdInfo[fd].qtail = NULL;
	dprintf(D_FULLDEBUG, "rudp[%d] created\n", fd);

	return 0;
}

int
rudp_close(int fd)
{
	int ret = 0;

	if (_rudp_fdInfo[fd].senders == NULL) {
		return close(fd);
	}
	dprintf(D_FULLDEBUG, "rudp[%d] deleted\n", fd);
	delSenders(_rudp_fdInfo[fd].senders);
	_rudp_fdInfo[fd].senders = NULL;
	if (_rudp_fdInfo[fd].qhead) {
		delQueue(_rudp_fdInfo[fd].qhead);
		_rudp_fdInfo[fd].qhead = _rudp_fdInfo[fd].qtail = NULL;
		ret = -1;
	}
	_rudp_fdInfo[fd].conn = 0;

	return ret;
}

int
rudp_dup2(int oldfd, int newfd)
{
	int ret;

	if (_rudp_fdInfo[oldfd].senders == NULL) {
		return dup2(oldfd, newfd);
	}

	dprintf(D_FULLDEBUG, "rudp[%d] is duped to rudp[%d]\n", oldfd, newfd);
	ret = dup2(oldfd, newfd);
	if (ret < 0) {
		dprintf(D_ALWAYS, "rudp_dup2: dup2 - %s\n", strerror(errno));
		return -1;
	}

	if (_rudp_fdInfo[ret].senders) {
		dprintf(D_ALWAYS, "rudp_dup2: newfd = %d is taken by another socket\n", newfd);
		return -1;
	}

	_rudp_fdInfo[ret].conn = _rudp_fdInfo[oldfd].conn;
	_rudp_fdInfo[ret].qhead = _rudp_fdInfo[ret].qtail = NULL;
	_rudp_fdInfo[ret].senders = _rudp_fdInfo[oldfd].senders;
	_rudp_fdInfo[ret].senders->ref++;

	return ret;
}

int
rudp_dup(int oldfd)
{
	int newfd;

	if (_rudp_fdInfo[oldfd].senders == NULL) {
		return dup(oldfd);
	}

	newfd = dup(oldfd);
	if (newfd < 0) {
		dprintf(D_ALWAYS, "rudp_dup: dup - %s\n", strerror(errno));
		return -1;
	}
	dprintf(D_FULLDEBUG, "rudp[%d] is duped to rudp[%d]\n", oldfd, newfd);

	if (_rudp_fdInfo[newfd].senders) {
		dprintf(D_ALWAYS, "rudp_dup2: newfd = %d is taken by another socket\n", newfd);
		return -1;
	}

	_rudp_fdInfo[newfd].conn = _rudp_fdInfo[oldfd].conn;
	_rudp_fdInfo[newfd].qhead = _rudp_fdInfo[newfd].qtail = NULL;
	_rudp_fdInfo[newfd].senders = _rudp_fdInfo[oldfd].senders;
	_rudp_fdInfo[newfd].senders->ref++;

	return newfd;
}

static int
sendData(
		int fd,
		const void *msg,
		size_t msgLen,
		const struct sockaddr *to,
		socklen_t tolen)
{
	char buf[1040];
	int bufLen, ready, maxfd, addrLen, tries, rst, i;
	fd_set rdfds, expfds;
	struct timeval tv;
	time_t current;
	short elapsed;

	_rudp_seqno++;
	addrLen = sizeof(struct sockaddr_in);

	// Stripe the message
	bufLen = sizeof(buf);
	if (stripeMsg(buf, &bufLen, DATA_PACKET, _rudp_seqno, msg, msgLen) < 0) {
		dprintf(D_ALWAYS, "sendData: failed to stripe message\n");
		return -1;
	}

	tries = 0;
	while (tries < 6) {
		if (to) {
			rst = sendto(fd, buf, bufLen, 0, to, tolen);
		} else {
			rst = send(fd, buf, bufLen, 0);
		}
		if (rst < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else {
				dprintf(D_ALWAYS, "sendData: send(to) - %s\n", strerror(errno));
				return -1;
			}
		}
		if (to) {
			dprintf(D_FULLDEBUG, "\t\t\tDATA(%d) sent to %s\n",
				_rudp_seqno, sin_to_string((struct sockaddr_in *)to));
		} else {
			dprintf(D_FULLDEBUG, "\t\t\tDATA(%d) sent fd = %d\n",
				_rudp_seqno, fd);
		}

		elapsed = 0;
		while(1) {
			current = time(NULL);
			// wait 2**tries sec before resend the packet
			tv.tv_sec = pow(2.0, tries) - elapsed;
			if (tv.tv_sec <= 0) { // timeout
				break;
			}
			tv.tv_usec = 0;
			FD_ZERO(&rdfds);
			FD_ZERO(&expfds);
			FD_SET(fd, &expfds);
			maxfd = fd + 1;
			for (i = 0; i < OPEN_MAX; i++) {
				if (_rudp_fdInfo[i].senders) {
					FD_SET(i, &rdfds);
					dprintf(D_FULLDEBUG, "\t\t\tfd = %d added for readiness to read\n", i);
					if (maxfd <= i) {
						maxfd = i + 1;
					}
				}
			}
			ready = select(maxfd, &rdfds, NULL, &expfds, &tv);
			if (ready < 0) {
				if (errno == EINTR) {
					elapsed = time(NULL) - current;
					continue;
				} else {
					dprintf(D_ALWAYS, "sendData: select - %s\n", strerror(errno));
					return -1;
				}
			}
			if (ready == 0) {
				break;
			}

			if (FD_ISSET(fd, &expfds)) {
				dprintf(D_ALWAYS, "sendData: select returned with expfds set\n");
				return -1;
			}

			for (i = 0; i < OPEN_MAX; i++) {
				if (FD_ISSET(i, &rdfds)) {
					dprintf(D_FULLDEBUG, "\t\t\tfd = %d is ready to read\n", i);
					rst = procUDPmsg(i);
					if (rst == ACK_PACKET) {
						return msgLen;
					} else if (rst < 0) {
						if (i == fd) return -1;
					}
					elapsed = time(NULL) - current;
				}
			}
		} // while (1)
		tries++;
		dprintf(D_FULLDEBUG, "\n\t\t\ttries = %d\n\n", tries);
	} // while(tries < 7)

	dprintf(D_ALWAYS, "sendData: couldn't get ACK from receiver\n");
	return -1;
}


int
rudp_sendto(
		int fd,
		const void *msg,
		size_t msgLen,
		const struct sockaddr *to,
		socklen_t tolen)
{

	if (_rudp_fdInfo[fd].senders == NULL) {
		return sendto(fd, msg, msgLen, 0, to, tolen); 
	}

	dprintf(D_FULLDEBUG, "\t\tSENDTO\n");
	_rudp_fdInfo[fd].conn = 0;

	if (msgLen > R_UDP_MAX_LEN) {
		dprintf(D_ALWAYS, "rudp_sendto: message is too big\n");
		return -1;
	}

	return sendData(fd, msg, msgLen, to, tolen);
}


int
rudp_send(
		int fd,
		const void *msg,
		size_t msgLen)
{
	if (_rudp_fdInfo[fd].senders == NULL) {
		return send(fd, msg, msgLen, 0); 
	}

	dprintf(D_FULLDEBUG, "\t\tSEND\n");
	_rudp_fdInfo[fd].conn = 1;

	if (msgLen > R_UDP_MAX_LEN) {
		dprintf(D_ALWAYS, "rudp_send: message is too big\n");
		return -1;
	}

	return sendData(fd, msg, msgLen, NULL, 0);
}


static int
recvData(int fd, void *msg, size_t msgLen, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	struct timeval tv;
	struct _RUDP_rmsg *ptr;
	fd_set rdfds, expfds;
	int val, len, ready, i, maxfd = 0;

	// Figure out the receiving mode
	if (val = fcntl(fd, F_GETFL, 0) < 0) {
		dprintf(D_ALWAYS, "recvData: fcntl - %s\n", strerror(errno));
		return -1;
	}
	while(1) {
		// If a msg has been queued for this socket, dequeue a msg return it
		if (_rudp_fdInfo[fd].qhead) {
			ptr = _rudp_fdInfo[fd].qhead;
			len = (msgLen > ptr->bytes) ? ptr->bytes : msgLen;
			memcpy(msg, ptr->msg, len);
			_rudp_fdInfo[fd].qhead = ptr->next;
			if (_rudp_fdInfo[fd].qhead == NULL) {
				_rudp_fdInfo[fd].qtail = NULL;
				dprintf(D_FULLDEBUG, "\t\t\tZero msg remained at the queue[fd = %d]\n", fd);
			}
			if (from) {
				memcpy(from, &(ptr->from), sizeof(ptr->from));
				*fromlen = sizeof(ptr->from);
			}
			free(ptr->msg);
			free(ptr);
			return len;
		}

		// Set fd_set for read and error
		FD_ZERO(&rdfds);
		FD_ZERO(&expfds);
		FD_SET(fd, &expfds);
		for (i = 0; i < OPEN_MAX; i++) {
			if (_rudp_fdInfo[i].senders) {
				FD_SET(i, &rdfds);
				dprintf(D_FULLDEBUG, "\t\t\tfd = %d added for readiness to read\n", i);
				maxfd = i + 1;
			}
		}

		// Do select
		if (val & O_NONBLOCK || flags & O_NONBLOCK) {
			tv.tv_sec = tv.tv_usec = 0;
			ready = select(maxfd, &rdfds, NULL, &expfds, &tv);
		} else {
			ready = select(maxfd, &rdfds, NULL, &expfds, NULL);
		}
		if (ready < 0) {
			if (errno == EINTR) {
				continue;
			}
			dprintf(D_ALWAYS, "recvData: select - %s\n", strerror(errno));
			return -1;
		}
		if (ready == 0) {
			return 0;
		}

		if (FD_ISSET(fd, &expfds)) {
			dprintf(D_ALWAYS, "recvData: select returned with expfds set\n");
			return -1;
		}

		for (i = 0; i < OPEN_MAX; i++) {
			if (FD_ISSET(i, &rdfds)) {
				dprintf(D_FULLDEBUG, "\t\t\tfd = %d is ready to read\n", i);
				(void) procUDPmsg(i);
			}
		}
	} // end of while(1)
}


int
rudp_recvfrom(int fd, void *msg, size_t msgLen, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
	if (_rudp_fdInfo[fd].senders == NULL) {
		return recvfrom(fd, msg, msgLen, flags, from, fromlen);
	}

	dprintf(D_FULLDEBUG, "\t\tRECVFROM:\n");
	_rudp_fdInfo[fd].conn = 0;
	return recvData(fd, msg, msgLen, flags, from, fromlen);
}


int
rudp_recv(int fd, void *msg, size_t msgLen, int flags)
{
	if (_rudp_fdInfo[fd].senders == NULL) {
		return recv(fd, msg, msgLen, flags);
	}

	dprintf(D_FULLDEBUG, "\t\tRECV:\n");
	_rudp_fdInfo[fd].conn = 1;
	return recvData(fd, msg, msgLen, flags, NULL, NULL);
}

int
rudp_select(
		int maxfd,
		fd_set *readfds,
		fd_set *writefds,
		fd_set *exceptfds,
		struct timeval *timeout)
{
	fd_set rudp_rdfds, rdfds, wrfds, expfds;
	int i, rudp_ready = 0;
	int ready;
	struct timeval tv;

	dprintf(D_FULLDEBUG, "\t\trudp_select:\n");

	// set rudp_rdfds and rudp_ready
	bzero(&rudp_rdfds, sizeof(fd_set));
	for (i = 0; i < maxfd; i++) {
		if (FD_ISSET(i, readfds) && _rudp_fdInfo[i].senders && _rudp_fdInfo[i].qhead) {
			FD_SET(i, &rudp_rdfds);
			rudp_ready++;
			dprintf(D_FULLDEBUG, "\t\t\tqueue for fd = %d are not empty\n", i);
		}
	}

	// do nonblocking select with given fd_sets
	tv.tv_sec = tv.tv_usec = 0;
	if (readfds) {
		memcpy(&rdfds, readfds, sizeof(rdfds));
	} else {
		bzero(&rdfds, sizeof(rdfds));
	}
	if (writefds) {
		memcpy(&wrfds, writefds, sizeof(wrfds));
	} else {
		bzero(&wrfds, sizeof(wrfds));
	}
	if (exceptfds) {
		memcpy(&expfds, exceptfds, sizeof(expfds));
	} else {
		bzero(&expfds, sizeof(expfds));
	}
	ready = select(maxfd, &rdfds, &wrfds, &expfds, &tv);
	dprintf(D_FULLDEBUG, "\t\t\tnonblocking select: %d fds are ready\n", ready);

	// if any fd is either ready or rudp rcv queue is not empty, return result
	if (ready > 0 || rudp_ready > 0) {
		if (ready < 0) { ready = 0; }
		if (rudp_ready > 0) {
			for (i = 0; i < maxfd; i++) {
				if (FD_ISSET(i, &rudp_rdfds) && !FD_ISSET(i, &rdfds)) {
					FD_SET(i, &rdfds);
					ready++;
				}
			}
		}
		if (readfds) {
			memcpy(readfds, &rdfds, sizeof(rdfds));
		}
		if (writefds) {
			memcpy(writefds, &wrfds, sizeof(wrfds));
		}
		if (exceptfds) {
			memcpy(exceptfds, &expfds, sizeof(expfds));
		}
		return ready;
	}

	// if non blocking select, return 0
	if (timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0) {
		if (readfds) {
			bzero(readfds, sizeof(rdfds));
		}
		if (writefds) {
			bzero(writefds, sizeof(wrfds));
		}
		if (exceptfds) {
			bzero(exceptfds, sizeof(expfds));
		}
		return 0;
	}

	// now, do real select
	return select(maxfd, readfds, writefds, exceptfds, timeout);
}
