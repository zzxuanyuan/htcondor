/*
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
#include "portfw.h"
*/

#include "condor_common.h"
#include "condor_debug.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_config.h"
#include "getParam.h"
#include "condor_rw.h"
#include "portfw.h"


inline int
sendCmd (const int fd,
		 const char *cmd,
		 const char *proto,
		 const unsigned int ip1,
		 const unsigned short port1,
		 const unsigned int ip2,
		 const unsigned short port2,
		 const unsigned short mport)
{
    int written = 0;
    char msg[80];

    sprintf (msg, "%s %s %d %d %d %d %d\n", cmd, proto, ip1, port1, ip2, port2, mport);
	dprintf (D_NETWORK, "\t\tMASQ sent: (%s %s %s %s %d)\n", cmd, proto,
			ipport_to_string(ip1, port1), ipport_to_string(ip2, port2), ntohs(mport));
	if ( sendLine (fd, msg, sizeof(msg)) != TRUE ) {
		dprintf (D_ALWAYS, "fwdClient.C: failed to send cmd");
		return FALSE;
	}
    return TRUE;
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
	int sock;
	int result;
	char buf[100];
	char rst[10];
	unsigned int rcvdIP;
	unsigned short rcvdPort;
	char strIP[20], strPort[10];
	int fields;

	// Create masq socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock <= 0) {
		dprintf(D_ALWAYS, "FwdClient.C - socket creation failed\n");
		return INTERNAL_ERR;
	}

	// Bind sock
	if (_condor_bind(sock, 0) != TRUE) {
		return INTERNAL_ERR;
	}

	// Connect to masqServer
	if(connect(sock, (struct sockaddr *)&masqServer, sizeof(masqServer))) {
		dprintf(D_ALWAYS, "FwdClient.C - \
							Failed to connect to masqServer\n");
		close (sock);
		return INTERNAL_ERR;
	}

	// Send setFWrule request
	if ( !outIP || !outPort ) {
		result = sendCmd (sock, cmd, proto, inIP, inPort, 0, 0, mport);
	} else {
		result = sendCmd (sock, cmd, proto, inIP, inPort, *outIP, *outPort, mport);
	}
	if ( result != TRUE ) {
		dprintf(D_ALWAYS, "FwdClient.C - could not send req.\n");
		close (sock);
		return INTERNAL_ERR;
	}
	// Get the result from fwdServer
	if ( getLine(sock, buf, sizeof (buf)) != TRUE ) {
		dprintf(D_ALWAYS, "FwdClient.C - could not get reply from fwdServer\n");
		close (sock);
		return INTERNAL_ERR;
	}
	// Unstripe the content read
	fields = sscanf(buf, "%s %s %s", rst, strIP, strPort);
	if (fields != 3) {
		dprintf(D_ALWAYS, "FwdClient.C - scanf failed\n");
		close (sock);
		return INTERNAL_ERR;
	}
	rcvdIP = atoi(strIP);
	rcvdPort = atoi(strPort);
	
	if ( !strcmp (NAK, rst) ) {
		// In case of NAK, rcvdIP is an err code
		dprintf (D_NETWORK, "\t\tMASQ recv: (NAK - )\n");
		close (sock);
		return rcvdIP;
	} else {
		dprintf (D_NETWORK, "\t\tMASQ recv: (%s %s)\n", rst, ipport_to_string(rcvdIP, rcvdPort));
		if (outIP && outPort) {
			*outIP = rcvdIP;
			*outPort = rcvdPort;
		}
		close (sock);
		return SUCCESS;
	}
}
