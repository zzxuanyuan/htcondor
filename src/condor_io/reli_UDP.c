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
#define BUCKET_SIZE 73

int _rudp_initialized = 0;
uint32_t _rudp_myAddr = 0;
uint32_t _rudp_gwAddr = 0;
struct timeval _rudp_tinit = {0, 0};
uint32_t _rudp_seqno = 0;
struct senderInfo *_rudp_senderBucket[BUCKET_SIZE];
unsigned int _rudp_no_bytes = 0;
char _rudp_rcv_buf[65536];
struct sockaddr_in _rudp_last_sender;

static int
getGWaddr(uint32_t *gwAddr)
{
	char *gwStr;
	struct in_addr sAddr;

	if ((gwStr = param("R_UDP_GW_ADDR")) == NULL) {
		dprintf(D_ALWAYS, "getGWaddr: Gateway address variable not defined\n");
		return -1;
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
	if ((ret = _ssc_net_getIPaddr(&_rudp_myAddr)) < 0) {
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


static struct senderInfo *
getSender(uint32_t ip, uint32_t gip, pid_t pid, unsigned long sec, unsigned long usec)
{
	int index;
	struct senderInfo *sptr, *ptr;

	// Hash into bucket
	index = (ip + gip) % BUCKET_SIZE;
	sptr = ptr = _rudp_senderBucket[index];

	// Follow the chain of the bucket
	while(ptr) {
		if (ptr->ip == ip && ptr->gip == gip && ptr->pid == pid) {
			if (ptr->sec != sec || ptr->usec != usec) {
				// Reuse the data structure
				ptr->sec = sec;
				ptr->usec = usec;
				ptr->nSeq = 0;
			}
			return ptr;
		}
		ptr = ptr->next;
	}

	// Not found, we need to allocate a data structure for this sender
	ptr = (struct senderInfo *)malloc(sizeof(struct senderInfo));
	ptr->ip = ip;
	ptr->gip = gip;
	ptr->pid = pid;
	ptr->sec = sec;
	ptr->usec = usec;
	ptr->nSeq = 0;
	ptr->next = sptr;
	_rudp_senderBucket[index] = ptr;

	return ptr;
}


int
rudp_sendto(
		int fd,
		const void *msg,
		size_t msgLen,
		const struct sockaddr *to,
		socklen_t tolen)
{
	char *dptr; 
	char buf[65520], rcvBuf[65520], opcode;
	int bufLen, ackLen, result, ready, maxfd, addrLen, tries, rst;
	unsigned int seqNo, dLen;
	fd_set rdfds;
	struct timeval tv;
	struct sockaddr_in from;
	uint32_t ip, gip;
	unsigned long sec, usec;
	pid_t pid;
	struct senderInfo *sender;

	if (!_rudp_initialized) {
		if (initialize() < 0) {
			dprintf(D_ALWAYS, "rudp_sendto: failed to initialize data structure\n");
			return -1;
		}
	}
	dprintf(D_FULLDEBUG, "\tSENDTO\n");

	_rudp_seqno++;
	maxfd = fd + 1;
	addrLen = sizeof(struct sockaddr_in);

	// Stripe the message
	bufLen = sizeof(buf);
	if (stripeMsg(buf, &bufLen, DATA_PACKET, _rudp_seqno, msg, msgLen) < 0) {
		dprintf(D_ALWAYS, "rudp_sendto: failed to stripe message\n");
		return -1;
	}

	tries = 0;
	while (tries < 7) {
		if (sendto(fd, buf, bufLen, 0, to, tolen) < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else {
				dprintf(D_ALWAYS, "rudp_sendto: sendto - %s\n", strerror(errno));
				return -1;
			}
		}
		dprintf(D_FULLDEBUG, "\t\tDATA(%d) to %s\n", _rudp_seqno, sin_to_string((struct sockaddr_in *)to));

		while(1) {
			// wait 2**tries sec before resend the packet
			tv.tv_sec = pow(2.0, tries);
			tv.tv_usec = 0;
			FD_ZERO(&rdfds);
			FD_SET(fd, &rdfds);
			ready = select(maxfd, &rdfds, NULL, NULL, &tv);
			if (ready < 0) {
				if (errno == EINTR) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_sendto: select - %s\n", strerror(errno));
					return -1;
				}
			}
			if (ready == 0) {
				break;
			}

			// Here, something must be available to read at fd
			result = recvfrom(fd, rcvBuf, 65520, 0, (struct sockaddr *)&from, &addrLen);
			if (result < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_sendto: recvfrom - %s\n", strerror(errno));
					return -1;
				}
			}
	
			// Unstripe the received msg
			if (unstripeMsg(rcvBuf, result, &opcode, &ip, &gip, &pid, &sec, &usec,
						&seqNo, &dptr, &dLen) != 1) {
				dprintf(D_ALWAYS, "rudp_sendto: unstripeMsg failed\n");
				continue;
			}
	
			switch(opcode) {
				case DATA_PACKET:
					dprintf(D_FULLDEBUG, "\t\tDATA(%d) from %s\n", seqNo, sin_to_string(&from));
					// Get the sender info
					sender = getSender(ip, gip, pid, sec, usec);
					// if the msg is not a delayed msg, ignore the data packet
					if ((sender->nSeq <= 2.147483e+09 &&
								sender->nSeq <= seqNo &&
								seqNo < sender->nSeq + 2.147483e+09) ||
						(sender->nSeq > 2.147483e+09 &&
								(sender->nSeq <= seqNo ||
								seqNo < sender->nSeq - 2.147483e+09))) {
						memcpy(_rudp_rcv_buf, rcvBuf, result);
						memcpy(&_rudp_last_sender, &from, sizeof(from));
						_rudp_no_bytes = result;
						dprintf(D_FULLDEBUG, "\t\t\tValid data packet - queued to rcv_buf\n");
						continue;
					}
					ackLen = sizeof(rcvBuf);
					if (stripeMsg(rcvBuf, &ackLen, ACK_PACKET, seqNo, NULL, 0) < 0) {
						dprintf(D_ALWAYS, "rudp_sendto: stripeMsg failed\n");
						return -1;
					}
					while(1) {
						rst = sendto(fd, rcvBuf, ackLen, 0, (struct sockaddr *)&from, sizeof(from));
						if (rst != ackLen) {
							if (errno == EAGAIN || errno == EINTR) {
								continue;
							} else {   
								dprintf(D_ALWAYS, "rudp_sendto: sendto - %s\n", strerror(errno));
								return -1;
							}
						}
						dprintf(D_FULLDEBUG, "\t\tACK(%d) for delayed msg to %s\n", seqNo,
								sin_to_string((struct sockaddr_in *)&from));
						break;
					}
					continue;
				case ACK_PACKET:
					dprintf(D_FULLDEBUG, "\t\tACK(%d) from %s\n", seqNo, sin_to_string(&from));
					if (seqNo != _rudp_seqno) {
						continue;
					}
					return msgLen;
			}
		} // while (1)
		tries++;
		dprintf(D_FULLDEBUG, "\n\t\ttries = %d\n\n", tries);
	} // while(tries < 7)

	dprintf(D_ALWAYS, "rudp_sendto: couldn't get ACK from receiver\n");
	return -1;
}


int
rudp_recvfrom(int fd, void *msg, size_t msgLen, int flags,
		struct sockaddr *from, socklen_t *fromlen, struct rudpaddr *raddr)
{
	uint32_t ip, gip;
	pid_t pid;
	int bufLen, mLen, rst;
	unsigned int seqNo;
	char opcode, rcvBuf[65536], *buf, *msgPtr;
	struct senderInfo *sender;

	dprintf(D_FULLDEBUG, "\tRECVFROM:\n");

	while(1) {
		if (_rudp_no_bytes != 0) {
			buf = _rudp_rcv_buf;
			bufLen = _rudp_no_bytes;
			memcpy(from, &_rudp_last_sender, sizeof(_rudp_last_sender));
			*fromlen = sizeof(_rudp_last_sender);
			_rudp_no_bytes = 0;
		} else {
			bufLen = recvfrom(fd, rcvBuf, sizeof(rcvBuf), flags, from, fromlen);
			if (bufLen < 0) {
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_recvfrom: recvfrom - %s\n", strerror(errno));
					return -1;
				}
			}
			buf = rcvBuf;
		}
		rst = unstripeMsg(buf, bufLen, &opcode, &ip, &gip, &pid, &(raddr->sec),
							&(raddr->usec), &seqNo, &msgPtr, &mLen);

		if ( rst != 1) {
			dprintf(D_ALWAYS, "rudp_recvfrom: unstripeMsg failed\n");
			continue;
		}

		if (opcode == ACK_PACKET) {
			dprintf(D_FULLDEBUG, "\t\tACK(%d) from %s\n", seqNo, sin_to_string((struct sockaddr_in *)from));
			continue;
		}
		dprintf(D_FULLDEBUG, "\t\tDATA(%d) from %s\n", seqNo, sin_to_string((struct sockaddr_in *)from));

		// send ACK
		bufLen = sizeof(rcvBuf);
		if (stripeMsg(buf, &bufLen, ACK_PACKET, seqNo, NULL, 0) < 0) {
			dprintf(D_ALWAYS, "rudp_recvfrom: stripeMsg failed\n");
			return -1;
		}
		while(1) {
			rst = sendto(fd, buf, bufLen, 0, from, sizeof(*from));
			if (rst != bufLen) {
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				} else {   
					dprintf(D_ALWAYS, "rudp_recvfrom: sendto - %s\n", strerror(errno));
					return -1;
				}
			}
			dprintf(D_FULLDEBUG, "\t\tACK(%d) to %s\n", seqNo, sin_to_string((struct sockaddr_in *)from));
			break;
		}

		// get the pointer to sender info
		sender = getSender(ip, gip, pid, raddr->sec, raddr->usec);

		// Ignore delayed msg. I added the 2nd condition for the case of
		// wrapping around of seqNo. 2.147483e+09 = 2 to the 31 = 1/2 of the
		// biggest 4 bytes integer
		if ((sender->nSeq <= 2.147483e+09 &&
					(seqNo < sender->nSeq ||
					 sender->nSeq + 2.147483e+09 < seqNo)) ||
			(sender->nSeq > 2.147483e+09 &&
					sender->nSeq - 2.147483e+09 < seqNo &&
					seqNo < sender->nSeq)) {
			dprintf(D_FULLDEBUG, "\t\tDelayed data(%d) ignored: data(%d) expected\n", seqNo, sender->nSeq);
			continue;
		}
		sender->nSeq = seqNo + 1;

		// pass the received data to application
		if (mLen > msgLen) {
			mLen = msgLen;
		}
		memcpy(msg, msgPtr, mLen);
		return mLen;
	}
}


