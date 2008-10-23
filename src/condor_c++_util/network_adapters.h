/***************************************************************
*
* Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

#ifndef _NETWORK_ADAPTERS_BASE_H_
#define _NETWORK_ADAPTERS_BASE_H_

/***************************************************************
 * Headers
 ***************************************************************/

#include "MyString.h"
#include "condor_classad.h"
#include "network_adapter.h"

/***************************************************************
 * NetworkAdaptersBase class
 ***************************************************************/

class NetworkAdaptersBase
{

public:

    /// Constructor
	NetworkAdaptersBase (void) throw ();

    /// Destructor
	virtual ~NetworkAdaptersBase (void) throw (); 

	//@}

	/** @name Adapter properties.
	Basic device properties.
	*/
	//@{

	/** Initialize the adapter
		@return true if successful, false if unsuccessful
	*/
	virtual bool initialize( void ) { return true; };

	/** Get number of adapters detected
	 */
	int getCount( void ) const { return adapters.length(); };


  protected:

	
  private:
	extArray<NetworkAdapterBase *>	adapters;
};

#endif // _NETWORK_ADAPTERS_BASE_H_
