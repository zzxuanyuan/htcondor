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

#include "hypervisor_factory.h"

//////Begin non-plugin temporary goo
#include "xen.h"
#include "kvm.h"
#include <boost/bind.hpp>
//////End non-plugin temporary goo

using namespace std;
using namespace boost;
using namespace condor::vmu;

/// static var init
map<string, hypervisor_factory::pfnManufacture> hypervisor_factory::m_SupportedHypervisors;

//////TODO:remove-> temporary remove once plugin capabilities are in place
bool m_bInitialized = hypervisor_factory::init();

////////////////////////////////////////////////
bool hypervisor_factory::discover(const string& szVMType, boost::shared_ptr<hypv_config> & local_config )
{
    bool bRet = 0;
    shared_ptr< hypervisor > pHypervisor = hypervisor_factory::manufacture( szVMType, local_config );

    if (pHypervisor)
    {
        // read in the configuration settings
        bRet = local_config->read_config();

        // if all is well then check the capabilities.
        if (bRet)
            bRet = pHypervisor->check_caps(local_config);
    }

    return (bRet);
}

////////////////////////////////////////////////
shared_ptr< hypervisor > hypervisor_factory::manufacture(const string& szVMType, boost::shared_ptr<hypv_config> & local_config)
{
    shared_ptr< hypervisor > pHypervisor;
    pfnManufacture pfnMF = m_SupportedHypervisors[szVMType];

    if (pfnMF)
    {
        pHypervisor = pfnMF(local_config);
    }

    return (pHypervisor);
}

////////////////////////////////////////////////
bool hypervisor_factory::init()
{
    bool bRet;
    pfnManufacture pfn = boost::bind( &kvm::manufacture, _1 );

    bRet = registerMfgFn("kvm", pfn);
    //registerMfgFn("xen",

    return (bRet);
}

////////////////////////////////////////////////
bool hypervisor_factory::registerMfgFn( const string& szVMType, const pfnManufacture & pfn )
{
    bool bRet = true;

    if (!m_SupportedHypervisors[szVMType])
    {
        m_SupportedHypervisors[szVMType]=pfn;
    }
    else
        bRet = false;

    return (bRet);
}

