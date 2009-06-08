/***************************************************************
 *
 * Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "internet.h"

#include "ReplicatorTransferer.h"

ReplicatorTransfererList::ReplicatorTransfererList( void )
{
}

ReplicatorTransfererList::~ReplicatorTransfererList( void )
{
	clear( );
}

bool
ReplicatorTransfererList::clear( void )
{
	m_list.clear();
}

bool
ReplicatorTransfererList::Register( ReplicatorTransferer &transferer )
{
	if ( find(transferer) ) {
		return true;
	}
	m_list.push_back( &transferer );
	return true;
}

ReplicatorTransferer *
ReplicatorTransfererList::find( int pid )
{
	list <ReplicatorTransferer *>::iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( *trans == pid ) {
			return trans;
		}
	}
	return NULL;
}

int
ReplicatorTransfererList::numActive( void )
{
	int		num = 0;
	list <ReplicatorTransferer *>::iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( trans->isActive() ) {
			num++;
		}
	}
	return num;

}
