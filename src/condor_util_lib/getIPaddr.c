/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.
 * No use of the CONDOR Software Program Source Code is authorized
 * without the express consent of the CONDOR Team.  For more information
 * contact: CONDOR Team, Attention: Professor Miron Livny,
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685,
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure
 * by the U.S. Government is subject to restrictions as set forth in
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison,
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*
** These are functions for generating internet addresses
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_config.h"

#include <sys/ioctl.h>
#include <net/if.h>

/*
#include <arpa/inet.h>
#include <netinet/tcp.h>
*/

/* Get an ip address of this host, preferably public one */
// @args:	ipaddr - ip address found will be returned via this arg
//					 in network byte order
// @return:	-1, if failed to find a valid ip address
//			0, if a public ip address found
//			1, if could not find a public ip but a private ip found
int
_getIPaddr(uint32_t *ipaddr)
{
	int sockfd, len, lastlen, flags, myflags;
	char *ptr, *buf, lastname[IFNAMSIZ], *cptr;
	struct ifconf ifc;
	struct ifreq *ifr, ifrcopy;
	struct sockaddr_in *sinptr;
	int result = -1;

	// Create a socket to do ioctl
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		dprintf(D_ALWAYS, "_getIPaddr: socket creation - %s\n", strerror(errno));
		return result;
	}

	// Get the list of interfaces in the system
	lastlen = 0;
	len = 100 * sizeof(struct ifreq); // initial buffer size guess
	while(1) {
		if ((buf = (char *)malloc(len)) == NULL) {
			dprintf(D_ALWAYS, "_getIPaddr: Out of Memory\n");
			return result;
		}
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		// The problem of ioctl is that some implementations of OS does not
		// return an error when buf is not large enough to hold the result.
		// Instead the result is truncated and success is returned. Hence
		// we try multiple ioctl with increasing buf size, until we get the
		// same length returned from two consecutive ioctl calls
		if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
			if (errno != EINVAL || lastlen != 0) {
				dprintf(D_ALWAYS, "_getIPaddr: ioctl failed - %s\n", strerror(errno));
			}
		} else {
			if (ifc.ifc_len == lastlen) { // success
				break;
			}
			lastlen = ifc.ifc_len;
		}
		len += 10 * sizeof(struct ifreq);
		free(buf);
	}

	// Skip "lo" and its alias interfaces. Also skip interfaces not up.
	// Keep getting attributes of interfaces until an interface with
	// a public ip address is found.
	lastname[0] = 0;
	for(ptr = buf; ptr < buf + ifc.ifc_len; ) {
		ifr = (struct ifreq *) ptr;
		// Figure out the size of ifreq structure
		// Difficulty is that some systems provide length field but others not
#ifdef HAVE_SOCKADDR_SA_LEN
		len = max(sizeof(struct sockaddr), ifr->ifr_addr.sa_len);
#else
		switch(ifr->ifr_addr.sa_family) {
#ifdef IPV6
			case AF_INET6:
				len = sizeof(struct sockaddr_in6);
				break;
#endif
			case AF_INET:
			default:
				len = sizeof(struct sockaddr_in);
				break;
		}
#endif
		// Move ptr to the next interface
		ptr += sizeof(ifr->ifr_name) + len;

		// For now, ignore non ipv4 addresses
		if (ifr->ifr_addr.sa_family != AF_INET) {
			continue;
		}

		// In solaris, alias ip has interface name associated with it
		// in the form of "primary_interface_name:number", while in BSD
		// alias ip's share the same interface name. Hence we must check
		// only the primary interface name part
		if ((cptr = strchr(ifr->ifr_name, ':')) != NULL) {
			*cptr = 0;
		}
		// Check if the interface is loopback
		if (!strcmp(ifr->ifr_name, "lo")) {
			continue;
		}
		// Check if the interface is the same as the previous one
		if (!strncmp(ifr->ifr_name, lastname, IFNAMSIZ)) {
			continue;
		}
		memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

		// Now, get the attributes of the current interface
		ifrcopy = *ifr;
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifrcopy) < 0) {
			dprintf(D_ALWAYS, "_getIPaddr: ioctl failed\n");
		}

		// If not up, ignore
		if ( !(ifrcopy.ifr_flags & IFF_UP) ) {
			continue;
		}

		// Check if ip address is in private range
		sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
		*ipaddr = sinptr->sin_addr.s_addr;
		if (is_priv_net(ntohl(*ipaddr))) {
			result = 1;
			continue;
		}
		return 0;
	}
	return result;
}