int
rudp_send(
		int fd,
		const void *msg,
		size_t msgLen)
{
	char *dptr; 
	char buf[65520], rcvBuf[65520], opcode;
	int bufLen, ackLen, result, ready, maxfd, addrLen, tries, rst;
	unsigned int seqNo, dLen;
	fd_set rdfds;
	struct timeval tv;
	struct sockaddr_in from;
	uint32_t ip, gip;
	unsigned long sec, usec;
	pid_t pid;
	struct senderInfo *sender;

	if (!_rudp_initialized) {
		if (initialize() < 0) {
			dprintf(D_ALWAYS, "rudp_send: failed to initialize data structure\n");
			return -1;
		}
	}
	dprintf(D_FULLDEBUG, "\tSEND\n");

	_rudp_seqno++;
	maxfd = fd + 1;
	addrLen = sizeof(struct sockaddr_in);

	// Stripe the message
	bufLen = sizeof(buf);
	if (stripeMsg(buf, &bufLen, DATA_PACKET, _rudp_seqno, msg, msgLen) < 0) {
		dprintf(D_ALWAYS, "rudp_send: failed to stripe message\n");
		return -1;
	}

	tries = 0;
	while (tries < 7) {
		if (send(fd, buf, bufLen, 0) < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else {
				dprintf(D_ALWAYS, "rudp_send: send - %s\n", strerror(errno));
				return -1;
			}
		}
		dprintf(D_FULLDEBUG, "\t\tDATA(%d) sent\n", _rudp_seqno);

		while(1) {
			// wait 2**tries sec before resend the packet
			tv.tv_sec = pow(2.0, tries);
			tv.tv_usec = 0;
			FD_ZERO(&rdfds);
			FD_SET(fd, &rdfds);
			ready = select(maxfd, &rdfds, NULL, NULL, &tv);
			if (ready < 0) {
				if (errno == EINTR) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_send: select - %s\n", strerror(errno));
					return -1;
				}
			}
			if (ready == 0) {
				break;
			}

			// Here, something must be available to read at fd
			result = recv(fd, rcvBuf, 65520, 0);
			if (result < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_send: recv - %s\n", strerror(errno));
					return -1;
				}
			}
	
			// Unstripe the received msg
			if (unstripeMsg(rcvBuf, result, &opcode, &ip, &gip, &pid, &sec, &usec,
						&seqNo, &dptr, &dLen) != 1) {
				dprintf(D_ALWAYS, "rudp_send: unstripeMsg failed\n");
				continue;
			}
	
			switch(opcode) {
				case DATA_PACKET:
					dprintf(D_FULLDEBUG, "\t\tDATA(%d) received\n", seqNo);
					// Get the sender info
					sender = getSender(ip, gip, pid, sec, usec);
					// if the msg is not a delayed msg, ignore the data packet
					if ((sender->nSeq <= 2.147483e+09 &&
								sender->nSeq <= seqNo &&
								seqNo < sender->nSeq + 2.147483e+09) ||
						(sender->nSeq > 2.147483e+09 &&
								(sender->nSeq <= seqNo ||
								seqNo < sender->nSeq - 2.147483e+09))) {
						memcpy(_rudp_rcv_buf, rcvBuf, result);
						memcpy(&_rudp_last_sender, &from, sizeof(from));
						_rudp_no_bytes = result;
						dprintf(D_FULLDEBUG, "\t\t\tValid data packet - queued to rcv_buf\n");
						continue;
					}
					ackLen = sizeof(rcvBuf);
					if (stripeMsg(rcvBuf, &ackLen, ACK_PACKET, seqNo, NULL, 0) < 0) {
						dprintf(D_ALWAYS, "rudp_send: stripeMsg failed\n");
						return -1;
					}
					while(1) {
						rst = send(fd, rcvBuf, ackLen, 0);
						if (rst != ackLen) {
							if (errno == EAGAIN || errno == EINTR) {
								continue;
							} else {   
								dprintf(D_ALWAYS, "rudp_send: send - %s\n", strerror(errno));
								return -1;
							}
						}
						dprintf(D_FULLDEBUG, "\t\tACK(%d) for delayed msg\n", seqNo);
						break;
					}
					continue;
				case ACK_PACKET:
					dprintf(D_FULLDEBUG, "\t\tACK(%d) received\n", seqNo);
					if (seqNo != _rudp_seqno) {
						continue;
					}
					return msgLen;
			}
		} // while (1)
		tries++;
		dprintf(D_FULLDEBUG, "\n\t\ttries = %d\n\n", tries);
	} // while(tries < 7)

	dprintf(D_ALWAYS, "rudp_send: couldn't get ACK from receiver\n");
	return -1;
}


