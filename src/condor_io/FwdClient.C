#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "getParam.h"
#include "condor_rw.h"
#include "portfw.h"


inline bool
sendCmd (const int fd,
		 const char *cmd,
		 const char *proto,
		 const int ip,
		 const short port,
		 const short mport)
{
    int written = 0;
    char msg[50];

    sprintf (msg, "%s %s %d %d %d\n", cmd, proto, ip, port, mport);
	if ( !sendLine (fd, msg, sizeof(msg)) ) {
		dprintf (D_ALWAYS, "fwdClient.C: failed to send cmd");
		return false;
	}
    return true;
}


int
setFWrule ( struct sockaddr_in masqServer,
			char *cmd,
			char *proto,
			unsigned int inIP,
			unsigned short inPort,
			unsigned int * outIP,
			unsigned short * outPort,
			unsigned short mport)
{
	struct sockaddr_in sockAddr;
	unsigned int addrLen = sizeof(sockAddr);

	// Create masq socket
	int masqSock = socket(AF_INET, SOCK_STREAM, 0);
	if (masqSock <= 0) {
		dprintf(D_ALWAYS, "fwdClient.C - socket creation failed\n");
		return FALSE;
	}

	// Bind masqSock
	int lowPort, highPort;
	if ( get_port_range(&lowPort, &highPort) == TRUE ) {
		if ( bindWithin(masqSock, lowPort, highPort) != TRUE ) {
			close (masqSock);
			return FALSE;
		}
	} else {
		memset(&sockAddr, 0, sizeof(sockAddr));
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = htonl(my_ip_addr());
		sockAddr.sin_port = htons(0);
		if(::bind(masqSock, (sockaddr *)&sockAddr, addrLen)) {
			dprintf(D_ALWAYS, "fwdClient.C - \
							Failed to bind masqSock to a local port\n");
			close (masqSock);
			return FALSE;
		}
	}

	// Connect to masqServer
	if(::connect(masqSock, (sockaddr *)&masqServer, addrLen)) {
		dprintf(D_ALWAYS, "fwdClient.C - \
							Failed to connect to masqServer\n");
		close (masqSock);
		return FALSE;
	}

	// Send setFWrule request
	if ( !sendCmd (masqSock, cmd, proto, inIP, inPort, mport) ) {
		dprintf(D_ALWAYS, "fwdClient.C - could not send req.\n");
		close (masqSock);
		return FALSE;
	}
	// Get the result from fwdServer
	char buf[100];
	if ( !getLine(masqSock, buf, sizeof (buf)) ) {
		dprintf(D_ALWAYS, "fwdClient.C - could not get reply from fwdServer\n");
		close (masqSock);
		return FALSE;
	}
	// Unstripe the content read
	char rst[10];
	char lineBuf[100];
	int rcvdIP, rcvdPort;
	int fields = sscanf(lineBuf, "%s %d %d", rst, &rcvdIP, &rcvdPort);
	if (fields != 3) {
		dprintf(D_ALWAYS, "fwdClient.C - scanf failed\n");
		close (masqSock);
		return FALSE;
	}
	
	int result = TRUE;
	if ( !strcmp (NAK, rst) ) {
		*outIP = *outPort = 0;
		if ( strcmp (QUERY, cmd) && rcvdIP != NOT_FOUND ) {
			result = FALSE;
		}
	} else {
		*outIP = rcvdIP;
		*outPort = rcvdPort;
	}

	close (masqSock);
	return result;
}
