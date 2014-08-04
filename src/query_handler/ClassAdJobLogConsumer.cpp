/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "compat_classad_util.h"

#include "ClassAdJobLogConsumer.h"

#include <string>

ClassAdJobLogConsumer::ClassAdJobLogConsumer()
	: m_reader(NULL)
{
}

void
ClassAdJobLogConsumer::Reset()
{
	m_collection.DeleteCollection(0);
}

bool
ClassAdJobLogConsumer::NewClassAd(const char *key, const char *mytype, const char *target)
{
	dprintf(D_FULLDEBUG, "Found new classad: %s.\n", key);
	ClassAd* ad = NULL;

	if (!m_collection.LookupClassAd(key, ad)) {
                if (!m_collection.NewClassAd(key, mytype, target)) {
                        dprintf(D_ALWAYS,
                                "error processing %s: failed to add '%s' to "
                                "ClassAd collection.\n",
                                m_reader ? m_reader->GetClassAdLogFileName() : "(null)",
                                key);
                        return false;
                }
		m_collection.LookupClassAd(key, ad);
                dprintf(D_FULLDEBUG, "Creating new cluster ad %s\n", key);
	}

		// Chain this ad to its parent, if any.
	PROC_ID proc = getProcByString(key);
	if(proc.proc >= 0) {
		char cluster_key[PROC_ID_STR_BUFLEN];
			// NOTE: cluster keys start with a 0: e.g. 021.-1
		sprintf(cluster_key,"0%d.-1", proc.cluster);
		ClassAd* cluster_ad = NULL;
		if (!m_collection.LookupClassAd(cluster_key, cluster_ad))
		{
			// The cluster ad doesn't exist yet.  This is expected.
			// For example, once the job queue is rewritten (i.e.
			// truncated), the order of entries in it is arbitrary.
			if (!m_collection.NewClassAd(cluster_key, mytype, target)) {
				dprintf(D_ALWAYS,
					"error processing %s: failed to add '%s' to "
					"ClassAd collection.\n",
				m_reader ? m_reader->GetClassAdLogFileName() : "(null)", cluster_key);
				delete ad;
				return true; // XXX: why is this ok?
			}
			m_collection.LookupClassAd(cluster_key, cluster_ad);
			dprintf(D_FULLDEBUG, "Creating cluster ad %s\n", cluster_key);
		}
		dprintf(D_FULLDEBUG, "Chaining ad %p to cluster\n", cluster_ad);
                ad->ChainToAd(cluster_ad);
        }

        return true;
}

bool
ClassAdJobLogConsumer::DestroyClassAd(const char *key)
{
	return m_collection.DestroyClassAd(key);
}

bool
ClassAdJobLogConsumer::SetAttribute(const char *key, const char *name, const char *value)
{
	dprintf(D_FULLDEBUG, "New attribute for %s: %s=%s\n", key, name, value);
	return m_collection.SetAttribute(key, name, value);
}

bool
ClassAdJobLogConsumer::DeleteAttribute(const char *key,
										  const char *name)
{
	return m_collection.DeleteAttribute(key, name);
}

