/* protocol */
#define TCP "tcp"
#define UDP "udp"

/* req. code */
#define ADD "add"
#define DELETE "delete"
#define QUERY "query"

/* response code */
#define NAK "nak"
#define ADDED "added"
#define DELETED "deleted"
#define RESULT "result"

/* error codes */
#define SUCCESS 0
#define INVALID_CMD 1
#define INVALID_PROTO 2
#define INVALID_PORT 3
#define NO_MORE_FREE_PORT 4
#define NOT_FOUND 5
#define ALREADY_SET 6
#define INTERNAL_ERR 9

int setFWrule ( struct sockaddr_in masqServer,
				char *cmd,
				char *proto,
				unsigned int inIP,
				unsigned short inPort,
				unsigned int * outIP,
				unsigned short * outPort,
				unsigned short mport);
