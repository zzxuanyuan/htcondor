#include <stdio.h>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <linux/types.h>
#include <asm/types.h>
#include <linux/ip_fw.h>

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
#include "getParam.h"
#include "condor_rw.h"
#include "portfw.h"
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



FwdMnger::FwdMnger (char *proto, int rawSock)
{
	_rawSock = rawSock;
	for (int i=0; i<NAT_MAX_HASH; i++) {
		_locals[i] = NULL;
		_remotes[i] = NULL;
	}

	// initialize the static part of _masq
	_masq.m_target = IP_MASQ_TARGET_MOD;
	_masq.m_cmd = IP_MASQ_CMD_NONE;
	strcpy(_masq.m_tname, "portfw");

	// _pfw is just a part of _masq
	if (!strcmp (proto, TCP)) {
		_protocol = SOCK_STREAM;
		_persistFile = ".condor_tcp_port_fwd";
		_pfw.protocol = IPPROTO_TCP;
	} else if (!strcmp (proto, UDP)) {
		_protocol = SOCK_DGRAM;
		_persistFile = ".condor_udp_port_fwd";
		_pfw.protocol = IPPROTO_UDP;
	} else {
		EXCEPT ("FwdMnger::FwdMnger - invalid protocol");
	}

	_pfw.pref = IP_PORTFW_DEF_PREF;
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
	remote *rt = _remotes[rport % NAT_MAX_HASH];
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
	rt = _remotes[rport % NAT_MAX_HASH];
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
	rtmp->lo = ltmp;
	rtmp->prev = NULL;
	rtmp->next = rt;
	if (rt) {
		rt->prev = rtmp;
	}
	_remotes[rport % NAT_MAX_HASH] = rtmp;

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
		_remotes[*rport % NAT_MAX_HASH] = rptr->next;
	}
	if (rptr->next) {
		rptr->next->prev = rptr->prev;
	}
	delete rptr;

	return SUCCESS;
}


