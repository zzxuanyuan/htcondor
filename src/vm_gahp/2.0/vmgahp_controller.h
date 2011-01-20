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

#ifndef VMGAHP_CONTROLLER
#define VMGAHP_CONTROLLER

#include "compat_classad.h"
#include "hypervisor.h"
#include <vector>
#include <string>

namespace condor
{
    namespace vmu
    {

        /**
         * The following is meant to encapsulate the external dependencies used
         * by daemons in condor, and operate on the interface to hypervisor.
         *
         * @author Timothy St. Clair
         */
        class vmgahp_controller : public Service
        {
        public:
            vmgahp_controller();
            virtual ~vmgahp_controller();

            /**
             * init() - initialize the controller, which basically means
             * test privsep && initialize daemoncore info.
             */
            virtual int init();

            /**
             * discover
             */
            virtual int discover( const std::vector< std::string >& vTypes  );

            /**
             * used to pull in config params, depending on the nature
             * of some params they may or may not be able to reconfig
             */
           virtual int config( );

           /**
            * Will spawn the vm given the input params
            */
           virtual int start( const char * pszVMType, const char * pszWorkingDir );

           /**
           */
           virtual int fini();

        protected:

            /**
             * init_uids() - Will validate that the process can switch
             * uids during the course of normal operation.
             */
            int init_uids();

            /**
             * waitForCommand() - is a registered function with daemoncore which
             * will get called when there is GAHP command passed via stdin
             * to the vm-gahp
             *
             * @return 0
             */
            int waitForCommand();

            /**
             * unmarshall() - will parse an input buffer and execute registered
             * gahp commands.
             *
             * @return the number of commands parsed.
             */
            int unmarshall(char * pszBuffer);

            /**
             */
            bool dispatch(pReq);

            ///< ptr to the hypervisior, starter : virt, so no need for a list 1:1
            boost::shared_ptr<hypervisor> m_hypervisor;

            ///< config knobs for hypervisors.
            boost::shared_ptr<hypv_config> m_hyp_config_params;

            boost::shared_ptr<ClassAd> m_jobAd;

            ///< stdout pipe to daemoncore parent process (startd or starter)
            int m_stdout_pipe;

             ///< stdin pipe to daemoncore parent process (starter)
            int m_stdin_pipe;

            ///< used for GAHP communications mode.
            bool m_bAsync_mode;

            ///< used to determine
            std::vector< shared_ptr<GahpRequest> > m_vAsyncResults;

        };

    }
}
#endif
