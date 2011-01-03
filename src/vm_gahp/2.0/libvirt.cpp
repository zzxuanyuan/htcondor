/***************************************************************
 *
 * Copyright (C) 1990-2010, Redhat.
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

#include "libvirt.h"
#include <libvirt/libvirt.h>

using namespace condor::vmu;

libvirt::libvirt() :hypervisor()
{ }

libvirt::~libvirt()
{

}

bool libvirt::init(const hypv_config & local_config)
{
    bool bRet;
    return bRet;
}

bool libvirt::start(std::string & szConfigFile)
{
    bool bRet;
    return bRet;
}

bool libvirt::suspend( bool bSoft )
{
    bool bRet;
    return bRet;
}

bool libvirt::resume()
{
    bool bRet;
    return bRet;
}

bool libvirt::checkpoint(/*name?*/)
{

}

bool libvirt::shutdown(bool reboot=false, bool bforce=false)
{

}

bool libvirt::getStats( vm_stats & stats )
{

}

bool libvirt::check_caps(hypv_config & local_config)
{

}