
#ifndef _CONDOR_SIGNALS_CONTROL_H
#define _CONDOR_SIGNALS_CONTROL_H

/*
These two functions disable and enable the handling
of the special Condor signals, regardless of the state of the
signal mapping table.

These functions were moved to the ckpt module becuase they functionally
belong to the signal handling code, and serve to hide the hairy
details of signal implementation.
*/

#include "condor_common.h"

BEGIN_C_DECLS

sigset_t _condor_signals_disable();
void     _condor_signals_enable(sigset_t s);

END_C_DECLS

#endif /* _CONDOR_SIGNALS_CONTROL_H */

