#include "condor_common.h"


// DAGMan Includes
#include "types.h"

namespace dagman {

//---------------------------------------------------------------------------
int compare(int a, int b) {
    if (a == b) return 0;
    return (a > b ? 1 : -1);
}

//---------------------------------------------------------------------------
int CondorID::Compare (const CondorID condorID) const {
    int result = compare (_cluster, condorID._cluster);
    if (result == 0) result = compare (_proc, condorID._proc);
    if (result == 0) result = compare (_subproc, condorID._subproc);
    return result;
}

//---------------------------------------------------------------------------
std::string to_string (int i) {
    char s[32];
    sprintf (s, "%d", i);
    return s;
}

} // namespace dagman
