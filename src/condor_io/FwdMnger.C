#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include <net/if.h>
#include "getParam.h"
#include "condor_rw.h"
#include "portfw.h"
extern "C" {
#include "fwkernel.h"
}
#include "FwdMnger.h"

#define IPPROTO_NONE    65535
#define IP_PORTFW_DEF_PREF 10

/*
 *  *  Nice glibc2 kLuDge
 *   */
#if defined(__GLIBC__) || (__GLIBC__ > 2)
#define _LINUX_IN_H
#define _LINUX_IP_H
#define _LINUX_ICMP_H
#define _LINUX_UDP_H
#define _LINUX_TCP_H
#define _LINUX_BYTEORDER_GENERIC_H
#endif



FwdMnger::FwdMnger (char *proto)
{
	for (int i=0; i<NAT_MAX_HASH; i++) {
		_locals[i] = NULL;
		_remotes[i] = NULL;
	}

	if (!strcmp (proto, TCP)) {
		_protocol = SOCK_STREAM;
		_persistFile = ".condor_tcp_port_fwd";
	} else if (!strcmp (proto, UDP)) {
		_protocol = SOCK_DGRAM;
		_persistFile = ".condor_udp_port_fwd";
	} else {
		EXCEPT ("FwdMnger::FwdMnger - invalid protocol");
	}
}


// Algorithm
// 	- if (rip, rport) is in _remotes[]
// 		+ if (lip, lport) is given
// 			: if (lip, lport)->(rip, rport, mport) is in the rule set, ALREADY_SET
// 			: else, CONFLICT_RULE
// 		+ else - use the rule which has been setup
//			: set (*lip, *lport) with (lip, lport)->(rip, rport, mport)
//			: set (rip, rport, mport) with the given mport
//			: ALEADY_SET
// 	- else
// 		+ if (lip, lport) is given
// 			: if (lip, lport) is in _locals[], LOCAL_NOAVAIL
// 			: else
// 				. mark (lip, lport) as being used
// 				. add (lip, lport)->(rip, rport, mport), SUCCESS
// 		+ else
// 			: find free (lip, lport)
// 			: add (lip, lport)->(rip, rport, mport), SUCCESS
int
FwdMnger::addInternal (	unsigned int * lip,
						unsigned short * lport,
						unsigned int rip,
						unsigned short rport,
						unsigned short mport)
{
	local *lt;
	remote *rt = _remotes[rip % NAT_MAX_HASH];
	bool found = false;
	while ( !found && rt ) {
		if (rt->ip == rip && rt->port == rport) {
			found = true;
		} else {
			rt = rt->next;
		}
	}

		/* (rip, rport) is in _remotes[] */
	if (found) {
		lt = rt->lo;
		if (*lip != 0 && *lport != 0) {
				/* (lip, lport) is given */
			if (*lip == lt->ip && *lport == lt->port) {
					/* (lip, lport)->(rip, rport, mport) is in the rule set */
				if (rt->mport != mport) {
					rt->mport = mport;
					rt->failed = 0;
					return RULE_REUSED;
				}
				return ALREADY_SET;
			} else {
					/* (lip, lport)->(rip, rport, mport) is NOT in the rule set */
				return COLFLICT_RULE;
			}
		}
			/* (lip, lport) is NOT given */
		// set (*lip, *lport) with (lip, lport)->(rip, rport, mport)
		*lip = lt->ip;
		*lport = lt->port;
		// set (rip, rport, mport) with the given mport
		rt->mport = mport;
		rt->failed = 0;
		return RULE_REUSED;
	}

		/* (rip, rport) is NOT in _remotes[] */
	if (*lip != 0 && *lport != 0) {
			/* (lip, lport) is given */
		lt = _locals[*lport % NAT_MAX_HASH];
		while (lt) {
			if (lt->ip == *lip && lt->port == *lport) {
					/* (lip, lport) is in _locals[] */
				return LOCAL_NOAVAIL;
			}
			lt = lt->next;
		}
			/* (lip, lport) is NOT in _locals[] */
				/* mark (lip, lport) as being used */
		if ( !freePortMnger.makeOccupied (*lip, *lport)) {
			dprintf (D_ALWAYS, "FwdMnger::addInternal - failed to mark the port as being used\n");
			return INTERNAL_ERR;
		}
	} else {
			/* (lip, lport) is NOT given */
				/* find free (lip, lport) */
		if ( !freePortMnger.nextFree (rport, lip, lport) ) {
			dprintf (D_ALWAYS, "FwdMnger::addInterna - can't find free (lip, lport)\n");
			return NO_MORE_FREE_PORT;
		}
	}

		/* add (lip, lport)->(rip, rport, mport) */
	lt = _locals[*lport % NAT_MAX_HASH];
	rt = _remotes[rip % NAT_MAX_HASH];
	local *ltmp = new local();
	remote *rtmp = new remote();
	ltmp->ip = *lip;
	ltmp->port = *lport;
	ltmp->rm = rtmp;
	ltmp->prev = NULL;
	ltmp->next = lt;
	if (lt) {
		lt->prev = ltmp;
	}
	_locals[*lport % NAT_MAX_HASH] = ltmp;

	rtmp->ip = rip;
	rtmp->port = rport;
	rtmp->mport = mport;
	rtmp->failed = 0;
	rtmp->lo = ltmp;
	rtmp->prev = NULL;
	rtmp->next = rt;
	if (rt) {
		rt->prev = rtmp;
	}
	_remotes[rip % NAT_MAX_HASH] = rtmp;

	return SUCCESS;
}


