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


/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
/* (Also allow strings of the form "<hostname>:pppp>")  */
int
string_to_sin( const char *addr, struct sockaddr_in *sin )
{
	char    *cur_byte;
	char    *end_string;
	int 	temp=0;
	char*	addrCpy;
	char*	string;
	char*   colon = 0;
	struct  hostent *hostptr;

	if( ! addr ) {
		return 0;
	}
	addrCpy = strdup(addr);
	string = addrCpy;
	string++;					/* skip the leading '<' */

	/* allow strings of the form "<hostname:pppp>" */
	if( !(colon = strchr(string, ':')) ) {
			// Not a valid sinful string, return failure
		free(addrCpy);
		return 0;
	}
	*colon = '\0';
	if ( is_ipaddr(string,NULL) != TRUE &&	// only call gethostbyname if not numbers
		((hostptr=gethostbyname(string)) != NULL && hostptr->h_addrtype==AF_INET) )
	{
			sin->sin_addr = *(struct in_addr *)(hostptr->h_addr_list[0]);
			string = colon + 1;
	}
	else
	{	
		/* parse the string in the traditional <xxx.yyy.zzz.aaa> form ... */	
		*colon = ':';
		cur_byte = (char *) &(sin->sin_addr);
		for(end_string = string; end_string != 0; ) {
			end_string = strchr(string, '.');
			if (end_string == 0) {
				end_string = strchr(string, ':');
				if (end_string) colon = end_string;
			}
			if (end_string) {
				*end_string = '\0';
				*cur_byte = atoi(string);
				cur_byte++;
				string = end_string + 1;
				*end_string = '.';
			}
		}
	}
	
	string[strlen(string) - 1] = '\0'; /* Chop off the trailing '>' */
	sin->sin_port = htons((short)atoi(string));
	sin->sin_family = AF_INET;
	string[temp-1] = '>';
	string[temp] = '\0';
	*colon = ':';
	free(addrCpy);
	return 1;
}


char *
sin_to_string(const struct sockaddr_in *sin)
{
	int             i;
	static  char    buf[24];
	char    tmp_buf[10];
	char    *cur_byte;
	unsigned char   this_byte;

	buf[0] = '\0';
	if (!sin) return buf;
	buf[0] = '<';
	buf[1] = '\0';
	cur_byte = (char *) &(sin->sin_addr);
	for (i = 0; i < sizeof(sin->sin_addr); i++) {
		this_byte = (unsigned char) *cur_byte;
		sprintf(tmp_buf, "%u.", this_byte);
		cur_byte++;
		strcat(buf, tmp_buf);
	}
	buf[strlen(buf) - 1] = ':';
	sprintf(tmp_buf, "%d>", ntohs(sin->sin_port));
	strcat(buf, tmp_buf);
	return buf;
}

char *
sock_to_string(SOCKET sockd)
{
	struct sockaddr_in	addr;
	int			addr_len;
	static char *mynull = "\0";

	addr_len = sizeof(addr);

	if (getsockname(sockd, (struct sockaddr *)&addr, &addr_len) < 0) 
		return mynull;

	return ( sin_to_string( &addr ) );
}

