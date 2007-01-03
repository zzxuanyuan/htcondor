/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "DBMSManager.h"
#include "condor_config.h"
#include "dc_collector.h"
#include "get_daemon_name.h"

DBMSManager::DBMSManager() {
	m_collectors = NULL;
	m_public_ad_update_timer = -1;
	m_public_ad_update_interval = -1;
}

DBMSManager::~DBMSManager() {
	if(m_collectors) {
		delete m_collectors;
	}
}

void
DBMSManager::init() {
	m_collectors = CollectorList::create();
	config();
}

void
DBMSManager::stop() {
	InvalidatePublicAd();
}

void
DBMSManager::InitPublicAd() {
	m_public_ad = ClassAd();

	//TODO: this is the wrong ADTYPE.
	//Use the new generic ad type when it becomes available.
	//Until then, actual writes to the collector are disabled,
	//because it conflicts with the schedd on the same host..

	m_public_ad.SetMyTypeName( SCHEDD_ADTYPE );
	m_public_ad.SetTargetTypeName( "" );

	m_public_ad.Assign(ATTR_MACHINE,my_full_hostname());
	m_public_ad.Assign(ATTR_NAME,m_name.GetCStr());

	config_fill_ad( &m_public_ad );
}

void
DBMSManager::config() {
	char *name = param("DBMSMANAGER_NAME");
	if(name) {
		char *valid_name = build_valid_daemon_name(name);
		m_name = valid_name;
		free(name);
		delete [] valid_name;
	}
	else {
		char *default_name = default_daemon_name();
		if(default_name) {
			m_name = default_name;
			delete [] default_name;
		}
	}

	InitPublicAd();

	int update_interval = 60;
	char *update_interval_str = param("UPDATE_INTERVAL");
	if(update_interval_str) {
		update_interval = atoi(update_interval_str);
	}
	if(m_public_ad_update_interval != update_interval) {
		m_public_ad_update_interval = update_interval;

		if(m_public_ad_update_timer >= 0) {
			daemonCore->Cancel_Timer(m_public_ad_update_timer);
			m_public_ad_update_timer = -1;
		}
		dprintf(D_FULLDEBUG, "Setting update interval to %d\n",
				m_public_ad_update_interval);
		m_public_ad_update_timer = daemonCore->Register_Timer(
			0,
			m_public_ad_update_interval,
			(Eventcpp)&DBMSManager::TimerHandler_UpdateCollector,
			"DBMSManager::TimerHandler_UpdateCollector",
			this);
	}
}

void
DBMSManager::TimerHandler_UpdateCollector() {
	//m_collectors->sendUpdates(UPDATE_SCHEDD_AD, &m_public_ad);
}

void
DBMSManager::InvalidatePublicAd() {
	//m_collectors->sendUpdates(INVALIDATE_SCHEDD_ADS, &m_public_ad);
}