// Algorithm
// 	- if (lip, lport)->(?ip, ?port, ?mport) is NOT in the set of rules, RULE_NOT_FOUND
// 	- make (lip, lport) free
// 	- set rip, rport with ?ip, ?port, respectively
// 	- delete (lip, lport)->(rip, rport, mport)
int
FwdMnger::deleteInternal (	unsigned int lip,
							unsigned short lport,
							unsigned int * rip,
							unsigned short * rport)
{
	remote *rptr;
	local * lptr = _locals[lport % NAT_MAX_HASH];
	bool found = false;
	while ( !found && lptr ) {
		if (lptr->ip == lip && lptr->port == lport) {
			found = true;
		} else {
			lptr = lptr->next;
		}
	}

	if (!found) {
		return RULE_NOT_FOUND;
	}

	if ( !freePortMnger.makeFree (lip, lport) ) {
		return INTERNAL_ERR;
	}

	rptr = lptr->rm;
	*rip = rptr->ip;
	*rport = rptr->port;

	if (lptr->prev) {
		lptr->prev->next = lptr->next;
	} else {
		_locals[lport % NAT_MAX_HASH] = lptr->next;
	}
	if (lptr->next) {
		lptr->next->prev = lptr->prev;
	}
	delete lptr;

	if (rptr->prev) {
		rptr->prev->next = rptr->next;
	} else {
		_remotes[*rip % NAT_MAX_HASH] = rptr->next;
	}
	if (rptr->next) {
		rptr->next->prev = rptr->prev;
	}
	delete rptr;

	return SUCCESS;
}


