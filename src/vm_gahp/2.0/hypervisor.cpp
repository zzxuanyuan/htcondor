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

#include "hypervisor.h"
#include "vmgahp_common.h"
#include "vm_univ_utils.h"
#include "stl_string_utils.h"
#include "MyString.h"

using namespace std;
using namespace condor::vmu;

hypv_config::hypv_config()
{

}

hypv_config::~hypv_config()
{ /* Nothing to do */ }

bool hypv_config::InsertAddAttr(ClassAd & ad)
{
    bool bRet = true;

    if (!ad.InsertAttr( ATTR_VM_TYPE, m_VM_TYPE.c_str()))
    {
        // we need to append instead
    }

    if (bRet)
        bRet = ad.InsertAttr("VM_GAHP_VERSION", "2.0");

    if (bRet)
        bRet = ad.InsertAttr(ATTR_VM_MEMORY, m_VM_MEMROY);

    if (bRet)
        bRet = ad.InsertAttr(ATTR_VM_NETWORKING, (m_VM_NETWORKING)?"TRUE":"FALSE");

    if (m_VM_NETWORKING && bRet)
    {
        bRet = ad.InsertAttr(ATTR_VM_NETWORKING_TYPES, m_VM_NETWORKING_TYPE.print_to_string());
    }

    return (bRet);
}

bool hypv_config::read_config()
{
    bool bRet=true;
    char * config_value=0;

    // Read VM_MEMORY
    m_VM_MEMROY =  param_integer("VM_MEMORY", 0);
    if( m_VM_MEMROY <= 0 )
    {
        vmprintf( D_ALWAYS,"ERROR: 'VM_MEMORY' is not defined in configuration ");
        bRet = false;
    }

    // Read VM_NETWORKING
    m_VM_NETWORKING = param_boolean("VM_NETWORKING", false);

    // Read VM_NETWORKING_TYPE
    if( m_VM_NETWORKING )
    {
        config_value = param("VM_NETWORKING_TYPE");
        if( !config_value )
        {
            vmprintf( D_ALWAYS,"WARNING: 'VM_NETWORKING' is true but 'VM_NETWORKING_TYPE' is not defined, so 'VM_NETWORKING' is disabled");
            m_VM_NETWORKING = false;
        }
        else
        {
            MyString networking_type = delete_quotation_marks(config_value);

            networking_type.trim();
            networking_type.lower_case();

            free(config_value);

            StringList networking_types(networking_type.Value(), ", ");
            m_VM_NETWORKING_TYPE.create_union(networking_types, false);

            if( m_VM_NETWORKING_TYPE.isEmpty() )
            {
                vmprintf( D_ALWAYS,"WARNING: 'VM_NETWORKING' is true but 'VM_NETWORKING_TYPE' is empty So 'VM_NETWORKING' is disabled\n");
                m_VM_NETWORKING = false;
            }
            else
            {
                config_value = param("VM_NETWORKING_DEFAULT_TYPE");
                if( config_value )
                {
                    m_VM_NETWORKING_DEFAULT_TYPE = (delete_quotation_marks(config_value)).Value();
                    free(config_value);
                }
            }
        }
     }

    return bRet;
}