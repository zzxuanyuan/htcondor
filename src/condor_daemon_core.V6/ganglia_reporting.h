/***************************************************************
 *
 * Copyright (C) 1990-2013, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef _GANGLIA_REPORTING_H_
#define _GANGLIA_REPORTING_H_

#include <vector>

#include <boost/utility.hpp>

#include "dc_service.h"

// ganglia.h contains expressions which are invalid C++
// Throughout the next few files, you will see bizarre constructs to
// make sure that only "C" compilers are invoked for any function that
// interacts with Ganglia.
typedef struct Ganglia_pool_s* Ganglia_pool;
typedef struct Ganglia_gmond_config_s* Ganglia_gmond_config;
typedef struct Ganglia_udp_send_channels_s* Ganglia_udp_send_channels;

// Forward dec'ls
//
class StatisticsPool;

namespace condor {

/*
 * A global object which allows us to push statistics to Ganglia.
 */
class GangliaReporting : public Service, public boost::noncopyable
{
public:
    /* Reconfigure this object. */
    void Reconfig(void);

    /* Publish a report */
    void Publish(void);

    /* Register */
    void Register(StatisticsPool *pool);

    /* Unregister */
    void Unregister(StatisticsPool *pool);

    /* Get the global instance */
    static GangliaReporting &
    GetInstance();

private:
    GangliaReporting();
    ~GangliaReporting();

    // Publish the contents of a pool
    void PublishPool(StatisticsPool &);

    // Context from ganglia.
    Ganglia_pool m_context;

    // Send channels.
    Ganglia_udp_send_channels m_send_channels;

    // Configuration file.
    Ganglia_gmond_config m_config;

    // List of registered statistics pools.
    std::vector<StatisticsPool*> m_pools;

    int m_flags;
    bool m_configured;

    // Global instance pointer.
    static
    GangliaReporting *m_instance;
};

}
#endif