// Synchronize kernel_forwarding and in-memory representation to condor_forwarding file
// Rationale: conservative approach - any unnecessary rule will get garbage collected
//
// Basic Algorithm
//	get rules from internal representation
//	for each entry in condor_forwarding file
// 		check if the corresponding entry is also in kernel_forwarding file too
// 		if not, add the rule to the kernel
//		add the rule to the in-memory representation. Multiple insertion does not cause problem!
// 		delete the rule from rules got at the first step
// 	if there is any rule remained in the rules, delete the rule from the internal representation
// 	and log error
void
FwdMnger::sync ()
{
	dprintf(D_FULLDEBUG, "\tSync:\n");

	FILE * c_fp = NULL;

	// get rules from internal representation
	struct fwRule * rules = getRules ();
	struct fwRule * newRules = NULL;

	// open condor_forward file, if there is
	c_fp = fopen (_persistFile, "r");
	if (!c_fp) {
		if (rules) {
			EXCEPT ("FwdMnger::sync - no persist file but _L2R is not empty");
			return;
		}
		dprintf (D_FULLDEBUG, "FwdMnger::sync - No persist forwarding file found\n");
		return;
	}

	// get rules from kernel
	struct fwRule * kRules;
   	if ( kRuleList (_protocol, &kRules) < 0 ) {
		dprintf(D_ALWAYS, "FwdMnger::sync - failed to get kernel rules\n");
		EXCEPT("\t%s", strError());
	}

	unsigned int c_lip, c_rip, t_lip, t_rip;
	unsigned short c_lport, c_rport, c_mport, t_lport, t_rport, t_mport;
	unsigned int h_lip, h_rip;
	unsigned short h_lport, h_rport;
	char c_buf[100];
	char t_buf[100];
	char proto[5];
	if (_protocol == SOCK_STREAM) {
		strcpy (proto, "TCP");
	} else {
		strcpy (proto, "UDP");
	}

	/********************************************/
	/* for each entry in condor_forwarding file */
	/********************************************/
	dprintf(D_FULLDEBUG, "\t\tFor each rule in persistent file:\n");
	while ( fgets (c_buf, 100, c_fp) != NULL ) {
		// read an entry from condor_forward file: ip-addr and port # is in network-byte order
		char tLport[10], tRport[10], tMport[10];
		if (sscanf (c_buf, "%x %s %x %s %s", &c_lip, tLport, &c_rip, tRport, tMport) != 5) {
			EXCEPT("FwdMnger::sync - sscanf(c_fp) failed: ");
		}
		c_lport = atoi(tLport);
		c_rport = atoi(tRport);
		c_mport = atoi(tMport);
		char t1buf[50], t2buf[50];
		sprintf(t1buf, "%s", ipport_to_string(c_lip, c_lport));
		sprintf(t2buf, "%s [%d]", ipport_to_string(c_rip, c_rport), ntohs(c_mport));
		dprintf(D_FULLDEBUG, "\t\t\t%s->%s\n", t1buf, t2buf);

		/********************************************/
		/* Check if the same is in the kernel       */
		/*   if not, add the rule in the kernel     */
		/********************************************/
		struct fwRule *tRule = kRules;
		bool found = false;
		while ( tRule && !found ) {
			if (tRule->lip == c_lip && tRule->lport == c_lport) {
				// (lip, lport) matches
				if (tRule->rip != c_rip || tRule->rport != c_rport) {
					// (rip, rport) does not match
					dprintf(D_ALWAYS, "For rule: %s->%s\n", t1buf, t2buf);
					EXCEPT("FwdMnger::sync - lip and lport match with kernel table but rip or rport dismatch\n");
				}
				dprintf(D_FULLDEBUG, "\t\t\t\tFound the same rule in kernel\n");
				found = true;
			} else {
				tRule = tRule->next;
			}
		}
		if ( !found ) {
			dprintf(D_ALWAYS, "\t\t\t\tNo rule: %s->? found in kernel\n", ipport_to_string(c_lip, c_lport));
			// add the rule to the kernel
			if (kRuleSet(_protocol, CMD_ADD, c_lip, c_lport, c_rip, c_rport ) < 0) {
				dprintf(D_ALWAYS, "FwdMnger::sync - failed to add the rule to the kernel\n");
				EXCEPT("\t%s\n", strError());
			}
			dprintf(D_ALWAYS, "\t\t\t\tAdded to the kernel: %s->%s\n", t1buf, t2buf);
		}

		/********************************************/
		/* Add the rule to in-memory representation */
		/********************************************/

		// find the rule in the rule-list which we got at the top
		struct fwRule *cur, *prev, *next;
		prev = next = NULL;
		cur = rules;
		while (cur) {
			if (cur->rip == c_rip && cur->rport == c_rport) {
				next = cur->next;
				break;
			} else {
				prev = cur;
				cur = cur->next;
			}
		}

		if (cur) {
			// A rule * -> (rip, rport) found
			if (cur->lip == c_lip && cur->lport == c_lport && cur->mport == c_mport) { // Exactly the same rule
				// Move the rule from rule-list to new rule-list
				dprintf(D_FULLDEBUG, "\t\t\t\tThe rule was in the internal representation, as expected!\n");
				// move the rule from the rule-list to the new rule-list
				if (prev) prev->next = next;
				else rules = next;
				cur->next = newRules;
				newRules = cur;
				dprintf(D_FULLDEBUG, "\t\t\t\tMoved the rule to the new rule-list\n");
			} else { // Rule reused
				// Same rule with different (lip, lport, mport) should be found later in condor_persist file
			}
		} else { // Need to add the rule to in-memory representation
			int result = addInternal (&c_lip, &c_lport, c_rip, c_rport, c_mport);
			if (result == SUCCESS) {
				dprintf(D_FULLDEBUG, "\t\t\t\tAdded to the internal representation\n");
				// add the fule to the new rule-list
				struct fwRule *tmpRule = (struct fwRule *)malloc(sizeof(struct fwRule));
				tmpRule->lip = c_lip;
				tmpRule->lport = c_lport;
				tmpRule->rip = c_rip;
				tmpRule->rport = c_rport;
				tmpRule->mport = c_mport;
				tmpRule->next = newRules;
				newRules = tmpRule;
				dprintf(D_FULLDEBUG, "\t\t\t\tAdded to the new rule-list\n");
			} else if (result == RULE_REUSED) {
				// replace the rule in the new rule list
				bool done = false;
				struct fwRule *temp = newRules;
				while (temp && !done) {
					if (temp->lip == c_lip && temp->lport == c_lport &&
						temp->rip == c_rip && temp->rport == c_rport && temp->mport != c_mport)
					{
						temp->mport = c_mport;
						done = true;
					}
					temp = temp->next;
				}
				if (!done) {
					EXCEPT("FwdMnger::sync - weird!");
				}
			} else if (result == ALREADY_SET) {
				// Nothing need to be done
			} else {
				EXCEPT("FwdMnger::sync - addInternal failed");
			}
		}
	}
	if ( !feof (c_fp) ) {
		EXCEPT ("FwdMnger::sync - fgets(c_fp) failed: ");
	}
	fclose (c_fp);

	struct fwRule *tmpRule = kRules;
	while(tmpRule) {
		struct fwRule *tmp = tmpRule;
		tmpRule = tmpRule->next;
		free(tmp);
	}

	/**************************************************/
	/* Write the new rules to the condor persist file */
	/**************************************************/
	if (newRules) {
		dprintf(D_FULLDEBUG, "\t\tWriting verified rules to condor persist file\n");
		// open the temp file
		FILE * t_fp = fopen (".condor_port_fwd.tmp", "w+");
		if ( !t_fp ) {
			EXCEPT ("FwdMnger::sync - failed to open temp file: ");
		}
		// write the rules to the temp file
		struct fwRule *curr = newRules, *temp = NULL;
		char t1buf[50], t2buf[50];
		while (curr) {
			fprintf (t_fp, "%x %d %x %d %d\n", curr->lip, curr->lport, curr->rip, curr->rport, curr->mport);
			sprintf(t1buf, "%s", ipport_to_string(curr->lip, curr->lport));
			sprintf(t2buf, "%s [%d]", ipport_to_string(curr->rip, curr->rport), ntohs(curr->mport));
			dprintf(D_FULLDEBUG, "\t\t\t%s->%s\n", t1buf, t2buf);
			temp = curr;
			curr = curr->next;
			free (temp);
		}
		// close the temp file
		fclose (t_fp);

		// rename the temp file
		if (rename (".condor_port_fwd.tmp", _persistFile)) {
			char err[100];
			sprintf(err, "FwdMnger::sync - rename failed: %s", strerror(errno));
			EXCEPT (err);
		}
	} else {
		if (unlink (_persistFile)) {
			char err[100];
			sprintf(err, "FwdMnger::sync - unlink failed: %s", strerror(errno));
			EXCEPT (err);
		}
	}

	dprintf(D_ALWAYS, "\t\tDeleting leftover internal rules:\n");
	while (rules) {
		if (deleteInternal (rules->lip, rules->lport, &(rules->rip), &(rules->rport)) != SUCCESS) {
			EXCEPT ("FwdMnger::sync - deleteInternal failed");
		}
		dprintf(D_ALWAYS, "\t\t\t%s->%s [%d]\n", ipport_to_string(rules->lip, rules->lport), ipport_to_string(rules->rip, rules->rport), ntohs(rules->mport));
		struct fwRule *temp = rules;
		rules = rules->next;
		free(temp);
	}

	return;
}


