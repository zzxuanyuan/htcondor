#ifndef _FWKERNER_H
#define _FWKERNER_H
/* Append or delete a rule to/from the kernel table
 *	@args:
 *		- protocol: SOCK_STREAM or SOCK_DGRAM 
 *		- command: CMD_ADD or CMD_DEL
 *		- lip, lport, rip, rport: all in network byte order
 *	@return:
 *		- 0 if succeed
 *		- -1 if failed
 */
int kRuleSet(int protocol, int command,
            unsigned int lip, unsigned short lport,
            unsigned int rip, unsigned short rport );

/* Get the list of rules in the kernel table
 *	@args:
 *		- protocol: SOCK_STREAM or SOCK_DGRAM 
 *		- rules
 *			+ should be NULL when it is passed to this function. Otherwise
 *			  memory leakage will occur
 *			+ will be pointed to the linked list of fwRules as a result
 *			+ lip, lport, rip, and rport of fwRule rule are in network byte order
 *	@return:
 *		- 0 if succeed
 *		- -1 if failed
 */
int kRuleList (int protocol, struct fwRule **rules);

/* Returns the error string which explains the most recent error */
char *strError(void);
#endif