char *
sin_to_hostname( const struct sockaddr_in *from, char ***aliases)
{
    struct hostent  *hp;
#ifndef WIN32
	struct hostent  *gethostbyaddr();
#endif

	if( !from ) {
		// make certain from is not NULL before derefencing it
		return NULL;
	}

    if( (hp=gethostbyaddr((char *)&from->sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
		// could not find a name for this address
        return NULL;
    } else {
		// CAREFULL: we are returning a staic buffer from gethostbyaddr.
		// The caller had better use the result immediately or copy it.
		// Also note this is not thread safe.  (as are lots of things in internet.c).
		if( aliases ) {
			*aliases = hp->h_aliases;
		}
		return hp->h_name;
    }
}


void
display_from( from )
struct sockaddr_in  *from;
{
    struct hostent  *hp;
#ifndef WIN32
	struct hostent  *gethostbyaddr();
#endif

	if( !from ) {
		dprintf( D_ALWAYS, "from NULL source\n" );
		return;
	}

    if( (hp=gethostbyaddr((char *)&from->sin_addr,
                sizeof(struct in_addr), AF_INET)) == NULL ) {
        dprintf( D_ALWAYS, "from (%s), port %d\n",
            inet_ntoa(from->sin_addr), ntohs(from->sin_port) );
    } else {
        dprintf( D_ALWAYS, "from %s, port %d\n",
                                        hp->h_name, ntohs(from->sin_port) );
    }
}

char *
calc_subnet_name(char* host)
{
	char			subnetname[MAXHOSTNAMELEN];
	char			*subnet_ptr;
	char			*host_addr_string;
	int				subnet_length;
	struct			in_addr	in;
	unsigned int	host_ordered_addr;
	unsigned int		net_ordered_addr;

	if ( !(host_ordered_addr = my_ip_addr()) ) {
		return strdup("");
	}

	net_ordered_addr = htonl(host_ordered_addr);
	memcpy((char *) &in,(char *)&net_ordered_addr, sizeof(host_ordered_addr));
	host_addr_string = inet_ntoa( in );
	if( host_addr_string ) {
		subnet_ptr = (char *) strrchr(host_addr_string, '.');
		if(subnet_ptr == NULL) {
			return strdup("");
		}
		subnet_length = subnet_ptr - host_addr_string;
		strncpy(subnetname, host_addr_string, subnet_length);
		subnetname[subnet_length] = '\0';
		return (strdup(subnetname));
	}
	return strdup("");
}

int
same_host(const char *h1, const char *h2)
{
	struct hostent *he1, *he2;
	char cn1[MAXHOSTNAMELEN];

	if (strcmp(h1, h2) == MATCH) {
		return TRUE;
	}
	
	if ((he1 = gethostbyname(h1)) == NULL) {
		return -1;
	}

	// stash h_name before our next call to gethostbyname
	strncpy(cn1, he1->h_name, MAXHOSTNAMELEN);

	if ((he2 = gethostbyname(h2)) == NULL) {
		return -1;
	}

	return (strcmp(cn1, he2->h_name) == MATCH);
}


/*
  Return TRUE if the given domain contains the given hostname, FALSE
  if not.  Origionally taken from condor_starter.V5/starter_common.C. 
  Moved here on 1/18/02 by Derek Wright.
*/
int
host_in_domain( const char *host, const char *domain )
{
	const char	*ptr;

	for( ptr=host; *ptr; ptr++ ) {
		if( strcmp(ptr,domain) == MATCH ) {
			return TRUE;
		}
	}
	return FALSE;
}


/*
  is_ipaddr() returns TRUE if buf is an ascii IP address (like
  "144.11.11.11") and false if not (like "cs.wisc.edu").  Allow
  wildcard "*".  If we return TRUE, and we were passed in a non-NULL 
  sin_addr, it's filled in with the integer version of the ip address. 
*/
int
is_ipaddr(const char *inbuf, struct in_addr *sin_addr)
{
	int len;
	char buf[17];
	int part = 0;
	int i,j,x;
	char save_char;
	unsigned char *cur_byte = NULL;
	if( sin_addr ) {
		cur_byte = (unsigned char *) sin_addr;
	}

	len = strlen(inbuf);
	if ( len < 3 || len > 16 ) 
		return FALSE;	// shortest possible IP addr is "1.*" - 3 chars
	
	// copy to our local buf
	strncpy( buf, inbuf, 17 );

	// on IP addresses, wildcards only permitted at the end, 
	// i.e. 144.92.* , _not_ *.92.11
	if ( buf[0] == '*' ) 
		return FALSE;

	// strip off any trailing wild card or '.'
	if ( buf[len-1] == '*' || buf[len-1] == '.' ) {
		if ( buf[len-2] == '.' )
			buf[len-2] = '\0';
		else
			buf[len-1] = '\0';
	}

	// Make certain we have a valid IP address, and count the parts,
	// and fill in sin_addr
	i = 0;
	for(;;) {
		
		j = i;
		while (buf[i] >= '0' && buf[i] <= '9') i++;
		// make certain a number was here
		if ( i == j )
			return FALSE;	
		// now that we know there was a number, check it is between 0 & 255
		save_char = buf[i];
		buf[i] = '\0';
		x = atoi( &(buf[j]) );
		if( x < 0 || x > 255 ) {
			return FALSE;
		}
		if( cur_byte ) {
			*cur_byte = x;	/* save into sin_addr */
			cur_byte++;
		}
		buf[i] = save_char;

		part++;
		
		if ( buf[i] == '\0' ) 
			break;
		
		if ( buf[i] == '.' )
			i++;
		else
			return FALSE;

		if ( part >= 4 )
			return FALSE;
	}
	
	if( cur_byte ) {
		for (i=0; i < 4 - part; i++) {
			*cur_byte = (unsigned char) 255;
			cur_byte++;
		}
	}
	return TRUE;
}


int
is_valid_sinful( const char *sinful )
{
	char* tmp;
	if( !sinful ) return FALSE;
	if( !(sinful[0] == '<') ) return FALSE;
	if( !(tmp = strchr(sinful, ':')) ) return FALSE;
	if( !(tmp = strrchr(sinful, '>')) ) return FALSE;
	return TRUE;
}


int
string_to_port( const char* addr )
{
	char *sinful, *tmp;
	int port = 0;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strrchr(sinful, '>')) ) {
		*tmp = '\0';
		tmp = strchr( sinful, ':' );
		if( tmp && tmp[1] ) {
			port = atoi( &tmp[1] );
		} 
	}
	free( sinful );
	return port;
}


