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

char *errMsg[] = {
	"success",
	"invalid command",
	"invalid protocol",
	"invalid port number",
	"no more free port remained",
	"(lip, lport) is NOT in _locals[]",
	"internal error" };

inline bool
sendCmd (const int fd,
		 const char *cmd,
		 const char *proto,
		 const unsigned int ip,
		 const unsigned short port,
		 const unsigned short mport)
{
    int written = 0;
    char msg[50];

    sprintf (msg, "%s %s %d %d %d\n", cmd, proto, ip, port, mport);
#ifdef MYDEBUG
	cout << "MASQ sent: " << cmd << " " << proto << " " << ipport_to_string(ip, port) << " " << ntohs(mport) << endl;
#endif
	dprintf (D_NETWORK, "MASQ sent: (%s %s %s %d)\n", cmd, proto, ipport_to_string(ip, port), ntohs(mport));
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
	// Create masq socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock <= 0) {
		dprintf(D_ALWAYS, "FwdClient.C - socket creation failed\n");
		return INTERNAL_ERR;
	}

	// Bind sock
	if (_condor_bind(sock, 0) != TRUE) {
		return INTERNAL_ERR;
	}

	// Connect to masqServer
	if(::connect(sock, (sockaddr *)&masqServer, sizeof(masqServer))) {
		dprintf(D_ALWAYS, "FwdClient.C - \
							Failed to connect to masqServer\n");
		close (sock);
		return INTERNAL_ERR;
	}

	// Send setFWrule request
	if ( !sendCmd (sock, cmd, proto, inIP, inPort, mport) ) {
		dprintf(D_ALWAYS, "FwdClient.C - could not send req.\n");
		close (sock);
		return INTERNAL_ERR;
	}
	// Get the result from fwdServer
	char buf[100];
	if ( !getLine(sock, buf, sizeof (buf)) ) {
		dprintf(D_ALWAYS, "FwdClient.C - could not get reply from fwdServer\n");
		close (sock);
		return INTERNAL_ERR;
	}
	// Unstripe the content read
	char rst[10];
	unsigned int rcvdIP;
	unsigned short rcvdPort;
	char strIP[20], strPort[10];
	int fields = sscanf(buf, "%s %s %s", rst, strIP, strPort);
	if (fields != 3) {
		dprintf(D_ALWAYS, "FwdClient.C - scanf failed\n");
		close (sock);
		return INTERNAL_ERR;
	}
	rcvdIP = atoi(strIP);
	rcvdPort = atoi(strPort);
	
	if ( !strcmp (NAK, rst) ) {
		// In case of NAK, rcvdIP is an err code
#ifdef MYDEBUG
		cout << "MASQ recv: " << errMsg[rcvdIP] << endl;
#endif
		dprintf (D_NETWORK, "MASQ recv: (NAK - %s)\n", errMsg[rcvdIP]);
		close (sock);
		return rcvdIP;
	} else {
#ifdef MYDEBUG
		cout << "MASQ recv: " << rst << " " << ipport_to_string(rcvdIP, rcvdPort) << endl;
#endif
		dprintf (D_NETWORK, "MASQ recv: (%s %s)\n", rst, ipport_to_string(rcvdIP, rcvdPort));
		if (outIP && outPort) {
			*outIP = rcvdIP;
			*outPort = rcvdPort;
		}
		close (sock);
		return SUCCESS;
	}
}
