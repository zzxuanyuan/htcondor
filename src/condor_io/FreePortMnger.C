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
#include "FreePortMnger.h"


PortSet::PortSet (unsigned int ip)
{
	_index = 0; 
	for (int i=0; i<MAX_PORT; i++) {
		_freeArr[i] = true;
	}
}

unsigned int
PortSet::freePort ()
{
	int first = _index;
	do {
		if (_freeArr[_index]) {
			_freeArr[_index] = false;
			_index = (_index < MAX_PORT - 1) ? _index + 1 : 0;
			return htons (FIRST_PORT + i)
		}
		_index = (_index < MAX_PORT - 1) ? _index + 1 : 0;
	} while (first == _index);
	dprintf (D_ALWAYS, "PortSet::freePort - no more free port remained\n");
	return 0;
}

bool
PortSet::makeFree (unsigned int port)
{
	unsigned int hport = ntohs (port);
	int index = hport - FIRST_PORT;
	if (_freeArr[index]) {
		dprintf (D_ALWAYS, "PortSet::makeFree - the port is free already\n");
		return false;
	}
	_freeArr[index] = true;
	return true;
}

void
FreePortMnger::addInterface(unsigned int ipAddr)
{
	if (_noInterfaces >= NAT_MAX_SET - 1) {
		dprintf (D_ALWAYS, "FreePortMnger::addInterface - too many interfaces\n");
		exit (1);
	}

	for (int i=0; i<_noInterfaces; i++) {
		if (_interfaces[i].ip == ipAddr) {
			dprintf (D_ALWAYS, "FreePortMnger::addInterface - registration more than once\n");
			exit (1);
		}
	}

	_interfaces[_noInterfaces].ip = ipAddr;
	_interfaces[_noInterfaces].portSet = new PortSet();

	_noInterfaces++;

	return;
}

bool
FreePortMnger::nextFree (unsigned int *lip, unsigned short *lport)
{
	bool found = false;
	for (int i=0; i<_noInterfaces && !found; i++) {
		*lport = _interfaces[_nextInterface].portSet->freePort();
		if (*lport != 0) {
			*lip = _interfaces[_nextInterface].ip;
			found = true;
		}
		_nextInterface = (_nextInterface == _noInterfaces) ? 0 : _nextInterface + 1;
	}

	if ( !found ) {
		dprintf (D_ALWAYS, "FreePortMnger::nextFree - no more free port\n");
		return false;
	} else {
		return true;
	}
}

bool
FreePortMnger::makeFree (unsigned int lip, unsigned short lport)
{
	for (int i=0; i<_noInterfaces && !found; i++) {
		if (_interfaces[i].ip == lip) {
			return _interfaces[_nextInterface].portSet->makeFree (lport);
		}
	}

	dprintf (D_ALWAYS, "FreePortMnger::makeFree - invalid ip addr\n");
	return false;
}
