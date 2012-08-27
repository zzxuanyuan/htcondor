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

//condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "condor_debug.h"
#include "condor_commands.h"
#include "hashkey.h"
#include "stl_string_utils.h"

// C++ includes
// enable for debugging classad to ostream
// watch out for unistd clash
//#include <sstream>

//local includes
#include "CollectorObject.h"
#include "AviaryConversionMacros.h"
#include "AviaryUtils.h"

using namespace std;
using namespace aviary::collector;
using namespace aviary::util;

namespace aviary {
namespace collector {

CollectorObject collector;

}}

CollectorObject::CollectorObject ()
{
    //
}

CollectorObject::~CollectorObject()
{
    //
}

string 
CollectorObject::getPool() {
    return getPoolName();
}

void
CollectorObject::update (string name, const ClassAd &ad)
{
    //
}

void
CollectorObject::remove (string name)
{
    //
}
