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
#include <libvirt/virterror.h>
#include "vmgahp_common.h"

using namespace std;
using namespace condor::vmu;

string libvirt::m_szLastError;

libvirt::libvirt()
:hypervisor()
{ }

libvirt::~libvirt()
{

}

bool libvirt::init(const hypv_config & local_config)
{
    bool bRet=true;
    return bRet;
}

bool libvirt::start(string & szConfigFile)
{
    bool bRet=true;
    return bRet;
}

bool libvirt::suspend( bool bSoft )
{
    bool bRet=true;
    return bRet;
}

bool libvirt::resume()
{
    bool bRet=true;
    return bRet;
}

bool libvirt::checkpoint(/*name?*/)
{
    bool bRet=true;
    return bRet;
}

bool libvirt::shutdown(bool reboot, bool bforce)
{
    bool bRet=true;
    return bRet;
}

bool libvirt::getStats( vm_stats & stats )
{
    bool bRet=true;
    return bRet;
}

bool libvirt::check_caps(hypv_config & local_config)
{
    bool bRet = false;
    virConnectPtr plibvirt = 0;//virConnectOpen( m_szSessionID.c_str() );

    //if (plibvirt)
    {
        char * pszCaps = virConnectGetCapabilities (plibvirt);

        if (pszCaps)
        {
            vmprintf(D_ALWAYS, "dump of caps:\n%s", pszCaps );
            bRet=true;
        }
        else
        {
            vmprintf(D_ALWAYS, "Error on virConnectGetCapabilities() to %s: %s", m_szSessionID.c_str(), this->getLastError() );
        }

        // Lists the networks available
        // virConnectListNetworks()

        virConnectClose(plibvirt);

    }
    //else
    //{
    //    vmprintf(D_ALWAYS, "Error on virConnectOpen() to %s: %s", m_szSessionID.c_str(), this->getLastError() );
    //}

    return (bRet);

}

const char * libvirt::getLastError()
{
    virErrorPtr perr = virGetLastError();

    if (perr)
        m_szLastError = perr->message;
    else
        m_szLastError = "No Reason Found";

    return ( m_szLastError.c_str() );
}

