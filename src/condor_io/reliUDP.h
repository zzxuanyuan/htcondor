struct rudpaddr {
	uint32_t ip;
	uint32_t gip;
	pid_t pid;
	unsigned long sec;
	unsigned long usec;
};

struct senderInfo {
	uint32_t ip;
	uint32_t gip;
	pid_t pid;
	unsigned long sec;
	unsigned long usec;
	unsigned int nSeq;
	struct senderInfo *next;
};

/* Send a message through a UDP socket in an error free way */
// ACK from receiver after checksum check done
// No in order delivery. No flow control. No congestion control
// @args:	fd -	UDP socket fd
//			msg -	buffer containing the msg to be sent
//			msgLen-	the length of 'msg'
//			to -	sockaddr of receiver
//			tolen -	the length of 'to'
// @return:	the length of the message sent, if succeed
//			-1, if error occured
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

/* Receive a message through a UDP socket in an error free way */
// @args:   fd- UDP socket discreptor
//          msg- the buffer to store the application level message
//          msgLen- the length of the above buffer
//          flags, from, fromlen- same as the flags of 'recvfrom'
//          raddr-  sender's {ip, gip, pid, tv_sec, tv_usec}. If the received
//                  message is not rudp message, raddr will be set to NULL
// @return: the length of the application level msg, if succeed
//          -1, if failed
int rudp_recvfrom(
		int fd,
		void *msg,
		size_t msgLen,
		int flags,
		struct sockaddr *from,
		socklen_t *fromlen,
		struct rudpaddr *raddr);
int rudp_recv(int fd, void *msg, size_t msgLen, int flags);
