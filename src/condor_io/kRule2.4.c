#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <ctype.h>
#include <stdarg.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>

#include "portfw.h"

int fwerrno = 0;
const char errMsg[100];

struct ipt_natinfo
{
	struct ipt_entry_target t;
	struct ip_nat_multi_range mr;
};


#define TABLE "nat"
#define CHAIN "PREROUTING"
#define TCP_DST_PORTS 0x02
#define UDP_DST_PORTS 0x02

// functions provided by extensions we use here
struct iptables_target * DNAT_init(void);
struct iptables_match * TCP_init(void);
struct iptables_match * UDP_init(void);


static void *
fw_malloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL) {
		printf("iptables: malloc failed: %s\n", strerror(errno));
		exit(1);
	}
	return p;
}


static void *
fw_calloc(size_t count, size_t size)
{
	void *p;

	if ((p = calloc(count, size)) == NULL) {
		printf("fw_calloc: calloc failed: %s\n", strerror(errno));
		exit(1);
	}
	return p;
}


static struct ipt_entry *
generate_entry( const struct ipt_entry *fw,
		struct iptables_match *matches,
		struct ipt_entry_target *target)
{
	unsigned int size;
	struct iptables_match *m;
	struct ipt_entry *e;

	size = sizeof(struct ipt_entry);
	for (m = matches; m; m = m->next) {
		if (!m->used)
			continue;

		size += m->m->u.match_size;
	}

	e = fw_malloc(size + target->u.target_size);
	*e = *fw;
	e->target_offset = size;
	e->next_offset = size + target->u.target_size;

	size = 0;
	for (m = matches; m; m = m->next) {
		if (!m->used)
			continue;

		memcpy(e->elems + size, m->m, m->m->u.match_size);
		size += m->m->u.match_size;
	}
	memcpy(e->elems + size, target, target->u.target_size);

	return e;
}


static unsigned char *
make_delete_mask(struct ipt_entry *fw, struct iptables_match *m, struct iptables_target *t)
{
	/* Establish mask for comparison */
	unsigned int size;
	unsigned char *mask, *mptr;

	size = sizeof(struct ipt_entry);
	size += IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;

	mask = fw_calloc(1, size
			+ IPT_ALIGN(sizeof(struct ipt_entry_target))
			+ t->size);

	memset(mask, 0xFF, sizeof(struct ipt_entry));
	mptr = mask + sizeof(struct ipt_entry);

	memset(mptr, 0xFF,
			IPT_ALIGN(sizeof(struct ipt_entry_match))
			+ m->userspacesize);
		mptr += IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;

	memset(mptr, 0xFF,
			IPT_ALIGN(sizeof(struct ipt_entry_target))
			+ t->userspacesize);

	return mask;
}


// @return: 0 - success
//			-1 - failure
int
kRuleSet  ( int protocol,			// SOCK_STREAM or SOCK_DGRAM
			int command,			// CMD_ADD or CMD_DEL
			unsigned int lip,		// in network byte order
			unsigned short lport,
			unsigned int rip,
			unsigned short rport )
{
	iptc_handle_t handle;
	struct iptables_target *target;
	struct iptables_match *match;
	struct ipt_entry fw, *e = NULL;
	struct ipt_natinfo info;
	struct ip_nat_range range;
	size_t size;
	struct ipt_tcp *tcpinfo;
	struct ipt_udp *udpinfo;
	unsigned char *mask;
	struct iptables_target dnat = {	
		NULL,
		"DNAT",
		NETFILTER_VERSION,
		IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
		IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	};
	struct iptables_match tcp = {
		NULL,
		"tcp",
		NETFILTER_VERSION,
		IPT_ALIGN(sizeof(struct ipt_tcp)),
		IPT_ALIGN(sizeof(struct ipt_tcp)),
	};
	struct iptables_match udp = {
		NULL,
		"udp",
		NETFILTER_VERSION,
		IPT_ALIGN(sizeof(struct ipt_udp)),
		IPT_ALIGN(sizeof(struct ipt_udp)),
	};

	// read the snapshot of the table
	handle = iptc_init(TABLE);
	if (!handle) {
		sprintf(errMsg, "can't read the snapshot of iptables");
		fwerrno = 1;
		return -1;
	}

	// initialize invariant part of firewall rule
	memset(&fw, 0, sizeof(fw));
	fw.nfcache |= NFC_IP_PROTO;
	fw.nfcache |= NFC_IP_DST;
	fw.nfcache |= NFC_IP_DST_PT;
	fw.nfcache |= NFC_UNKNOWN;
	fw.ip.dmsk.s_addr = 0xFFFFFFFF;

	if(protocol == SOCK_STREAM) {
		fw.ip.proto = IPPROTO_TCP;
	} else if(protocol == SOCK_DGRAM) {
		fw.ip.proto = IPPROTO_UDP;
	} else {
		sprintf(errMsg, "invalid protocol passed");
		fwerrno = 1;
		return -1;
	}

	/* setup target */
	target = &dnat;
	size = IPT_ALIGN(sizeof(struct ipt_entry_target)) + target->size;
	memset(&info, 0, sizeof(info));
	target->t = (void *)&info;
	target->t->u.target_size = size;
	strcpy(target->t->u.user.name, "DNAT");

	// --to rip:rport
	target->tflags = 1;
	memset(&range, 0, sizeof(range));
	range.flags |= IP_NAT_RANGE_MAP_IPS;
	range.max_ip = range.min_ip = rip;
	range.flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
	range.min.tcp.port = range.max.tcp.port = rport;
	size = IPT_ALIGN(sizeof(info) + info.mr.rangesize * sizeof(range));
	info.t.u.target_size = size;
	info.mr.range[info.mr.rangesize] = range;
	info.mr.rangesize++;

	/* setup matches */
	if(protocol == SOCK_STREAM) {
		match = &tcp;
		size = IPT_ALIGN(sizeof(struct ipt_entry_match)) + match->size;
		match->m = fw_calloc(1, size);
		match->m->u.match_size = size;
		strcpy(match->m->u.user.name, match->name);
		tcpinfo = (struct ipt_tcp *)match->m->data;
		tcpinfo->spts[1] = tcpinfo->dpts[1] = 0xFFFF;
		// --dport lport
		tcpinfo->dpts[0] = tcpinfo->dpts[1] = ntohs(lport);
		match->mflags |= TCP_DST_PORTS;
		match->used = 1;
	} else if(protocol == SOCK_DGRAM) {
		match = &udp;
		size = IPT_ALIGN(sizeof(struct ipt_entry_match)) + match->size;
		match->m = fw_calloc(1, size);
		match->m->u.match_size = size;
		strcpy(match->m->u.user.name, match->name);
		udpinfo = (struct ipt_udp *)match->m->data;
		udpinfo->spts[1] = udpinfo->dpts[1] = 0xFFFF;
		// --dport lport
		udpinfo->dpts[0] = udpinfo->dpts[1] = ntohs(lport);
		match->used = 1;
		match->mflags |= UDP_DST_PORTS;
	}

	e = generate_entry(&fw, match, target->t);

	switch(command) {
		case CMD_ADD:
			e->ip.src.s_addr = 0;
			e->ip.dst.s_addr = lip;
			if(!iptc_append_entry(CHAIN, e, &handle)) {
				printf("iptc_append_entry failed\n");
				return -1;
			}
			break;
		case CMD_DEL:
			mask = make_delete_mask(e, match, target);
			e->ip.src.s_addr = 0;
			e->ip.dst.s_addr = lip;
			if (!iptc_delete_entry(CHAIN, e, mask, &handle)) {
				printf("iptc_delete_entry failed\n");
				return -1;
			}
			break;
		default:
			//EXCEPT("kRuleSet - invalid command passed: %d\n", command);
			printf("kRuleSet - invalid command passed: %d\n", command);
			exit(1);
	}

	if (!iptc_commit(&handle)) {
		printf("iptc_commit failed\n");
		return -1;
	}
	return 0;
}