void
FwdMnger::addInterface(char *dotNotation)
{
	struct in_addr inp;
	if ( !inet_aton(dotNotation, &inp) ) {
		EXCEPT ("FwdMnger::addInterface - inet_aton failed");
	}
	unsigned int ipAddr = inp.s_addr;

	freePortMnger.addInterface(ipAddr);

	return;
}


int
FwdMnger::addRule ( unsigned int *l_ip,
					unsigned short *l_port,
					const unsigned int r_ip,
					const unsigned short r_port,
					unsigned short m_port)
{
	dprintf(D_FULLDEBUG, "\tAdd: \n");
	int sock;
	sockaddr_in address;
	int result;
	unsigned int t_int, d_ip;
	unsigned short t_short, d_port;

	// See if m_port is being used by the same host as rport
	//	if this is the case, we can sure that the previous rule is a garbage and
	//	delete the rule without having to wait until timeout
	remote *rt = _remotes[r_ip % NAT_MAX_HASH];
	while ( rt ) {
		if (rt->ip == r_ip && rt->port == m_port) {
			if (deleteRule(rt->lo->ip, rt->lo->port, &d_ip, &d_port) != SUCCESS) {
				EXCEPT("FwdMnger::addRule - failed to delete reused rule");
			}
			rt = _remotes[r_ip % NAT_MAX_HASH];
		}
		rt = rt->next;
	}

	// update internal data structure
	// let 'addInternal' find a free proxy (ip, port) pair, if necessary
	*l_ip = *l_port = 0;
	result = addInternal (l_ip, l_port, r_ip, r_port, m_port);
	if (result == ALREADY_SET) {
		return SUCCESS;
	} else if (result != SUCCESS && result != RULE_REUSED) {
		dprintf(D_ALWAYS, "FwdMnger::addRule - Adding the rule to internal data structure failed: \n");
		return result;
	}
	dprintf(D_FULLDEBUG, "\t\tInternal: %s->%s [%d]\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port), ntohs(m_port));

	// write the rule to the persist file: we write the forwarding rule to the condor
	// persist file before seting up actual forwarding rule, which will write the
	// forwarding rule to kernel persist file. At some bad situation, we will have some
	// garbage rules remained in our condor persist file, but this causes no harm and
	// the garbage will be cleaned up periodically by 'sync()'
	char buf[100];
	FILE * fp = fopen (_persistFile, "a+");
	if (!fp) {
		dprintf (D_ALWAYS, "FwdMnger::addRule - fopen failed\n");
		return INTERNAL_ERR;
	}
	memset (buf, 0, sizeof(buf));
	sprintf (buf, "%x %d %x %d %d\n", *l_ip, *l_port, r_ip, r_port, m_port);
	fputs (buf, fp);
	fclose (fp);
	dprintf(D_FULLDEBUG, "\t\tPersistent: %s->%s [%d]\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port), ntohs(m_port));
	if (result == RULE_REUSED) {
		return SUCCESS;
	}

	// now setup the port forwarding rule
	if (kRuleSet(_protocol, CMD_ADD, *l_ip, *l_port, r_ip, r_port ) < 0) {
		dprintf(D_ALWAYS, "FwdMnger::addRule - failed to add the rule to the kernel\n");
		EXCEPT("\t%s\n", strError());
	}
	dprintf(D_FULLDEBUG, "\t\tKernel: %s->%s\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port));
	return SUCCESS;
}


int
FwdMnger::queryRule(const unsigned int lip,
					const unsigned short lport,
					unsigned int *rip,
					unsigned short *rport)
{
	dprintf(D_ALWAYS, "\tqueryRule:\n");
	remote *rptr;
	local *lptr = _locals[lport % NAT_MAX_HASH];
	bool found = false;
	while ( !found && lptr ) {
		if (lptr->ip == lip && lptr->port == lport) {
			found = true;
		} else {
			lptr = lptr->next;
		}
	}

	if (!found) {
		return RULE_NOT_FOUND;
	}

	rptr = lptr->rm;
	*rip = rptr->ip;
	*rport = rptr->port;

	return SUCCESS;
}


void
FwdMnger::garbage (	const unsigned int l_ip,		// in network byte order
					const unsigned short l_port,	// in network byte order
					unsigned int r_ip,				// in network byte order
					unsigned short r_port)			// in network byte order
	// increase failed count
	// if failed more than threshold, delete the rule
{
	dprintf(D_FULLDEBUG, "\tgarbage: %s->%s\n", ipport_to_string(l_ip, l_port), ipport_to_string(r_ip, r_port));

	// find the rule
	remote *rptr;
	local * lptr = _locals[l_port % NAT_MAX_HASH];
	bool found = false;
	while ( !found && lptr ) {
		if (lptr->ip == l_ip && lptr->port == l_port) {
			found = true;
		} else {
			lptr = lptr->next;
		}
	}

	if (!found) {
		dprintf (D_ALWAYS, "FwdMnger::garbage - can't find the rule\n");
		return;
	}

	if (lptr->rm->failed++ < 2) {
		dprintf (D_FULLDEBUG, "\t\tfailed = %d\n", lptr->rm->failed);
		return;
	}

	(void) deleteRule (l_ip, l_port, &r_ip, &r_port);
	return;
}


int
FwdMnger::deleteRule (	const unsigned int l_ip,		// in network byte order
						const unsigned short l_port,	// in network byte order
						unsigned int *r_ip,				// in network byte order
						unsigned short *r_port)			// in network byte order
{
	dprintf(D_FULLDEBUG, "\tdeleteRule:\n");
	// delete internal data structure first
	int result = deleteInternal(l_ip, l_port, r_ip, r_port);
	if (result == SUCCESS) {
		dprintf(D_FULLDEBUG, "\t\tfrom Internal: %s->%s\n", ipport_to_string(l_ip, l_port), ipport_to_string(*r_ip, *r_port));
	} else {
		dprintf(D_ALWAYS, "FwdMnger::deleteRule - deleteInternal failed: result = %d\n", result);
	}

	// unset forwarding rule here
	if (kRuleSet(_protocol, CMD_DEL, l_ip, l_port, *r_ip, *r_port ) < 0) {
		dprintf(D_ALWAYS, "FwdMnger::deleteRule - failed to delete the rule to the kernel\n");
		EXCEPT("\t%s\n", strError());
	}

	dprintf(D_FULLDEBUG, "\t\tfrom Kernel\n");

	return deletePersist (l_ip, l_port, *r_ip, *r_port);
}


int
FwdMnger::deletePersist (const unsigned int lip,
						 const unsigned short lport,
						 const unsigned int rip,
						 const unsigned short rport)
{
	bool added = false, changed = false;

	// open the persist file
	FILE * fp = fopen (_persistFile, "r");
	if (!fp) {
		dprintf (D_ALWAYS, "FwdMnger::deletePersist - faile to fopen(persist file)\n");
		return INTERNAL_ERR;
	}

	// open a temp file
	FILE *temp = fopen (".condor_port_fwd.tmp", "w+");
	if (!temp) {
		dprintf (D_ALWAYS, "FwdMnger::deletePersist - faile to fopen(temp file)\n");
		return INTERNAL_ERR;
	}

	// copy line by line to the new file
	char buf[100];
	bool done = false;
	unsigned int c_lip, c_rip;
	unsigned short c_lport, c_rport, c_mport;
	char tLport[10], tRport[10], tMport[10];
	while ( fgets (buf, 100, fp) != NULL ) {
		if (sscanf (buf, "%x %s %x %s %s", &c_lip, tLport, &c_rip, tRport, tMport) != 5) {
			dprintf (D_ALWAYS, "FwdMnger::deletePersist - sscanf failed\n");
			return INTERNAL_ERR;
		}
		c_lport = atoi(tLport);
		c_rport = atoi(tRport);
		c_mport = atoi(tMport);
		if (lip == c_lip && lport == c_lport && rip == c_rip && rport == c_rport) {
			done = true;
		} else {
			sprintf (buf, "%x %d %x %d %d\n", c_lip, c_lport, c_rip, c_rport, c_mport);
			fputs (buf, temp);
			added = true;
		}
	}
	if (!feof (fp)) {
		dprintf (D_ALWAYS, "FwdMnger::deletePersist - fgets error\n");
		fclose (fp);
		fclose (temp);
		return INTERNAL_ERR;
	}
	fclose (fp);
	fclose (temp);
	if (!done) {
		dprintf (D_ALWAYS, "FwdMnger::deletePersist - can't find the record to delete\n");
		return INTERNAL_ERR;
	}

	if (added) {
		if (rename (".condor_port_fwd.tmp", _persistFile)) {
			char err[100];
			sprintf(err, "FwdMnger::sync - rename failed: %s", strerror(errno));
			EXCEPT (err);
		}
	} else {
		if (unlink (_persistFile)) {
			char err[100];
			sprintf(err, "FwdMnger::deletePersist - unlink failed: %s", strerror(errno));
			EXCEPT (err);
		}
	}

	dprintf(D_ALWAYS, "\t\tfrom Persistent: success\n");
	return SUCCESS;
}


struct fwRule *
FwdMnger::getRules ()
{
	dprintf(D_ALWAYS, "\t\tRules:\n");

	struct fwRule * rules = NULL;
	local * lptr;
	for (int i=0; i<NAT_MAX_HASH; i++) {
		lptr = _locals[i];
		while ( lptr ) {
			struct fwRule * temp = (struct fwRule *) malloc (sizeof (struct fwRule));
			temp->lip = lptr->ip;
			temp->lport = lptr->port;
			temp->rip = lptr->rm->ip;
			temp->rport = lptr->rm->port;
			temp->mport = lptr->rm->mport;
			temp->next = rules;
			char t1buf[50], t2buf[50];
			sprintf(t1buf, "%s", ipport_to_string(temp->lip, temp->lport));
			sprintf(t2buf, "%s [%d]", ipport_to_string(temp->rip, temp->rport), ntohs(temp->mport));
			dprintf(D_ALWAYS, "\t\t\t%s->%s\n", t1buf, t2buf);
			rules = temp;
			lptr = lptr->next;
		}
	}

	return rules;
}
