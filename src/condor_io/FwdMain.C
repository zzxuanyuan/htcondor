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
#include "portfw.h"
#include "FwdMnger.h"

char* mySubSystem = "FWD_SERVER";       // used by Daemon Core


void inline
usage (char *msg)
{
	cerr << "Usage: FwdServer [DaemonCore options] -i Lip -p Lport [-I Pip]+" << endl;
	cerr << "\tLip: ip address connected to the private network\n";
	cerr << "\t\tIf you have multiple interface connected to the private network,\n";
	cerr << "\t\tyou should run this program for each interface to fully utilize the bandwidth\n";
	cerr << "\tLport: port this daemon is listening from Cedars running in private network\n";
	cerr << "\tPip(Public IP): Internet ip address known to public(i.e. Internet)\n"; 
	cerr << "\t\tIf the machine has multiple interface connected to Internet,\n";
	cerr << "\t\tyou should specify every ip-addr of them to fully utilize the bandwidth\n";
	cerr << "\n" << msg << endl;
	return;
}


int
main_init (int argc, char *argv[])
{
	unsigned long mngAddr = 0;
	unsigned short mngPort = 0;
	FwdMnger *tcpPortMnger = NULL;
	FwdMnger *udpPortMnger = NULL;

	// get root privilige
	priv_state priv = set_root_priv();

	tcpPortMnger = new FwdMnger(TCP);
	if (!tcpPortMnger) {
		perror ("Couldn't create tcpPortMnger object");
		exit(-1);
	}
	udpPortMnger = new FwdMnger(UDP);
	if (!udpPortMnger) {
		perror ("Couldn't create udpPortMnger object");
		exit(-1);
	}


    	/* get command line arguments */
	char c;
	int noInterfaces = 0;
	while ( (c = getopt(argc, argv, "i:p:I:")) != (unsigned) -1 ) {
		switch (c) {
			case 'i':
				if(mngAddr != 0) {
					usage("local IP-addr defined multiple times");
					exit(0);
                }
                mngAddr = inet_addr (optarg);
				break;
			case 'p':
				if (mngPort != 0) {
					usage("local port number defined multiple times");
					exit(0);
				}
                mngPort = atoi (optarg);
                break;
            case 'I':
				tcpPortMnger->addInterface(optarg);
				udpPortMnger->addInterface(optarg);
				noInterfaces++;
                break;
            default:
				usage("");
                exit (0);
        }
    }
    if (mngAddr == 0 || mngPort == 0) {
        usage ("local IP and port # should be specified");
        exit (0);
    }
	if (noInterfaces == 0) {
		usage("At least one public interface should be given");
		exit(0);
	}

    	/* initialize forwardMngers*/
	dprintf(D_ALWAYS, "Initial Synchronization...\n");
	tcpPortMnger->sync();
	udpPortMnger->sync();

    	/* create a socket */
	ReliSock * cmdSock = new ReliSock();
	if ( !cmdSock ) {
		EXCEPT ("Failed to create ReliSock\n");
	}
	if (cmdSock->assign() != TRUE) {
		EXCEPT("FwdServer - failed to assign _sock to cmdSock");
	}
	int listenSock = cmdSock->get_file_desc();
    
    	/* bind to the given mngAddr and mngPort */
	// Because we need to bind to a specific ip-addr, we can't use Cedar bind,
	// which binds socket to (my_ip_addr(), given port)
	struct sockaddr_in mng_sin;
	memset (&mng_sin, 0, sizeof (mng_sin));
	mng_sin.sin_family = PF_INET;
	mng_sin.sin_port = htons (mngPort);
	mng_sin.sin_addr.s_addr = mngAddr;
	if (bind (listenSock, (struct sockaddr *)&mng_sin, sizeof (mng_sin)) < 0) {
		EXCEPT ("bind failed");
	}

    	/* make listenSock passive */
    if (listen (listenSock, 5) < 0) {
        EXCEPT ("listen call failed");
    }

		/* create FwdServer */
	FwdServer * fwdServer = new FwdServer (mngAddr, tcpPortMnger, udpPortMnger);
	if (fwdServer == NULL) {
		EXCEPT ("Failed to create FwdServer\n");
	}
	dprintf(D_ALWAYS, "FwdServer created\n");

		/* register Socket and Timer */
	daemonCore->Register_Socket (cmdSock, "<FwdServer Command Socket>",
								 (SocketHandlercpp) &FwdServer::handleCommand,
								 "port forwarding command handler",
								 fwdServer, ALLOW);
	dprintf(D_ALWAYS, "Socket Registered\n");
	daemonCore->Register_Timer (180, 180,
								(Eventcpp) &FwdServer::probeCedars,
								"Cedar Heartbeat checker",
								fwdServer);
	dprintf(D_ALWAYS, "Timer Registered\n");

	// return to the previous privilige
	set_priv(priv);

	return TRUE;
}


int main_config()
{
    dprintf(D_ALWAYS, "main_config() called\n");
    return TRUE;
}


int main_shutdown_fast()
{
    dprintf(D_ALWAYS, "main_shutdown_fast() called\n");
    DC_Exit(0);
    return TRUE;    // to satisfy c++
}


int main_shutdown_graceful()
{
    dprintf(D_ALWAYS, "main_shutdown_graceful() called\n");
    DC_Exit(0);
    return TRUE;    // to satisfy c++
}
