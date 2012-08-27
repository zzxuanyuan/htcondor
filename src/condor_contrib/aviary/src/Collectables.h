/*
 * Copyright 2009-2011 Red Hat, Inc.
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

#ifndef _COLLECTABLES_H
#define _COLLECTABLES_H

#include <string>
#include <set>

using namespace std;

namespace aviary {
namespace collector {
    
    typedef set<Slot*> DynamicSlotList;
    
    struct Collectable {
        string Pool;
        string Name;
        string MyAddress;
        int DaemonStartTime;
    };
    
    struct Collector: public Collectable {
        int RunningJobs;
        int IdleJobs;
        int HostsTotal;
        int HostsClaimed;
        int HostsUnclaimed;
        int HostsOwner;
    };

    struct Master: public Collectable {
        string Arch;
        string OpSysLongName;
        int RealUid;
    };
    
    struct Negotiator: public Collectable {
        double MatchRate;
        int Matches;
        int Duration;
        int NumSchedulers;
        int ActiveSubmitterCount;
        int NumIdleJobs;
        int NumJobsConsidered;
        int Rejections;
        int TotalSlots;
        int CandidateSlots;
        int TrimmedSlots;
    };
    
    struct Scheduler: public Collectable {
        int JobQueueBirthdate;
        int MaxJobsRunning;
        int NumUsers;
        int TotalJobAds;
        int TotalRunningJobs;
        int TotalHeldJobs;
        int TotalIdleJobs;
        int TotalRemovedJobs;
    };
    
    struct Slot: public Collectable {
        string Arch;
        string OpSys;
        string Activity;
        string State;
        int Cpus;
        int Disk;
        int Memory;
        int Swap;
        int Mips;
        double LoadAvg;
        string Start;
        string FileSystemDomain;
    };
    
    struct PartitionableSlot: public Slot {
        DynamicSlotList m_dynamic_slots;
    };
    
    struct Submitter {
        string Name;
        string Machine;
        string ScheddName;
        int RunningJobs;
        int HeldJobs;
        int IdleJobs;
    };
}} 

#endif /* _COLLECTABLES_H */
