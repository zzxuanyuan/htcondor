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
    int result = compare (m_cluster, condorID.m_cluster);
    if (result == 0) result = compare (m_proc, condorID.m_proc);
    if (result == 0) result = compare (m_subproc, condorID.m_subproc);
    return result;
}

//---------------------------------------------------------------------------
CondorID::operator std::string() const {
    std::string s;
    s += '(';
    s += toString(m_cluster);
    s += '.';
    s += toString(m_proc);
    s += '.';
    s += toString(m_subproc);
    s += ')';
    return s;
}

//---------------------------------------------------------------------------
ostream & operator << (ostream & out, const CondorID & condorID) {
    out << (std::string) condorID;
    return out;
}

//---------------------------------------------------------------------------
std::string toString (int i) {
    char s[32];
    sprintf (s, "%d", i);
    return s;
}

} // namespace dagman
