/* 
** Copyright 1993 by Miron Livny, Mike Litzkow, and Emmanuel Ackaouy.
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Emmanuel Ackaouy
**
*/ 


#define _POSIX_SOURCE

#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "condor_network.h"
#include "internet.h"
#include "condor_debug.h"
#include <stdio.h>

#if defined(WIN32)
class SockInitializer
{
public:
	SockInitializer() {
		WORD wVersionRequested = MAKEWORD( 1, 1 );
		WSADATA wsaData;
		int err;

		err = WSAStartup( wVersionRequested, &wsaData );
		if ( err < 0 ) {
			fprintf( stderr, "Can't find usable WinSock DLL!\n" );	
			exit(1);
		}

		if ( LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 ) {
			fprintf( stderr, "Warning: using WinSock version %d.%d, requested 1.1\n",
				LOBYTE( wsaData.wVersion ), HIBYTE( wsaData.wVersion ) );
		}
	}

	~SockInitializer() {
		if (WSACleanup() < 0)
			fprintf(stderr, "WSACleanup() failed, errno = %d\n", 
					WSAGetLastError());
		}
};

static SockInitializer _SockInitializer;
#endif

/*
**	Methods shared by all Socks
*/


int Sock::getportbyserv(
	char	*s
	)
{
	servent		*sp;
	char		*my_prot;

	if (!s) return -1;

	switch(type()){
		case safe_sock:
			my_prot = "udp";
			break;
		case reli_sock:
			my_prot = "tcp";
			break;
		default:
			assert(0);
	}

	if (!(sp = getservbyname(s, my_prot))) return -1;

	return ntohs(sp->s_port);
}



int Sock::assign(
	SOCKET		sockd
	)
{
	int		my_type;

	if (!valid()) return FALSE;
	if (_state != sock_virgin) return FALSE;

	if (sockd != INVALID_SOCKET){
		_sock = sockd;		/* Could we check for correct protocol ? */
		_state = sock_assigned;
		return TRUE;
	}

	switch(type()){
		case safe_sock:
			my_type = SOCK_DGRAM;
			break;
		case reli_sock:
			my_type = SOCK_STREAM;
			break;
		default:
			assert(0);
	}

	if ((_sock = socket(AF_INET, my_type, 0)) < 0) return FALSE;

	_state = sock_assigned;
	return TRUE;
}



int Sock::bind(
	int		port
	)
{
	sockaddr_in		sin;

	if (!valid()) return FALSE;

		/* if stream not assigned to a sock, do it now	*/
	if (_state == sock_virgin) assign();

	if (_state != sock_assigned) return FALSE;

	memset(&sin, 0, sizeof(sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons((u_short)port);

	if (::bind(_sock, (sockaddr *)&sin, sizeof(sockaddr_in)) < 0) {
		int error = WSAGetLastError();
		dprintf( D_ALWAYS, "bind failed: WSAError = %d\n", error );
		return FALSE;
	}

	_state = sock_bound;
	return TRUE;
}


int Sock::setsockopt(SOCKET level, int optname, const char* optval, int optlen)
{
	if(::setsockopt(_sock, level, optname, optval, optlen) < 0)
	{
		return FALSE;
	}
	return TRUE; 
}


int Sock::do_connect(
	char	*host,
	int		port
	)
{
	sockaddr_in		sin;
	hostent			*hostp;
	unsigned long	inaddr;

	if (!valid()) return FALSE;
	if (!host || port < 0) return FALSE;


		/* we bind here so that a sock may be	*/
		/* assigned to the stream if needed		*/
	if (_state == sock_virgin || _state == sock_assigned) bind();

	if (_state != sock_bound) return FALSE;


	memset(&sin, 0, sizeof(sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((u_short)port);

	/* might be in <x.x.x.x:x> notation				*/
	if (host[0] == '<') {
		string_to_sin(host, &sin);
	}
	/* try to get a decimal notation 	 			*/
	else if ((inaddr = inet_addr(host)) != (unsigned int)-1){
		memcpy((char *)&sin.sin_addr, &inaddr, sizeof(inaddr));
	}
	/* if dotted notation fails, try host database	*/
	else{
		if ((hostp = gethostbyname(host)) == (hostent *)0) return FALSE;
		memcpy(&sin.sin_addr, hostp->h_addr, hostp->h_length);
	}

	if (::connect(_sock, (sockaddr *)&sin, sizeof(sockaddr_in)) < 0) {
#if defined(WIN32)
		if (WSAGetLastError() != WSAEALREADY) {
			dprintf( D_ALWAYS, "Can't connect to %s:%d, errno = %d\n",
				host, port, WSAGetLastError() );
			return FALSE;
		}
#else
		if (errno != EINPROGRESS) {
			dprintf( D_ALWAYS, "Can't connect to %s:%d, errno = %d\n",
				host, port, errno );
		}
#endif
	}

	if (_timeout > 0) {
		struct timeval	timer;
		fd_set			writefds;
		int				nfds, nfound;
		timer.tv_sec = _timeout;
		timer.tv_usec = 0;
#if !defined(WIN32) // nfds is ignored on WIN32
		nfds = _sock + 1;
#endif
		FD_ZERO( &writefds );
		FD_SET( _sock, &writefds );

		nfound = select( nfds, 0, &writefds, 0, &timer );

		switch(nfound) {
		case 0:
			return FALSE;
			break;
		case 1:
			break;
		default:
			dprintf( D_ALWAYS, "select returns %d, connect failed\n",
				nfound );
			return FALSE;
			break;
		}
	}

	_state = sock_connect;
	return TRUE;
}


#if !defined(WIN32)
#define closesocket close
#endif

int Sock::close()
{
	if (_state == sock_virgin) return FALSE;

	if (::closesocket(_sock) < 0) return FALSE;

	_state = sock_virgin;
	return TRUE;
}


#if !defined(WIN32)
#define ioctlsocket ioctl
#endif

int Sock::timeout(int sec)
{
	int t = _timeout;

	_timeout = sec;

	if (_timeout == 0) {
		unsigned long mode = 0;	// reset blocking mode
		if (ioctlsocket(_sock, FIONBIO, &mode) < 0)
			return FALSE;
	} else {
		unsigned long mode = 1;	// nonblocking mode
		if (ioctlsocket(_sock, FIONBIO, &mode) < 0)
			return FALSE;
	}

	return t;
}