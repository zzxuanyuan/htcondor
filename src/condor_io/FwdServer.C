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
#include "FwdMnger.h"

extern char *errMsg[];

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

#ifdef MYDEBUG
	if (!strcmp(tag, NAK)) {
		cout << "\t\tReply: NAK (" << errMsg[ip] << ")\n"; 
	} else {
		if (!strcmp(tag, ADDED)) {
			cout << "\t\tReply: ADDED " << ipport_to_string(ip, port) << endl;
		} else if (!strcmp(tag, RESULT)) {
			cout << "\t\tReply: RESULT " << ipport_to_string(ip, port) << endl;
		} else if (!strcmp(tag, DELETED)) {
			cout << "\t\tReply: DELETED " << ipport_to_string(ip, port) << endl;
		} else {
			cout << "\t\tReply: invalid code\n";
			exit(1);
		}
	}
#endif
	sprintf (msg, "%s %d %d\n", tag, ip, port);
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
	if ( !connect(sock, (sockaddr *)&peer, addrLen) ) {
#ifdef MYDEBUG
		cout << "\t\tHeartBeat: " << ipport_to_string(c_ip, c_port) << "is alive\n";
#endif
		close (sock);
		return true;
	} else {
#ifdef MYDEBUG
		cout << "\t\tHeartBeat: " << ipport_to_string(c_ip, c_port) << "is dead\n";
#endif
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
	// get root privilige
	priv_state priv = set_root_priv();

	// make internal representation, condor persistent table, kernel table consistent
#ifdef MYDEBUG
	cout << "\tSynching _tcpPortMnger...\n";
#endif
	_tcpPortMnger->sync ();
#ifdef MYDEBUG
	cout << "\tSynching _udpPortMnger...\n";
#endif
	_udpPortMnger->sync ();

	// check peer Cedars are still alive
#ifdef MYDEBUG
	cout << "\tProbing Cedars for _tcpPortMnger...\n";
#endif
	probePerMnger (_ipAddr, _tcpPortMnger);
#ifdef MYDEBUG
	cout << "\tProbing Cedars for _udpPortMnger...\n";
#endif
	probePerMnger (_ipAddr, _udpPortMnger);

	// go back to the previous privilige
	set_priv(priv);
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
	unsigned int ipAddr1, ipAddr2;
	unsigned int rst_ip;
	unsigned short port1, port2;
	unsigned short rst_port;
	unsigned short mport;
	int result;

	// get root privilige
	priv_state priv = set_root_priv();

	// I am not a Cedar application, so I am working on underlying socket directly
	int listenSock = ((ReliSock *)cmdSock)->get_file_desc();
	int newSock = accept (listenSock, (struct sockaddr *)&cedar, &addrLen);
	if (newSock < 0) {
		EXCEPT ("accept failed");
	}

	if (!getLine (newSock, lineBuf, 100)) {
		EXCEPT ( "getLine failed");
	}
	char tIP1[20], tPort1[10], tIP2[20], tPort2[10], tMport[10];
	int fields = sscanf(lineBuf, "%s %s %s %s %s %s %s", cmd, proto, tIP1, tPort1, tIP2, tPort2, tMport);
	if (fields != 7) {
		char err[50];
		sprintf(err, "FwdServer::handleCommand - scanf failed: %d returned", fields);
		EXCEPT (err);
	}
	ipAddr1 = atoi(tIP1);
	port1 = atoi(tPort1);
	ipAddr2 = atoi(tIP2);
	port2 = atoi(tPort2);
	mport = atoi(tMport);
#ifdef MYDEBUG
	cout << "\t\tRecv: " << cmd << " " << proto << " " << ipport_to_string(ipAddr1, port1) << " ";
	cout << ipport_to_string(ipAddr2, port2) << " " << ntohs(mport) << endl;
#endif
	dprintf (D_NETWORK, "(%s, %s, %d, %d, %d, %d, %d)\n", cmd, proto,
			ntohl(ipAddr1), ntohs(port1), ntohl(ipAddr2), ntohs(port2), ntohs(mport));
	if (!strcmp (cmd, ADD)) {
		rst_ip = ipAddr2;
		rst_port = port2;
		if (!strcmp (proto, TCP)) {
			result = _tcpPortMnger->addRule(&rst_ip, &rst_port, ipAddr1, port1, mport);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, ADDED, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->addRule (&rst_ip, &rst_port, ipAddr1, port1, mport);
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
			result = _tcpPortMnger->queryRule (ipAddr1, port1, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, RESULT, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->queryRule (ipAddr1, port1, &rst_ip, &rst_port);
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
			result = _tcpPortMnger->deleteRule (ipAddr1, port1, &rst_ip, &rst_port);
			if (result != SUCCESS) {
				reply (newSock, NAK, result, result);
			} else {
				reply (newSock, DELETED, rst_ip, rst_port);
			}
		} else if (!strcmp (proto, UDP)) {
			result = _udpPortMnger->deleteRule (ipAddr1, port1, &rst_ip, &rst_port);
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
	
	// go back to the previous privilige
	set_priv(priv);

	return KEEP_STREAM;
}
