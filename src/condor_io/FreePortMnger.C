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
#include "FreePortMnger.h"


PortSet::PortSet ()
{
	_index = 0;
	for (int i=0; i<MAX_PORT; i++) {
		_freeArr[i] = true;
	}
}

uint16_t
PortSet::freePort ()
{
	int index = _index;
	do {
		if (_freeArr[index]) {
			_freeArr[index] = false;
			_index = (index < MAX_PORT - 1) ? index + 1 : 0;
			return htons (FIRST_PORT + index);
		}
		index = (index < MAX_PORT - 1) ? index + 1 : 0;
	} while (first != _index);
	dprintf (D_ALWAYS, "PortSet::freePort - no more free port remained\n");
	return 0;
}

uint16_t
PortSet::freePort (uint16_t port)
{
	int first = ntohs(port) - FIRST_PORT;
	int index = first;
	do {
		if (_freeArr[index]) {
			_freeArr[index] = false;
			_index = (index < MAX_PORT - 1) ? index + 1 : 0;
			return htons (FIRST_PORT + index);
		}
		index = (index < MAX_PORT - 1) ? index + 1 : 0;
	} while (first != index);
	dprintf (D_ALWAYS, "PortSet::freePort - no more free port remained\n");
	return 0;
}

bool
PortSet::makeOccupied (uint16_t port)
{
	unsigned short hport = ntohs (port);
	int index = hport - FIRST_PORT;
	if ( !_freeArr[index] ) {
		dprintf (D_ALWAYS, "PortSet::makeOccupied - the port is occupied already\n");
		return false;
	}
	_freeArr[index] = false;
	return true;
}

bool
PortSet::makeFree (uint16_t port)
{
	unsigned short hport = ntohs (port);
	int index = hport - FIRST_PORT;
	if (_freeArr[index]) {
		dprintf (D_ALWAYS, "PortSet::makeFree - the port is free already\n");
		return false;
	}
	_freeArr[index] = true;
	return true;
}

FreePortMnger::FreePortMnger()
{
	for (int i=0; i < MAX_SET; i++) {
		_interfaces[i].ip = 0;
		_interfaces[i].portSet = NULL;
	}
}

void
FreePortMnger::addInterface(uint32_t ipAddr)
{
	if (_noInterfaces >= MAX_SET - 1) {
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
FreePortMnger::nextFree(uint32_t *lip, uint16_t *lport)
{
	bool found = false;
	for (int i=0; i<_noInterfaces && !found; i++) {
		*lport = _interfaces[_nextInterface].portSet->freePort();
		if (*lport != 0) {
			*lip = _interfaces[_nextInterface].ip;
			found = true;
		}
		_nextInterface = (_nextInterface == _noInterfaces-1) ? 0 : _nextInterface + 1;
	}

	if ( !found ) {
		dprintf (D_ALWAYS, "FreePortMnger::nextFree - no more free port\n");
		return false;
	} else {
		return true;
	}
}

bool
FreePortMnger::nextFree(uint16_t rport, uint32_t *lip, uint16_t *lport)
{
	bool found = false;
	for (int i=0; i<_noInterfaces && !found; i++) {
		*lport = _interfaces[_nextInterface].portSet->freePort(rport);
		if (*lport != 0) {
			*lip = _interfaces[_nextInterface].ip;
			found = true;
		}
		_nextInterface = (_nextInterface == _noInterfaces-1) ? 0 : _nextInterface + 1;
	}

	if ( !found ) {
		dprintf (D_ALWAYS, "FreePortMnger::nextFree - no more free port\n");
		return false;
	} else {
		return true;
	}
}

bool
FreePortMnger::makeOccupied (uint32_t lip, uint16_t lport)
{
	for (int i=0; i<_noInterfaces; i++) {
		if (_interfaces[i].ip == lip) {
			return _interfaces[i].portSet->makeOccupied (lport);
		}
	}

	dprintf (D_ALWAYS, "FreePortMnger::makeFree - invalid ip addr\n");
	return false;
}

bool
FreePortMnger::makeFree (uint32_t lip, uint16_t lport)
{
	for (int i=0; i<_noInterfaces; i++) {
		if (_interfaces[i].ip == lip) {
			return _interfaces[i].portSet->makeFree (lport);
		}
	}

	dprintf (D_ALWAYS, "FreePortMnger::makeFree - invalid ip addr\n");
	return false;
}

FreePortMnger::~FreePortMnger()
{
	for (int i=0; i<_noInterfaces; i++) {
		delete _interfaces[i].portSet;
	}
}