int
rudp_recv(int fd, void *msg, size_t msgLen, int flags)
{
	uint32_t ip, gip;
	pid_t pid;
	int bufLen, mLen, rst;
	unsigned int seqNo;
	char opcode, rcvBuf[65536], *msgPtr, *buf;
	struct senderInfo *sender;
	unsigned long sec, usec;

	dprintf(D_FULLDEBUG, "\tRECV:\n");

	while(1) {
		if (_rudp_no_bytes != 0) {
			buf = _rudp_rcv_buf;
			bufLen = _rudp_no_bytes;
			_rudp_no_bytes = 0;
		} else {
			bufLen = recv(fd, rcvBuf, sizeof(rcvBuf), flags);
			if (bufLen < 0) {
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				} else {
					dprintf(D_ALWAYS, "rudp_recv: recv - %s\n", strerror(errno));
					return -1;
				}
			}
			buf = rcvBuf;
		}

		rst = unstripeMsg(buf, bufLen, &opcode, &ip, &gip, &pid, &sec,
							&usec, &seqNo, &msgPtr, &mLen);

		if ( rst != 1) {
			dprintf(D_ALWAYS, "rudp_recv: unstripeMsg failed\n");
			continue;
		}

		if (opcode == ACK_PACKET) {
			dprintf(D_FULLDEBUG, "\t\tACK(%d) received\n", seqNo);
			continue;
		}
		dprintf(D_FULLDEBUG, "\t\tDATA(%d) received\n", seqNo);

		// send ACK
		bufLen = sizeof(buf);
		if (stripeMsg(buf, &bufLen, ACK_PACKET, seqNo, NULL, 0) < 0) {
			dprintf(D_ALWAYS, "rudp_recv: stripeMsg failed\n");
			return -1;
		}
		while(1) {
			rst = send(fd, buf, bufLen, 0);
			if (rst != bufLen) {
				if (errno == EAGAIN || errno == EINTR) {
					continue;
				} else {   
					dprintf(D_ALWAYS, "rudp_recv: send - %s\n", strerror(errno));
					return -1;
				}
			}
			dprintf(D_FULLDEBUG, "\t\tACK(%d) sent\n", seqNo);
			break;
		}

		// get the pointer to sender info
		sender = getSender(ip, gip, pid, sec, usec);

		// Ignore delayed msg. I added the 2nd condition for the case of
		// wrapping around of seqNo. 2.147483e+09 = 2 to the 31 = 1/2 of the
		// biggest 4 bytes integer
		if ((sender->nSeq <= 2.147483e+09 &&
					(seqNo < sender->nSeq ||
					 sender->nSeq + 2.147483e+09 < seqNo)) ||
			(sender->nSeq > 2.147483e+09 &&
					sender->nSeq - 2.147483e+09 < seqNo &&
					seqNo < sender->nSeq)) {
			dprintf(D_FULLDEBUG, "\t\tDelayed data(%d) ignored: data(%d) expected\n", seqNo, sender->nSeq);
			continue;
		}
		sender->nSeq = seqNo + 1;

		// pass the received data to application
		memcpy(msg, msgPtr, mLen);
		return mLen;
	}
}
