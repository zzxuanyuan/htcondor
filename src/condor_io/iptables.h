#ifndef _IPTABLES_USER_H
#define _IPTABLES_USER_H

#include "iptables_common.h"
#include "libiptc.h"

/* Include file for additions: new matches and targets. */
struct iptables_match
{
	struct iptables_match *next;

	ipt_chainlabel name;

	const char *version;

	/* Size of match data. */
	size_t size;

	/* Size of match data relevent for userspace comparison purposes */
	size_t userspacesize;

	unsigned int option_offset;
	struct ipt_entry_match *m;
	unsigned int mflags;
	unsigned int used;
};

struct iptables_target
{
	struct iptables_target *next;

	ipt_chainlabel name;

	const char *version;

	/* Size of target data. */
	size_t size;

	/* Size of target data relevent for userspace comparison purposes */
	size_t userspacesize;

	unsigned int option_offset;
	struct ipt_entry_target *t;
	unsigned int tflags;
	unsigned int used;
};

#endif /*_IPTABLES_USER_H*/
