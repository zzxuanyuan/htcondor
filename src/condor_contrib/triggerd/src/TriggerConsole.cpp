/*
 * Copyright 2008 Red Hat, Inc.
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

#include "condor_common.h"
#include "condor_config.h"

#include "TriggerConsole.h"

#include "qmf/Agent.h"
#include "qmf/Data.h"
#include "qmf/ConsoleEvent.h"
#include "qpid/messaging/Duration.h"

#include <iostream>

using namespace qpid::console;


TriggerConsole::TriggerConsole()
{
   sm = NULL;
   broker = NULL;
}


TriggerConsole::~TriggerConsole()
{
   if (false == param_boolean("QMF_DELETE_ON_SHUTDOWN", true))
   {
      return;
   }

   qmf2Session.close();
   qpidConnection.close();

   if (NULL != broker)
   {
      dprintf(D_FULLDEBUG, "Triggerd: Deleting broker object\n");
      sm->delBroker(broker);
      dprintf(D_FULLDEBUG, "Triggerd: Deleted broker object\n");
      broker = NULL;
   }
   if (NULL != sm)
   {
      dprintf(D_FULLDEBUG, "Triggerd: Deleting session manager object\n");
      delete sm;
      dprintf(D_FULLDEBUG, "Triggerd: Deleted session manager object\n");
      sm = NULL;
   }
}


void
TriggerConsole::config(std::string host, int port, std::string user, std::string passwd, std::string mech)
{
   qpid::client::ConnectionSettings settings;
   SessionManager::Settings sm_settings;

   sm_settings.rcvObjects = false;
   sm_settings.rcvEvents = false;
   sm_settings.rcvHeartbeats = false;
   sm_settings.userBindings = false;

   settings.host = host;
   settings.port = port;
   settings.username = user;
   settings.password = passwd;
   settings.mechanism = mech;

   if (NULL == sm)
   {
      sm = new SessionManager(this, sm_settings);
   }

   if (NULL != sm)
   {
      if (NULL != broker)
      {
         sm->delBroker(broker);
         broker = NULL;
      }
      broker = sm->addBroker(settings);
   }

   // Set up a QMFv2 Session to query masters
   std::stringstream url;
   std::stringstream options;

   url << host << ":" << port;
   options << "{reconnect:True"; 
   if (!user.empty())
      options << ", username:'" << user << "', password:'" << passwd << "'";
   options << "}";

   qpidConnection = qpid::messaging::Connection(url.str(), options.str());
   qpidConnection.open();

   qmf2Session = qmf::ConsoleSession(qpidConnection);
   qmf2Session.open();
   qmf2Session.setAgentFilter("[and, [eq, _vendor, [quote, 'com.redhat.grid']], [eq, _product, [quote, 'master']]]");
}


std::list<std::string>
TriggerConsole::findAbsentNodes()
{
   Object::Vector list;
   std::set<std::string> nodes_in_pool;
   std::list<std::string> missing_nodes;
   uint64_t timeout(30);

   // Drain the queue of pending console events.
   qmf::ConsoleEvent event;
   while (qmf2Session.nextEvent(event, qpid::messaging::Duration::IMMEDIATE));

   // Traverse the list of master agents (already constrained by the agent
   // filter set in 'config') to collect master objects.  Make asynchronous
   // queries so we don't get hung up on one or more unresponsive agents.
   // We can then have a single timeout interval for the entire set of queries.
   uint32_t agentCount(qmf2Session.getAgentCount());
   for (uint32_t idx = 0; idx < agentCount; idx++) {
       qmf::Agent agent(qmf2Session.getAgent(idx));
       agent.queryAsync("{class:master, package:'com.redhat.grid'}");
   }

   // Get the list of Master objects, which represent each condor node
   // currently running
   uint32_t responsesCollected(0);
   while (qmf2Session.nextEvent(event, qpid::messaging::Duration::SECOND * timeout)) {
       if (event.getType() == qmf::CONSOLE_QUERY_RESPONSE) {
           if (event.getDataCount() > 0) {
               qmf::Data master(event.getData(0));
               nodes_in_pool.insert(master.getProperty("Machine").asString());
           }

           if (++responsesCollected == agentCount)
               break;
       }
   }

   dprintf(D_FULLDEBUG, "Triggerd: Found %d nodes in the pool\n", (int)nodes_in_pool.size());

   // Get the list of nodes configured in the store
   try
   {
      sm->getObjects(list, "Node");
   }
   catch(...)
   {
      dprintf(D_ALWAYS, "Triggerd Error: Failed to contact AMQP broker.  Unable to find nodes expected to be in the pool\n");
      dprintf(D_ALWAYS, "Triggerd: Check for absent nodes aborted\n");
      return missing_nodes;
   }

   if (list.size() > 0)
   {
      std::string name;

      dprintf(D_FULLDEBUG, "Triggerd: %d nodes expected to be in the pool\n", (int)list.size());
      for (Object::Vector::iterator node = list.begin(); node != list.end(); node++)
      {
         name = node->attrString("name");
         if (nodes_in_pool.end() == nodes_in_pool.find(name))
         {
            // This is a missing node
            missing_nodes.push_front(name);
         }
      }
   }

   dprintf(D_FULLDEBUG, "Triggerd: Found %d missing nodes\n", (int)missing_nodes.size());
   return missing_nodes;
}