void
kRuleList ( int protocol )
{
	char *cName;
	const struct ipt_entry *entry;
	const struct ipt_entry_target *t;
	struct ipt_natinfo *info;
	struct ipt_entry_match *m;
	struct ipt_tcp *tcp;
	struct ipt_udp *udp;
	int proto;
	struct ip_nat_range range;
	unsigned int lip, rip;
	unsigned short lport, rport;

	//Rule rules = NULL, irule;

	if (protocol == SOCK_STREAM) {
		proto = IPPROTO_TCP;
	} else if (protocol == SOCK_DGRAM) {
		proto = IPPROTO_UDP;
	}

	//Rule * rules = NULL;

	if (protocol == SOCK_STREAM) {
		strcpy(proto, "TCP");
	} else if (protocol == SOCK_DGRAM) {
		strcpy(proto, "UDP");
	}

	// read the snapshot of the table
	handle = iptc_init(TABLE);
	if (!handle) {
		//EXCEPT("kRuleSet - can't read the snapshot of iptables\n");
		printf("kRuleSet - can't read the snapshot of iptables\n");
		exit(1);
	}

	for (cName = iptc_first_chain(handle); cName; cName = iptc_next_chain(handle)) {

		if (strcmp(cName, CHAIN) != 0)
			continue;

		entry = iptc_first_rule(this, handle);
		while (entry) {
			if (entry->ip.proto != proto) {
				continue;
			}

			// get the match of the rule: simple because we know that our rules have only one match
			m = (void *) (entry + sizeof(struct ipt_entry));
			if (proto == IPPROTO_TCP) {
				tcp = (void *) m->data;
				if (tcp->spts[0] != 0 || tcp->spts[1] != 0xFFFF || tcp->dpts[0] != tcp->dpts[1]) {
					continue;
				}
				// found a rule we want
				lport = htons(tcp->dpts[0]);
			} else {
				udp = (void *) m->data;
				if (udp->spts[0] != 0 || udp->spts[1] != 0xFFFF || udp->dpts[0] != udp->dpts[1]) {
					continue;
				}
				// found a rule we want
				lport = htons(udp->dpts[0]);
			}

			// get the target of the rule
			t = ipt_get_target((struct ipt_entry *)entry);
			info = (void *)t;
			range = info->mr.range[0];
			if (range.min_ip != range.max_ip || range.min.all != range.max.all) {
				continue;
			}
			rip = range.min;
			rport = range.min.all;

			lip = entry->ip.dst.s_addr;
			// Now we got every information to make our rule
			/*
			irule = (Rule *) malloc(sizeof(Rule));
			irule->lip = entry->ip.dst.s_addr;
			irule->lport = lport;
			irule->rip = rip;
			irule->rport = rport;
			irule->next = rules;
			rules = irule;
			*/

			entry = iptc_next_rule(entry, handle);
		}
	}

	//return rules;
}

char *strError(void)
{
	return errMsg;
}
