/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/* prototype in condor_includes/get_port_range.h */

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"

int get_port_range(int *low_port, int *high_port)
{
	char *low = NULL, *high = NULL;

	if ( (low = param("LOWPORT")) == NULL ) {
		return FALSE;
    }
	if ( (high = param("HIGHPORT")) == NULL ) {
		free(low);
        dprintf(D_ALWAYS, "LOWPORT is defined but, HIGHPORT undefined!\n");
		return FALSE;
	}

    *low_port = atoi(low);
    *high_port = atoi(high);

    if(*low_port < 1024 || *high_port < 1024 || *low_port > *high_port) {
        dprintf(D_ALWAYS, "get_port_range - invalid LOWPORT(%d) \
                           and/or HIGHPORT(%d)\n",
                           *low_port, *high_port);
        free(low);
        free(high);
        return FALSE;
    }

    free(low);
    free(high);
    return TRUE;
}


int getMnger (char *mHost, int *mPort)
{
	char *host = NULL, *port = NULL;

	if ( (host = param("NETMNGER_IP")) == NULL ) {
        dprintf(D_NETWORK, "NETMNGER_IP undefined\n");
		return FALSE;
    }
	if ( (port = param("NETMNGER_PORT")) == NULL ) {
		free(host);
        dprintf(D_NETWORK, "NETMNGER_PORT undefined\n");
		return FALSE;
	}

	strcpy (mHost, host);
    *mPort = atoi(port);

    free(host);
    free(port);
    return TRUE;
}


int getMasqServer (char *mHost, unsigned short *mPort)
{
	char *host = NULL, *port = NULL;

	if ( (host = getenv("_condor_MASQ_SERVER_IP")) == NULL ) {
		if ( (host = param("MASQ_SERVER_IP")) == NULL ) {
			return FALSE;
		}
    }

	if ( (port = getenv("_condor_MASQ_SERVER_PORT")) == NULL ) {
		if ( (port = param("MASQ_SERVER_PORT")) == NULL ) {
			free(host);
			dprintf(D_NETWORK, "MASQ_SERVER_IP is defined but ");
			dprintf(D_NETWORK, "MASQ_SERVER_PORT NOT defined\n");
			return FALSE;
		}
    }

	strcpy (mHost, host);
    *mPort = atoi(port);

    free(host);
    free(port);
    return TRUE;
}
