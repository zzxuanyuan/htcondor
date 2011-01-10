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

#include "kvm.h"

using namespace std;
using namespace boost;
using namespace condor::vmu;

kvm::kvm()
{
    m_szSessionID = "qemu:///session";
}

kvm::~kvm()
{

}

shared_ptr<hypervisor> kvm::manufacture()
{
    shared_ptr<hypervisor> pRet(new kvm());
    return (pRet);
}