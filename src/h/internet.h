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

#ifndef INTERNET_H
#define INTERNET_H


#if !defined(SOCKET) && !defined(WIN32)
#define SOCKET int
#endif 

/* maximum length of a machine name */
#define  MAXHOSTLEN     1024	

#if defined(__cplusplus)
extern "C" {
#endif

/* Convert a string of the form "<xx.xx.xx.xx:pppp>" to a sockaddr_in  TCP */
int string_to_sin(const char *addr, struct sockaddr_in *sin);

char *sin_to_string(const struct sockaddr_in *sin);

/* Extract the port from a string of the form "<xx.xx.xx.xx:pppp>" */
int string_to_port( const char* addr );

/* Extract the ip_addr from a string of the form "<xx.xx.xx.xx:pppp>"
   and convert it to the unsigned int version from the ASCII version */
unsigned int string_to_ip( const char* addr );

/* Return "<xx.xx.xx.xx:pppp>" from ip addr and port # in network byte order */
char * ipport_to_string(const unsigned int ip, const unsigned short port);

/* Return hostname from ip addr in network byte order */
char * ip_to_name (unsigned int ip);

/* Extract the ip_addr from a string of the form "<xx.xx.xx.xx:pppp>"
   and return a pointer to the ASCII version */
char* string_to_ipstr( const char* addr );

/* Convert a sinful string into a hostname. */
char* string_to_hostname( const char* addr );

char *sock_to_string(SOCKET sockd);

/* Return port # in network byte order for the given socket */
unsigned short sock_to_port (int sock);

/* Return the real hostname of a machine given a sin; return NULL if it cannot
 * be found or error.  Also return aliases. */
char *sin_to_hostname(const struct sockaddr_in *sin, char ***aliases);

void
display_from( struct sockaddr_in *from );

/* Returns 1 if h1 and h2 are both hostnames which refer to the same
   host, 0 if they don't, and -1 on error. */
int same_host(const char *h1, const char *h2);

char* calc_subnet_name( char* host );

int is_ipaddr(const char *inbuf, struct in_addr *sin_addr);

int is_valid_sinful( const char *sinful );

/* Binds the given fd to any port on the correct local interface for
   this machine.   Returns 1 if successful, 0 on error. */
int _condor_local_bind( int fd );
int _condor_bind (int fd, unsigned short port);
int _condor_connect(int sock, unsigned int ip, unsigned short port);
int bindWithin(const int sock, const unsigned short low_port, const unsigned short high_port);

#if defined(__cplusplus)
}
#endif

#endif /* INTERNET_H */
