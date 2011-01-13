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

#include "condor_common.h"
#include "condor_attributes.h"
#include "condor_config.h"

#include "libvirt.h"
#include "vmgahp_common.h"
#include "stl_string_utils.h"
#include <libvirt/virterror.h>

using namespace std;
using namespace condor::vmu;

string libvirt::m_szLastError;

//////////////////////////////////////////////////////////////
bool libvirt_config::InsertAddAttr( ClassAd & ad )
{
    string szAttribute;

    sprintf(szAttribute, "VM_libvirt_%s_capabilities", m_VM_TYPE.c_str());
    bool bRet = ad.InsertAttr( szAttribute, m_szCaps );
    hypv_config::InsertAddAttr(ad);

    return bRet;
}
//////////////////////////////////////////////////////////////

libvirt::libvirt()
:hypervisor()
{ }

libvirt::~libvirt()
{

}

bool libvirt::config(const boost::shared_ptr<hypv_config> & local_config)
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

bool libvirt::check_caps(boost::shared_ptr<hypv_config> & local_config)
{
    bool bRet = false;
    boost::shared_ptr<libvirt_config> pConfig = boost::dynamic_pointer_cast<libvirt_config> (local_config);
    virConnectPtr plibvirt = virConnectOpen( m_szSessionID.c_str() );

    if (plibvirt)
    {
        // in order to get a hypervisor's capabilities you need to connect to it.
        char * pszCaps = virConnectGetCapabilities (plibvirt);

        if ( pszCaps )
        {
            pConfig->m_szCaps = pszCaps;
            bRet=true;
        }
        else
        {
            vmprintf(D_ALWAYS, "Error on virConnectGetCapabilities() to %s: %s", m_szSessionID.c_str(), this->getLastError() );
        }

        virConnectClose(plibvirt);

    }
    else
    {
        vmprintf(D_ALWAYS, "Error on virConnectOpen() to %s: %s", m_szSessionID.c_str(), this->getLastError() );
    }

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

