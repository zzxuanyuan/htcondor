#include <stdio.h>
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
#include <errno.h>

#include <linux/ip_masq.h>

#include "portfw.h"

#define IPPROTO_NONE 65535
#define IP_PORTFW_DEF_PREF 10

char errMsg[100];

struct ip_masq_ctl _masq;
#define _pfw _masq.u.portfw_user

int _rawSock = 0;


static int initialize()
{
	// create raw socket if not created yet
	if ((_rawSock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == -1) {
		sprintf(errMsg, "Raw socket creation failed: %s", strerror(errno));
		return -1;
	}
	// initialize the static part of _masq
	_masq.m_target = IP_MASQ_TARGET_MOD;
	strcpy(_masq.m_tname, "portfw");

	// _pfw is just a part of _masq
	_pfw.pref = IP_PORTFW_DEF_PREF;

	return 0;
}


int
kRuleSet  ( int protocol, int command,
			unsigned int lip, unsigned short lport,
			unsigned int rip, unsigned short rport )
{
	if (!_rawSock) {
		if (initialize() < 0) return -1;
	}
	
	if (protocol == SOCK_STREAM) {
		_pfw.protocol = IPPROTO_TCP;
	} else if (protocol == SOCK_DGRAM) {
		_pfw.protocol = IPPROTO_UDP;
	} else {
		sprintf(errMsg, "invalid protocol passed");
		return -1;
	}

	// now setup the port forwarding rule
	if(command == CMD_ADD ) {
		_masq.m_cmd = IP_MASQ_CMD_ADD;
	} else if (command == CMD_DEL) {
		_masq.m_cmd = IP_MASQ_CMD_DEL;
	} else {
		sprintf(errMsg, "invalid command passed");
		return -1;
	}
	_masq.m_cmd = command;
	_pfw.laddr = lip;
	_pfw.lport  = lport;
	_pfw.raddr = rip;
	_pfw.rport = rport;
	if (setsockopt(_rawSock, IPPROTO_IP, IP_FW_MASQ_CTL , (void *) &_masq, sizeof (_masq))) {
		sprintf(errMsg, "setsockopt failed: %s", strerror(errno));
		return -1;
	}
	return 0;
}


int
kRuleList (int protocol, struct fwRule **rules)
{
	FILE * k_fp = NULL;
	const char *proc_names[] = {
		"/proc/net/ip_masq/portfw",
		"/proc/net/ip_portfw",
		NULL
	};
	const char **proc_name = proc_names;

	char proto[5];
	int PrCnt, Pref;
	unsigned int k_lip, k_rip;
	unsigned short k_lport, k_rport;
	char k_buf[100], p_name[10], tLport[10], tRport[10];
	int rst;
	char t1buf[50], t2buf[50];
	struct fwRule * temp;

	if (protocol == SOCK_STREAM) {
		strcpy(proto, "TCP");
	} else if (protocol == SOCK_DGRAM) {
		strcpy(proto, "UDP");
	}

	// open kernel_forwarding table
	for (;*proc_name;proc_name++) {
		k_fp = fopen (*proc_name, "r");
		if (k_fp) {
			break;
		}
	}
	if (!k_fp) {
		sprintf(errMsg, "Could not open kernel_forwarding file");
		return -1;
	}

	*rules = NULL;
	while ( fgets (k_buf, 100, k_fp) ) {
		// read an entry from kernel_forward file: ip-addr and port # is in host-byte order
		rst = sscanf (k_buf, "%s %x %s > %x %s %d %d", p_name, &k_lip, tLport, &k_rip, tRport, &PrCnt, &Pref);
		if (rst == 1) { // Header line of kernel file
			continue;
		} else if (rst != 7) {
			sprintf(errMsg, "sscanf(k_fp) failed");
			return -1;
		}
		
		// check protocol
		if ( strcmp(proto, p_name) ) {
			continue;
		}

		k_lport = atoi(tLport);
		k_rport = atoi(tRport);

		temp = (struct fwRule *) malloc (sizeof (struct fwRule));
		temp->lip = htonl(k_lip);
		temp->lport = htons(k_lport);
		temp->rip = htonl(k_rip);
		temp->rport = htons(k_rport);
		temp->next = *rules;
		*rules = temp;
	}
	if ( !feof (k_fp) ) {
		sprintf(errMsg, "fgets(k_fp) failed: %s", strerror(errno));
		return -1;
	}

	return 0;
}


char *
strError(void)
{
	return errMsg;
}
