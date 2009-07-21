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

// ReplicatorTransferer methods
time_t
ReplicatorTransferer::getAge( time_t now ) const
{
	if ( m_time < 0 ) {
		return 0;
	}
	if ( now == 0 ) {
		now = time(NULL);
	}
	return now - m_time;
}

bool
ReplicatorTransferer::kill( int sig ) const
{
	if ( m_pid > 0 ) {
        return daemonCore->Send_Signal( m_pid, sig );
	};
	return true;
}

// ReplicatorTransfererList methods
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
	return true;
}

bool
ReplicatorTransfererList::Register( ReplicatorTransferer &transferer )
{
	if ( Find(transferer.getPid()) ) {
		return true;
	}
	m_list.push_back( &transferer );
	return true;
}

ReplicatorTransferer *
ReplicatorTransfererList::Find( int pid )
{
	list <ReplicatorTransferer *>::iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( trans->getPid() == pid ) {
			return trans;
		}
	}
	return NULL;
}

int
ReplicatorTransfererList::numActive( void ) const
{
	int		num = 0;
	list <ReplicatorTransferer *>::const_iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( trans->isActive() ) {
			num++;
		}
	}
	return num;
}

bool
ReplicatorTransfererList::killTransList(
	int									 sig,
	const list<ReplicatorTransferer *>	&transferers )
{
	bool	ok = true;
	list <ReplicatorTransferer *>::const_iterator iter;
	for( iter = transferers.begin(); iter != transferers.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( trans->kill(sig) ) {
			ok = false;
		}
	}
	return ok;
}

int
ReplicatorTransfererList::getOldTransList(
	time_t							 maxage,
	list<ReplicatorTransferer *>	&transferers )
{
	int		num = 0;
	list <ReplicatorTransferer *>::iterator iter;
	for( iter = m_list.begin(); iter != m_list.end(); iter++ ) {
		ReplicatorTransferer	*trans = *iter;
		if ( trans->isActive() && (trans->getAge() > maxage) ) {
			transferers.push_back( trans );
			num++;
		}
	}
	return num;
}
