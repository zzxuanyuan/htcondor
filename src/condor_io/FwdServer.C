#include "condor_common.h"
#include "condor_constants.h"
#include "condor_io.h"
#include "sock.h"
#include "condor_network.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_debug.h"
#include "condor_socket_types.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "getParam.h"
#include "condor_rw.h"
#include "portfw.h"
#include "fwdMnger.h"

bool inline getLine(int sock, char *buf, int max);
void inline reply (const int, const char *, const unsigned int, const unsigned short);


/* Get a line from this socket.  Return true on success. */
bool inline
getLine(int sock, char *buf, int max)
{
    int rbytes = 0;

    while (rbytes <= max) {
        if ( read(sock, &buf[rbytes], 1) != 1 ) {
            return false;
        }
        if(buf[rbytes] == '\n') {
            buf[rbytes] = '\0';
            return true;
        } else rbytes++;
    }
    return false;
}


void inline
reply (const int fd, const char *tag, const unsigned int ip, const unsigned short port)
{
	int written = 0;
	char msg[50];

	sprintf (msg, "%s %d %d", tag, ip, port);
	int len = strlen (msg);
	while (written != len) {
		int result = write (fd, msg, len - written);
		if (result < 0) {
			perror ("reply: failed to send msg");
			return;
		} else if (result == 0) {
			cerr << "reply: Cedar has closed the connection prematurely\n";
			return;
		} else {
			written += result;
		}
	}
	return;
}


inline bool
probeCedar (unsigned int myIP, unsigned int c_ip, unsigned short c_port)
{
	struct sockaddr_in sin, peer;
	int addrLen = sizeof (struct sockaddr_in);

	int sock = socket (PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		EXCEPT ("probeCedar - socket create failed: ");
	}

	memset (&sin, 0, sizeof (sin));
	sin.sin_family = PF_INET;
	sin.sin_addr.s_addr = myIP;
	if (bind (sock, (sockaddr *) &sin, addrLen) < 0) {
		EXCEPT ("probeCedar - bind failed: ");
	}

	memset (&peer, 0, sizeof (peer));
	peer.sin_family = PF_INET;
	peer.sin_addr.s_addr = c_ip;
	peer.sin_port = c_port;
	if (connect (sock, (sockaddr *) &peer, addrLen) < 0) {
		close (sock);
		return true;
	} else {
		close (sock);
		return false;
	}
}


inline void
probePerMnger (unsigned int ipAddr, FwdMnger * mnger)
{
	// get the list of cedars to contact
	struct fwRule *rules, *tptr;
	tptr = rules = mnger->getRules ();
	while (tptr) {
		if ( !probeCedar (ipAddr, tptr->rip, tptr->mport) ) {
			mnger->deleteRule (tptr->lip, tptr->lport, &(tptr->rip), &(tptr->rport));
		}
		tptr = tptr->next;
	}
}


void
FwdServer::probeCedars ()
{
	// make internal representation, condor persistent table, kernel table consistent
	_tcpPortMnger->sync ();
	_udpPortMnger->sync ();

	// check peer Cedars are still alive
	probePerMnger (_ipAddr, _tcpPortMnger);
	probePerMnger (_ipAddr, _udpPortMnger);
}


FwdServer::FwdServer (unsigned int ip, FwdMnger * tcpMnger, FwdMnger * udpMnger)
{
	_ipAddr = ip;
	_tcpPortMnger = tcpMnger;
	_udpPortMnger = udpMnger;
}


int
FwdServer::handleCommand (Stream *cmdSock)
{
	struct sockaddr_in cedar;
	unsigned int addrLen = sizeof (cedar);
	char lineBuf[100];
	char cmd[30];
	char proto[30];
	unsigned int ipAddr;
	unsigned int rst_ip;
	unsigned short port;
	unsigned short rst_port;
	unsigned short mport;
	int result;

	// I am not a Cedar application, so I am working on underlying socket directly
	int listenSock = ((ReliSock *)cmdSock)->get_file_desc();
	int newSock = accept (listenSock, (struct sockaddr *)&cedar, &addrLen);
	if (newSock < 0) {
		EXCEPT ("accept failed");
	}

	if (!getLine (newSock, lineBuf, 100)) {
		EXCEPT ( "getLine failed");
	}
	int fields = sscanf(lineBuf, "%s %s %d %d %d", cmd, proto, &ipAddr, &port, &mport);
	if (fields != 4) {
		EXCEPT ("FwdServer::handleCommand - scanf failed: ");
	}
	dprintf (D_NETWORK, "(%s, %s, %d, %d, %d)\n", cmd, proto, ntohl(ipAddr), ntohs(port), ntohs(mport));
	if (!strcmp (cmd, ADD)) {
		if (!strcmp (proto, TCP)) {
			result = _tcpPortMnger->addRule(&rst_ip, &rst_port, ipAddr, port, mport);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, ADDED, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->addRule (&rst_ip, &rst_port, ipAddr, port, mport);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, ADDED, rst_ip, rst_port);
			}
		} else {
			reply (newSock, NAK, INVALID_PROTO, INVALID_PROTO);
		}
	} else if (!strcmp (cmd, QUERY)) {
		if (!strcmp (proto, TCP)) {
			result = _tcpPortMnger->queryRule (ipAddr, port, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, RESULT, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->queryRule (ipAddr, port, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, RESULT, rst_ip, rst_port);
			}
		} else {
			reply (newSock, NAK, INVALID_PROTO, INVALID_PROTO);
		}
	} else if (!strcmp (cmd, DELETE)) {		// close
		if (!strcmp (proto, TCP)) {
			result = _tcpPortMnger->deleteRule (ipAddr, port, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, DELETED, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->deleteRule (ipAddr, port, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, DELETED, rst_ip, rst_port);
			}
		} else {
			reply (newSock, NAK, INVALID_PROTO, INVALID_PROTO);
		}
	} else {		// invalid command
		reply (newSock, NAK, INVALID_CMD, INVALID_CMD);
	}
	close (newSock);
	return TRUE;
}