unsigned int
string_to_ip( const char* addr )
{
	char *sinful, *tmp;
	unsigned int ip = 0;
	struct in_addr sin_addr;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return 0;
	}

	sinful = strdup( addr );
	if( (tmp = strchr(sinful, ':')) ) {
		*tmp = '\0';
		if( is_ipaddr(&sinful[1], &sin_addr) ) {
			ip = sin_addr.s_addr;
		}
	} else {
		EXCEPT( "is_valid_sinful(\"%s\") is true, but can't find ':'" );
	}
	free( sinful );
	return ip;
}


char*
string_to_ipstr( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
	char sinful[MAXHOSTNAMELEN];

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

	strncpy( sinful, addr, MAXHOSTNAMELEN );
	tmp = strchr( sinful, ':' );
	if( tmp ) {
		*tmp = '\0';
	} else {
		return NULL;
	}
	if( is_ipaddr(&sinful[1], NULL) ) {
		strncpy( result, &sinful[1], MAXHOSTNAMELEN );
		return result;
	}
	return NULL;
}


char*
string_to_hostname( const char* addr ) 
{
	char *tmp;
	static char result[MAXHOSTNAMELEN];
    struct sockaddr_in sin;

	if( ! (addr && is_valid_sinful(addr)) ) {
		return NULL;
	}

    string_to_sin( addr, &sin );
	if( (tmp = sin_to_hostname(&sin, NULL)) ) {
		strncpy( result, tmp, MAXHOSTNAMELEN );
	} else {
		return NULL;
	}
	return result;
}


/* Bind the given fd to the correct local interface. */
int
_condor_local_bind( int fd )
{
	/* Note: this function is completely WinNT screwed.  However,
	 * only non-Cedar components call this function (ckpt-server,
	 * old shadow) --- and these components are not destined for NT
	 * anyhow.  So on NT, just pass back success so log file is not
	 * full of scary looking error messages.
	 *
	 * This function should go away when everything uses CEDAR.
	 */
#ifndef WIN32
	int lowPort, highPort;
	if ( get_port_range(&lowPort, &highPort) == TRUE ) {
		if ( bindWithin(fd, lowPort, highPort) == TRUE )
            return TRUE;
        else
			return FALSE;
	} else {
		struct sockaddr_in sin;
		memset( (char *)&sin, 0, sizeof(sin) );
		sin.sin_family = AF_INET;
		sin.sin_port = 0;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		if( bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0 ) {
			dprintf( D_ALWAYS, "ERROR: bind(%s:%d) failed, errno: %d\n",
					 inet_ntoa(sin.sin_addr), sin.sin_port, errno );
			return FALSE;
		}
	}
#endif  /* of ifndef WIN32 */
	return TRUE;
}


int bindWithin(const int fd, const int low_port, const int high_port)
{
	int start_trial, this_trial;
	int pid, range;
	int nextBindPort;

	// Use hash function with pid to get the starting point
    pid = (int) getpid();
    range = high_port - low_port + 1;
    // this line must be changed to use the hash function of condor
    start_trial = low_port + (pid * 173/*some prime number*/ % range);

    this_trial = start_trial;
	do {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sin.sin_port = htons((u_short)this_trial++);

		if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == 0) { // success
			dprintf(D_NETWORK, "_condor_local_bind - bound to %d...\n", this_trial-1);
			return TRUE;
		} else {
            dprintf(D_NETWORK, "_condor_local_bind - failed to bind: %s\n", strerror(errno));
        }
		if ( this_trial > high_port )
			this_trial = low_port;
    } while(this_trial != start_trial);

	dprintf(D_ALWAYS, "_condor_local_bind::bindWithin - failed to bind any port within (%d ~ %d)\n",
	        low_port, high_port);

	return FALSE;
}

// @args:   ip - ip address in host byte order
int
is_priv_net(uint32_t ip)
{
	return ((ip & 0xFF000000) == 0x0A000000 ||      // 10/8
			(ip & 0xFFF00000) == 0xAC100000 ||      // 172.16/12
			(ip & 0xFFFF0000) == 0xC0A80000);       // 192.168/16
}


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
