#if !defined(_QMGR_H)
#define _QMGR_H

#if !defined(WIN32)
#include <netinet/in.h>
#endif

#define QMGR_PORT 9605

#include "condor_io.h"

#if defined(__cplusplus)
extern "C" {
#endif

int handle_q(ReliSock *, struct sockaddr_in*);

#if defined(__cplusplus)
}
#endif

#endif /* _QMGR_H */