// Synchronize condor_forwarding file and internal representation to kernel_forwarding file
// Rationale: kernel_forwarding table update serves as a commit of port forwarding transaction
//
// Basic Algorithm
//	get Rules from internal representation
//	for each entry in condor_forwarding file
// 		check if the corresponding entry is also in kernel_forwarding file too
// 		if not, delete the entry from condor_forwarding file and internal representation,
// 				and then log error
// 		if yes, copy the entry to the new persist file and
// 				make an internal data structure for the entry
// 		delete the rule from Rules got at the first step
// 	if there is any rule remained in the Rules, delete the rule from the internal representation
// 	and log error
void
FwdMnger::sync ()
{
	dprintf(D_ALWAYS, "\tSync:\n");

	bool changed = false, added = false;
	FILE * c_fp = NULL;
	FILE * t_fp = NULL;
	FILE * k_fp = NULL;
	const char *proc_names[] = {
		"/proc/net/ip_masq/portfw",
		"/proc/net/ip_portfw",
		NULL
	};
	const char **proc_name = proc_names;

	// get rules from internal representation
	Rule * rules = getRules ();

	// open condor_forward file, if there is
	c_fp = fopen (_persistFile, "r");
	if (!c_fp) {
		if (rules) {
			EXCEPT ("FwdMnger::sync - no persist file but _L2R is not empty");
			return;
		}
		dprintf (D_ALWAYS, "FwdMnger::sync - No persist forwarding file found\n");
		return;
	}

	// open temp file which will replace condor_forward file
	t_fp = fopen (".condor_port_fwd.tmp", "w+");
	if ( !t_fp ) {
		EXCEPT ("FwdMnger::sync - failed to open temp file: ");
	}

	// open kernel_forwarding file
	for (;*proc_name;proc_name++) {
		k_fp = fopen (*proc_name, "r");
		if (k_fp) {
			break;
		}
		dprintf(D_ALWAYS, "\t\tFailed to open %s: %s\n", *proc_name, strerror(errno));
	}
	if (!k_fp) {
		EXCEPT ("FwdMnger::sync - Could not open kernel_forwarding file\n");
	}

	int PrCnt, Pref;
	unsigned int c_lip, c_rip, k_lip, k_rip, t_lip, t_rip;
	unsigned short c_lport, c_rport, c_mport, k_lport, k_rport, t_lport, t_rport, t_mport;
	unsigned int h_lip, h_rip;
	unsigned short h_lport, h_rport;
	char c_buf[100];
	char k_buf[100];
	char t_buf[100];
	char p_name[10];
	char proto[5];
	if (_protocol == SOCK_STREAM) {
	   	strcpy (proto, "TCP");
	} else {
		strcpy (proto, "UDP");
	}
	// for each entry in condor_forwarding file
	dprintf(D_ALWAYS, "\t\tFor each rule in persistent file:\n");
	while ( fgets (c_buf, 100, c_fp) != NULL ) {
		// read an entry from condor_forward file: ip-addr and port # is in network-byte order
		char tLport[10], tRport[10], tMport[10];
		if (sscanf (c_buf, "%x %s %x %s %s", &c_lip, tLport, &c_rip, tRport, tMport) != 5) {
			dprintf(D_ALWAYS, "FwdMnger::sync - sscanf(c_fp) failed: ");
			continue;
		}
		c_lport = atoi(tLport);
		c_rport = atoi(tRport);
		c_mport = atoi(tMport);
		char t1buf[50], t2buf[50];
		sprintf(t1buf, "%s", ipport_to_string(c_lip, c_lport));
		sprintf(t2buf, "%s [%d]", ipport_to_string(c_rip, c_rport), ntohs(c_mport));
		dprintf(D_ALWAYS, "\t\t\t%s->%s\n", t1buf, t2buf);
		h_lip = ntohl (c_lip);
		h_lport = ntohs (c_lport);
		h_rip = ntohl (c_rip);
		h_rport = ntohs (c_rport);
		// scan through the kernel_forward file
		if ( fseek ( k_fp, 0L, SEEK_SET) ) {
			EXCEPT ("FwdMnger::sync - fseek failed: ");
		}
		bool found = false;
		while ( fgets (k_buf, 100, k_fp) != NULL && !found ) {
			// read an entry from kernel_forward file: ip-addr and port # is in host-byte order
			int rst = sscanf (k_buf, "%s %x %s > %x %s %d %d", p_name, &k_lip, tLport, &k_rip, tRport, &PrCnt, &Pref);
			if (rst == 1) {
				// Header line of kernel file
				continue;
			} else if (rst != 7) {
				EXCEPT ("FwdMnger::sync - sscanf(k_fp) failed: ");
			}
			k_lport = atoi(tLport);
			k_rport = atoi(tRport);
			if (!strcmp (proto, p_name) && h_lip == k_lip && h_lport == k_lport) {
				// (lip, lport) matches
				if (h_rip != k_rip || h_rport != k_rport) {
					// (rip, rport) does not match
					dprintf(D_ALWAYS,  "\t\t\t\tlip and lport match with kernel file but rip or rport dismatch\n");
					continue;
				}
				dprintf(D_ALWAYS, "\t\t\t\tFound the same rule in kernel\n");
				found = true;
				// make internal data structures
				int result = addInternal (&c_lip, &c_lport, c_rip, c_rport, c_mport);
				if ( result == SUCCESS ) {
					// the rule has not been there
					dprintf(D_ALWAYS, "\t\t\t\tAdded to the internal representation\n");
					// copy the entry to the temp file
					fprintf (t_fp, "%x %d %x %d %d\n", c_lip, c_lport, c_rip, c_rport, c_mport);
					added = true;
					dprintf(D_ALWAYS, "\t\t\t\tCopied to the new persistent file\n");
				} else if ( result == ALREADY_SET ) {
					dprintf(D_ALWAYS, "\t\t\t\tThe rule was in the internal representation, as expected!\n");
					// copy the entry to the temp file
					fprintf (t_fp, "%x %d %x %d %d\n", c_lip, c_lport, c_rip, c_rport, c_mport);
					added = true;
					dprintf(D_ALWAYS, "\t\t\t\tCopied to the new persistent file\n");
				} else if (result == RULE_REUSED) {
					// lseek to the start of the temp file
					if (fseek(t_fp, 0L, SEEK_SET) < 0) {
						EXCEPT("fseek failed");
					}
					// find the existing entry
					long position = 0L;
					while ( fgets (t_buf, 100, t_fp) != NULL ) {
						char tLport[10], tRport[10], tMport[10];
						if (sscanf (t_buf, "%x %s %x %s %s", &t_lip, tLport, &t_rip, tRport, tMport) != 5) {
							dprintf(D_ALWAYS, "sscanf failed");
							continue;
						}
						t_lport = atoi(tLport);
						t_rport = atoi(tRport);
						t_mport = atoi(tMport);
						if (t_lip == c_lip && t_lport == c_lport && t_rip == c_rip && t_rport == c_rport) {
							dprintf(D_ALWAYS, "\t\t\t\tFound the record with the same (lip, lport)->(rip, rport)\n");
							dprintf(D_ALWAYS, "\t\t\t\t\tMoving offset from %d to %d\n", ftell(t_fp), position);
							// lseek one line backward
							if (fseek(t_fp, position, SEEK_SET) < 0) {
								EXCEPT("fseek failed");
							}
							break;
						}	
						position = ftell(t_fp);
					}
					// replace the entry with newer one
					fprintf (t_fp, "%x %d %x %d %d\n", c_lip, c_lport, c_rip, c_rport, c_mport);
					changed = added = true;
					dprintf(D_ALWAYS, "\t\t\t\tCopied to the new persistent file\n");
					// lseek to the last to the file
					if (fseek(t_fp, 0, SEEK_END) < 0) {
						EXCEPT("fseek failed");
					}
				} else {
					EXCEPT ("FwdMnger::sync - failed to insert the rule to internal rep.\n");
				}
			}
		}
		if ( !feof (k_fp) && !found ) {
			EXCEPT ("FwdMnger::sync - fgets(k_fp) failed: ");
		}
		if ( !found ) {
			dprintf(D_ALWAYS, "\t\t\t\tNo rule: %s->? found in kernel\n", ipport_to_string(c_lip, c_lport));
			unsigned int dumIp;
			unsigned short dumPort;
			int result;
			result = deleteInternal (c_lip, c_lport, &dumIp, &dumPort);
			if (result == SUCCESS) {
				dprintf(D_ALWAYS, "\t\t\t\tDeleted from the internal representation\n");
			} else if (result != RULE_NOT_FOUND) {
				EXCEPT ("FwdMnger::sync - internal representation deletion failed");
			}
			changed = true;
		}

		// delete the rule from the rules list got at the top of this function
		Rule *cur, *prev;
		prev = NULL;
		cur = rules;
		while (cur) {
			if (cur->lip == c_lip && cur->lport == c_lport) {
				if (prev) {
					prev->next = cur->next;
				} else {
					rules = cur->next;
				}
				Rule * temp = cur;
				cur = cur->next;
				dprintf(D_ALWAYS, "\t\t\t\tDeleted from the rule list\n");
				free (temp);
			} else {
				prev = cur;
				cur = cur->next;
			}
		}
	}
	if ( !feof (c_fp) ) {
		EXCEPT ("FwdMnger::sync - fgets(c_fp) failed: ");
	}
	fclose (c_fp);
	fclose (k_fp);
	fclose (t_fp);

	if (changed) {
		if (added) {
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
	}

	dprintf(D_ALWAYS, "\t\tDeleting leftover internal rules:\n");
	while (rules) {
		if (deleteInternal (rules->lip, rules->lport, &(rules->rip), &(rules->rport)) != SUCCESS) {
			EXCEPT ("FwdMnger::sync - deleteInternal failed");
		}
		dprintf(D_ALWAYS, "\t\t\t%s->%s [%d]\n", ipport_to_string(rules->lip, rules->lport), ipport_to_string(rules->rip, rules->rport), ntohs(rules->mport));
		Rule *temp = rules;
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
	dprintf(D_ALWAYS, "\tAdd: \n");
	int sock;
	sockaddr_in address;
	int result;
	unsigned int t_int;
	unsigned short t_short;

	// update internal data structure
	// let 'addInternal' find a free public (ip, port) pair
	*l_ip = *l_port = 0;
	result = addInternal (l_ip, l_port, r_ip, r_port, m_port);
	if (result == ALREADY_SET) {
		return SUCCESS;
	} else if (result != SUCCESS && result != RULE_REUSED) {
		dprintf(D_ALWAYS, "\t\tInternal Failed: \n");
		return result;
	}
	dprintf(D_ALWAYS, "\t\tInternal: %s->%s [%d]\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port), ntohs(m_port));

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
	dprintf(D_ALWAYS, "\t\tPersistent: %s->%s [%d]\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port), ntohs(m_port));
	if (result == RULE_REUSED) {
		return SUCCESS;
	}

	// now setup the port forwarding rule
	_masq.m_cmd = IP_MASQ_CMD_ADD;
	_pfw.laddr = *l_ip;
	_pfw.lport  = *l_port;
	_pfw.raddr = r_ip;
	_pfw.rport = r_port;
	if (setsockopt(_rawSock, IPPROTO_IP, IP_FW_MASQ_CTL , (void *) &_masq, sizeof (_masq))) {
		dprintf (D_ALWAYS, "FwdMnger::addRule - setsockopt failed\n");
		deleteInternal (*l_ip, *l_port, &t_int, &t_short);
		dprintf(D_ALWAYS, "\t\tKernel Failed\n");
		dprintf(D_ALWAYS, "\t\tDelete Internal: %s->%s\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(t_int, t_short));
		return INTERNAL_ERR;
	}
	dprintf(D_ALWAYS, "\t\tKernel: %s->%s\n", ipport_to_string(*l_ip, *l_port), ipport_to_string(r_ip, r_port));
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


int
FwdMnger::deleteRule (	const unsigned int l_ip,		// in network byte order
						const unsigned short l_port,	// in network byte order
						unsigned int *r_ip,				// in network byte order
						unsigned short *r_port)			// in network byte order
{
	dprintf(D_ALWAYS, "\tdeleteRule:\n");
	// delete internal data structure first
	int result = deleteInternal(l_ip, l_port, r_ip, r_port);
	if (result != SUCCESS) {
		dprintf(D_ALWAYS, "\t\tfrom Internal: result = %d\n", result);
		return result;
	}
	dprintf(D_ALWAYS, "\t\tfrom Internal: %s->%s\n", ipport_to_string(l_ip, l_port), ipport_to_string(*r_ip, *r_port));

	// unset forwarding rule here
	_masq.m_cmd = IP_MASQ_CMD_DEL;
	_pfw.laddr = l_ip;
	_pfw.lport = l_port;
	_pfw.raddr = *r_ip;
	_pfw.rport = *r_port;
	if (setsockopt(_rawSock, IPPROTO_IP, IP_FW_MASQ_CTL , (void *) &_masq, sizeof (_masq))) {
		dprintf (D_ALWAYS, "FwdMnger::deleteRule - failed to setsockopt\n");
		return INTERNAL_ERR;
	}
	dprintf(D_ALWAYS, "\t\tfrom Kernel\n");

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


Rule *
FwdMnger::getRules ()
{
	dprintf(D_ALWAYS, "\t\tRules:\n");

	Rule * rules = NULL;
	local * lptr;
	for (int i=0; i<NAT_MAX_HASH; i++) {
		lptr = _locals[i];
		while ( lptr ) {
			Rule * temp = (Rule *) malloc (sizeof (Rule));
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
