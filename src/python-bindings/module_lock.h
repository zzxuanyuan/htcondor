
#ifndef __MODULE_LOCK_H_
#define __MODULE_LOCK_H_

#ifdef WIN32
// Some definitions to translate types from pthread to Win32
typedef CRITICAL_SECTION pthread_mutex_t;

#else
#include <pthread.h>
#endif

namespace condor {

class ModuleLock {

public:
    ModuleLock();
    ~ModuleLock();

    void acquire();
    void release();

private:

    bool m_release_gil;
    bool m_owned;
    static pthread_mutex_t m_mutex;
    PyThreadState *m_save;
};

}

#endif
