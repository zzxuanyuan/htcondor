/*
 * Copyright 2009-2011 Red Hat, Inc.
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

// condor includes
#include "condor_common.h"
#include "condor_config.h"
#include "../condor_collector.V6/CollectorPlugin.h"
#include "hashkey.h"
#include "../condor_collector.V6/collector.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// local includes
#include "AviaryUtils.h"
#include "AviaryProvider.h"
#include "AviaryCollectorServiceSkeleton.h"

using namespace std;
using namespace aviary::util;
using namespace aviary::transport;
using namespace aviary::collector;

AviaryProvider* provider = NULL;

struct AviaryCollectorPlugin : public Service, CollectorPlugin
{

    void
    initialize()
    {
        char *tmp;
        string collName;
        
        dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Initializing...\n");
        
        tmp = param("COLLECTOR_NAME");
        if (NULL == tmp) {
            collName = getPoolName();
        } else {
            collName = tmp;
            free(tmp); tmp = NULL;
        }
        
        string log_name;
        sprintf(log_name,"aviary_collector.log");
        provider = AviaryProviderFactory::create(log_name, getPoolName(),COLLECTOR,"", "services/collector/");
        if (!provider) {
            EXCEPT("Unable to configure AviaryProvider. Exiting...");
        }
        
        ReliSock *sock = new ReliSock;
        if (!sock) {
            EXCEPT("Failed to allocate transport socket");
        }
        
        if (!sock->assign(provider->getListenerSocket())) {
            EXCEPT("Failed to bind transport socket");
        }
        int index;
        if (-1 == (index =
            daemonCore->Register_Socket((Stream *) sock,
                                        "Aviary Method Socket",
                                        (SocketHandlercpp) ( &AviaryCollectorPlugin::handleTransportSocket ),
                                        "Handler for Aviary Methods.", this))) {
            EXCEPT("Failed to register transport socket");
                                        }
    }
    
    void invalidate_all() {
    }
    
    void
    shutdown()
    {
        dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: shutting down...\n");
        invalidate_all();
    }
    
    void
    update(int command, const ClassAd &ad)
    {
        MyString name;
        AdNameHashKey hashKey;
        
        switch (command) {
            case UPDATE_STARTD_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_STARTD_AD\n");
                if (param_boolean("AVIARY_IGNORE_UPDATE_STARTD_AD", true)) {
                    dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Configured to ignore UPDATE_STARTD_AD\n");
                    break;
                }
                
                if (!makeStartdAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                }
                
                if (startdAds->lookup(hashKey, slotObject)) {
                    // Key doesn't exist
                    slotObject = new SlotObject(singleton->getInstance(),
                                                hashKey.name.Value());
                    
                    // Ignore old value, if it existed (returned)
                    startdAds->insert(hashKey, slotObject);
                }
                
                slotObject->update(ad);
                
                break;
            case UPDATE_NEGOTIATOR_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_NEGOTIATOR_AD\n");
                if (param_boolean("AVIARY_IGNORE_UPDATE_NEGOTIATOR_AD", true)) {
                    dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Configured to ignore UPDATE_NEGOTIATOR_AD\n");
                    break;
                }
                
                if (!makeNegotiatorAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                }
                
                if (negotiatorAds->lookup(hashKey, negotiatorObject)) {
                    // Key doesn't exist
                    if (!ad.LookupString(ATTR_NAME, name)) {
                        name = "UNKNOWN";
                    }
                    name.sprintf("Negotiator: %s", hashKey.name.Value());
                    
                    negotiatorObject =
                    new NegotiatorObject(singleton->getInstance(),
                                         name.Value());
                    
                    // Ignore old value, if it existed (returned)
                    negotiatorAds->insert(hashKey, negotiatorObject);
                }
                
                negotiatorObject->update(ad);
                
                break;
            case UPDATE_SCHEDD_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_SCHEDD_AD\n");
                if (param_boolean("AVIARY_IGNORE_UPDATE_SCHEDD_AD", true)) {
                    dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Configured to ignore UPDATE_SCHEDD_AD\n");
                    break;
                }
                
                if (!makeScheddAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                }
                
                // The JobServer constructs a ref to the Scheduler
                // based on this Schedd's name, thus we must construct
                // the Scheduler's id in the same way or a disconnect
                // will occur.
                if (!ad.LookupString(ATTR_NAME, name)) {
                    name = "UNKNOWN";
                }
                
                if (schedulerAds->lookup(hashKey, schedulerObject)) {
                    // Key doesn't exist
                    schedulerObject =
                    new SchedulerObject(singleton->getInstance(),
                                        name.Value());
                    
                    // Ignore old value, if it existed (returned)
                    schedulerAds->insert(hashKey, schedulerObject);
                }
                
                schedulerObject->update(ad);
                
                break;
            case UPDATE_GRID_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_GRID_AD\n");
                
                if (!makeGridAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                }
                
                if (gridAds->lookup(hashKey, gridObject)) {
                    // Key doesn't exist
                    gridObject = new GridObject(singleton->getInstance(), hashKey.name.Value());
                    
                    // Ignore old value, if it existed (returned)
                    gridAds->insert(hashKey, gridObject);
                }
                
                gridObject->update(ad);
                
                break;
            case UPDATE_COLLECTOR_AD:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received UPDATE_COLLECTOR_AD\n");
                // We could receive collector ads from many
                // collectors, but we only maintain our own. So,
                // ignore all others.
                char *str;
                if (ad.LookupString(ATTR_MY_ADDRESS, &str)) {
                    string public_addr(str);
                    free(str);
                    
                    if (((Collector *)collector->GetManagementObject())->get_MyAddress() == public_addr) {
                        collector->update(ad);
                    }
                }
                break;
            default:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Unsupported command: %s\n",
                        getCollectorCommandString(command));
        }
    }
    
    void
    invalidate(int command, const ClassAd &ad)
    {
        AdNameHashKey hashKey;
        
        switch (command) {
            case INVALIDATE_STARTD_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received INVALIDATE_STARTD_ADS\n");
                if (!makeStartdAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                    return;
                }
                if (0 == startdAds->lookup(hashKey, slotObject)) {
                    startdAds->remove(hashKey);
                    delete slotObject;
                }
                else {
                    dprintf(D_FULLDEBUG, "%s startd key not found for removal\n",HashString(hashKey).Value());
                }
                break;
            case INVALIDATE_NEGOTIATOR_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received INVALIDATE_NEGOTIATOR_ADS\n");
                if (!makeNegotiatorAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                    return;
                }
                if (0 == negotiatorAds->lookup(hashKey, negotaitorObject)) {
                    negotiatorAds->remove(hashKey);
                    delete negotaitorObject;
                }
                else {
                    dprintf(D_FULLDEBUG, "%s negotiator key not found for removal\n",HashString(hashKey).Value());
                }
                break;
            case INVALIDATE_SCHEDD_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received INVALIDATE_SCHEDD_ADS\n");
                if (!makeScheddAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                    return;
                }
                if (0 == schedulerAds->lookup(hashKey, schedulerObject)) {
                    schedulerAds->remove(hashKey);
                    delete schedulerObject;
                }
                else {
                    dprintf(D_FULLDEBUG, "%s scheduler key not found for removal\n",HashString(hashKey).Value());
                }
                break;
            case INVALIDATE_GRID_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received INVALIDATE_GRID_ADS\n");
                if (!makeGridAdHashKey(hashKey, ((ClassAd *) &ad))) {
                    dprintf(D_FULLDEBUG, "Could not make hashkey -- ignoring ad\n");
                    return;
                }
                if (0 == gridAds->lookup(hashKey, gridObject)) {
                    gridAds->remove(hashKey);
                    delete gridObject;
                }
                else {
                    dprintf(D_FULLDEBUG, "%s grid key not found for removal\n",HashString(hashKey).Value());
                }
                break;
            case INVALIDATE_COLLECTOR_ADS:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Received INVALIDATE_COLLECTOR_ADS\n");
                break;
            default:
                dprintf(D_FULLDEBUG, "AviaryCollectorPlugin: Unsupported command: %s\n",
                        getCollectorCommandString(command));
        }
    }
    
    int
    handleTransportSocket(Stream *)
    {
        string provider_error;
        if (!provider->processRequest(provider_error)) {
            dprintf (D_ALWAYS,"Error processing request: %s\n",provider_error.c_str());
        }
        return KEEP_STREAM;
    }
};

static AviaryCollectorPlugin instance;
