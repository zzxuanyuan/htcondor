// errno and corresponding error message
extern int fwerrno;
extern const char *errMsg;

// Functions
int kRuleSet(int protocol, int command,
            unsigned int lip, unsigned short lport,
            unsigned int rip, unsigned short rport );

struct fwRule * kRuleList ( int protocol );

char *strError(void);
