#ifndef _TYPES_H_
#define _TYPES_H_

#include <string>

//---------------------------------------------------------------------------
#ifndef __cplusplus
typedef int bool;
#define true  1
#define false 0
#endif

namespace dagman {

///
typedef int JobID_t;

//---------------------------------------------------------------------------
/** Condor uses three integers to identify jobs. This structure 
    will be used to store those three numbers.  
*/
class CondorID {
  public:
    ///
    CondorID () : m_cluster(-1), m_proc(-1), m_subproc(-1) {}

    ///
    CondorID (int cluster, int proc, int subproc):
        m_cluster(cluster), m_proc(proc), m_subproc(subproc) {}

    ///
    inline void Set (int cluster, int proc, int subproc) {
        m_cluster = cluster;
        m_proc    = proc;
        m_subproc = subproc;
    }

    /** Compare this condorID's with another
        @return zero if they match
    */
    int Compare (const CondorID condorID) const;

    ///
    inline bool operator == (const CondorID condorID) const {
        return Compare (condorID) == 0;
    }

    ///
    operator std::string() const;

    /** */ int m_cluster;
    /** */ int m_proc;
    /** */ int m_subproc;
};

//---------------------------------------------------------------------------
ostream & operator << (ostream & out, const CondorID & condorID);
std::string toString (int i);

} // namespace dagman

#endif /* #ifndef _TYPES_H_ */
