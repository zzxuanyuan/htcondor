#include <stdio.h>
#include <iostream>

int
main(void)
{
	int ret = setFWrule(_masqServer, ADD, proto, _myIP, _myPort, &_lip, &_lport, _mport);
	if (ret != SUCCESS) {
		dprintf (D_ALWAYS, "Sock::bind adding fw rule failed\n");
		dprintf (D_ALWAYS, "\t - errcode: %d\n", ret);
		return FALSE;
	}

	int ret = setFWrule (_masqServer, DELETE, proto, _lip, _lport, NULL, NULL, _mport);
	if ( ret != SUCCESS) {
		dprintf (D_ALWAYS, "Sock::deleteFWrule deleting fw rule failed\n");
		dprintf (D_ALWAYS, "\t - errcode: %d\n", ret);
		return FALSE;
	}
}
