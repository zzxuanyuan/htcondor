/* protocol */
#define TCP "tcp"
#define UDP "udp"

/* req. code */
#define ADD "add"
#define DELETE "delete"
#define QUERY "query"

/* command codes */
#define CMD_ADD 1
#define CMD_DEL 2

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
#define ALREADY_SET 5
#define RULE_REUSED 6
#define LOCAL_NOAVAIL 7
#define COLFLICT_RULE 8
#define RULE_NOT_FOUND 9
#define INTERNAL_ERR 19

struct fwRule {
	unsigned int lip;
	unsigned short lport;
	unsigned int rip;
	unsigned short rport;
	unsigned short mport;
	struct fwRule * next;
};

int setFWrule ( struct sockaddr_in masqServer,
				char *cmd,
				char *proto,
				unsigned int inIP,
				unsigned short inPort,
				unsigned int * outIP,
				unsigned short * outPort,
				unsigned short mport);
