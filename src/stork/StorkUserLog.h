/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
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

// Stork user logging interfaces.

#ifndef __STORK_USER_LOG_H__
#define __STORK_USER_LOG_H__

#include "condor_common.h"
#include "StorkJobId.h"
#include "user_log.c++.h"

#ifndef WANT_NAMESPACES
#define WANT_NAMESPACES
#endif
#include "classad_distribution.h"

class StorkUserLog
{
	public:
		/// Constructor
		StorkUserLog( void );
		StorkUserLog(const classad::ClassAd* ad);

		/// Destructor
		~StorkUserLog(void);

		/// assignment
		void assign( const classad::ClassAd* ad );

		/// = operator
		StorkUserLog& operator=(const classad::ClassAd* ad);

		// Input error indicator.
		bool error(void) const { return _error; }

		// Input error message.  Only valid if error==false.
		const char* errorMsg(void) const { return _errorMsg.c_str(); }

		/// Job submit event
		bool submitEvent(void);

		/// Job execute event
		bool executeEvent(void);

		/// Job generic event
		bool genericEvent( std::string text );

		/// Job terminated event
		bool terminateEvent( void );

		/// Job aborted (removed) event
		bool abortEvent( void );

	private:

		/// Initialize
		void initialize(void);

		// Private member data.

		/// Job ClassAd
		const classad::ClassAd*		_ad;

		/// User logging enabled.
		bool						_enable;

		/// Job user log file.
		std::string					_log;

		/// log in XML format
		bool						_logXML;

		/// UserLog object.
		UserLog						_userLog;

		/// Job id.
		StorkJobId					_jobId;

		/// Job owner.
		std::string					_owner;

		/// Cluster id
		int							_cluster;

		/// Proc id
		int							_proc;

		/// subproc id
		int							_subproc;

		/// error indicateor
		bool						_error;

		/// Last error message;
		std::string					_errorMsg;
};

#endif // __STORK_USER_LOG_H__

