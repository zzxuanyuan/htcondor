//#include "condor_common.h"
#include "dag.h"

template class std::list<dagman::Job *>;
template class std::list<dagman::Job *>::iterator;

template class std::list<dagman::JobID_t>;
template class std::list<dagman::JobID_t>::iterator;

template class std::list<dagman::TQI *>;
template class std::list<dagman::TQI *>::iterator;

#if 0
template class rb_tree<Job *,   Job *,   identity<Job *>,   less<Job *> >;
template class rb_tree<JobID_t, JobID_t, identity<JobID_t>, less<JobID_t> >;
template class rb_tree<TQI *,   TQI *,   identity<TQI *>,   less<TQI *> >;
#endif
