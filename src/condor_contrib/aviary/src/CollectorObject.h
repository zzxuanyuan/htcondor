/*
* Copyright 2009-2012 Red Hat, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _COLLECTOROBJECT_H
#define _COLLECTOROBJECT_H

// condor includes
#include "condor_common.h"
#include "condor_classad.h"

#include "Collectables.h"

using namespace std;
using namespace compat_classad;

namespace aviary {
namespace collector {

typedef map<string,Collector*> CollectorList;
typedef map<string,Master*> MasterList;
typedef map<string,Negotiator*> NegotiatorList;
typedef map<string,Scheduler*> SchedulerList;
typedef map<string,Submitter*> SubmitterList;
// slots... STATIC, PARTITIONABLE, DYNAMIC
typedef map<string,Slot*> SlotList;
    
class CollectorObject
{
public:

    // SOAP-facing method


    // daemonCore-facing methods
    void update(const ClassAd& ad);
    void invalidate(const ClassAd& ad);
    void invalidateAll();

    CollectorObject();
    ~CollectorObject();
    string getPool();

private:
    CollectorList m_collectors;
    MasterList m_masters;
    NegotiatorList m_negotiators;
    SchedulerList m_schedulers;
    SlotList m_slots;
    SubmitterList m_submitters;

};

extern CollectorObject collector;

}} /* aviary::collector */

#endif /* _COLLECTOROBJECT_H */
