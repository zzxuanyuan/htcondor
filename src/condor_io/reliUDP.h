#ifndef _RUDP_HEADER
#define _RUDP_HEADER

#define _RUDP_BUCKET_SIZE 73
#define R_UDP_MAX_LEN 1024

struct rudpaddr {
	uint32_t ip;
	uint32_t gip;
	pid_t pid;
	unsigned long sec;
	unsigned long usec;
};

struct _RUDP_rmsg {
	char *msg;
	int seq;
	int bytes;
	struct sockaddr_in from;
	struct _RUDP_rmsg *next;
};

struct _RUDP_senderInfo {
	uint32_t ip;		// ip addr in network byte order
	uint32_t gip;		// gateway ip addr in network byte order
						// valid only if ip is private one
	pid_t pid;			// process id
	unsigned long sec;	// time of the birth. Actually the time when the 1st
	unsigned long usec; // rudp function is called
	unsigned int seq;	// sequence no of the last msg received from this sender
	struct _RUDP_senderInfo *next;
};

struct _RUDP_senders {
	int ref;
	struct _RUDP_senderInfo *senderArr[_RUDP_BUCKET_SIZE];
};

struct _RUDP_FDinfo {
	int conn;
	struct _RUDP_rmsg *qhead;
	struct _RUDP_rmsg *qtail;
	struct _RUDP_senders *senders;
};


/* Create an rudp socket */
// @return: fd of the UDP socket
int rudp_socket(void);

/* Close an rudp socket */
// @args: fd - file descriptor of the udp socket
// @return: 0, if succeed
//			-1, if receiving buffer is not empty
int rudp_close(int fd);

int rudp_dup2(int oldfd, int newfd);

/* Send a message through a UDP socket in error free way */
// ACK from receiver after checksum check done
// No in order delivery. No flow control. No congestion control
// @args:	fd -	UDP socket fd
//			msg -	buffer containing the msg to be sent
//			msgLen-	the length of 'msg'. Max is 1024
//			to -	sockaddr of receiver
//			tolen -	the length of 'to'
// @return:	the length of the message sent, if succeed
//			-1, if error occured
//
// Note: this function does not return until one of the followings occurs:
//			- an ACK for the packet sent gets received
//			- ACK is not delivered until max tries
//			- Error from kernel returned
// Note: returning success means that the packet has been queued at the receiver side
//       and does not necessarily means that the packet will get processed by the receiver
int rudp_sendto(
		int fd,
		const void *msg,
		size_t msgLen,
		const struct sockaddr *to,
		socklen_t tolen);
int rudp_send(
		int fd,
		const void *msg,
		size_t msgLen);

/* Receive a message through a UDP socket in error-free way */
// @args:   fd- UDP socket discreptor
//          msg- the buffer to store the application level message
//          msgLen- the length of the above buffer
//          flags, from, fromlen- same as the flags of 'recvfrom'
// @return: the length of the application level msg, if valid DATA packet
//          received
//          0, if nonblocking read is requested and no data packet arrived
//          -1, if failed
int rudp_recvfrom(
		int fd,
		void *msg,
		size_t msgLen,
		int flags,
		struct sockaddr *from,
		socklen_t *fromlen);
int rudp_recv(int fd, void *msg, size_t msgLen, int flags);

#endif
