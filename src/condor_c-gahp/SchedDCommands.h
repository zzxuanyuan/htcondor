/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef SCHEDD_COMMANDS_H
#define SCHEDD_COMMANDS_H

#include "condor_common.h"
#include "condor_classad.h"

/**
 *
 * This class represents a simple data structure for
 * representing a job request to the SchedD
 *
 * Use one of the static methods to construct an instance
 *
 **/

class SchedDRequest {
public:
	static SchedDRequest * createRemoveRequest (const int request_id,
												const int cluster_id,
												const int proc_id);

	static SchedDRequest * createHoldRequest (const int request_id,
												const int cluster_id,
												const int proc_id);

	static SchedDRequest * createReleaseRequest (const int request_id,
												const int cluster_id,
												const int proc_id);

	static SchedDRequest * createStatusConstrainedRequest (const int request_id,
															const char * constraint);

	static SchedDRequest * createUpdateConstrainedRequest (const int request_id,
															const char * constraint,
															const ClassAd * classad);


	static SchedDRequest * createUpdateRequest (const int request_id,
													const int cluster_id,
													const int proc_id,
													const ClassAd * classad);

	static SchedDRequest * createSubmitRequest (const int request_id,
													const ClassAd * classad);

	~SchedDRequest() {
		if (classad)
			delete classad;
		if (constraint)
			free (constraint);
	}

	ClassAd * classad;
	char * constraint;
	int cluster_id;
	int proc_id;

	int request_id;

	// Status of the command
	enum {
		SDCS_NEW,
		SDCS_PENDING,
		SDCS_COMPLETED,
		SDCS_ERROR,
	} status;

	// Command being sent
	enum {
		SDC_NOOP,
		SDC_ERROR,
		SDC_REMOVE_JOB,
		SDC_HOLD_JOB,
		SDC_RELEASE_JOB,
		SDC_SUBMIT_JOB,
		SDC_COMPLETE_JOB,
		SDC_STATUS_CONSTRAINED,
		SDC_UPDATE_CONSTRAINED,
		SDC_UPDATE_JOB,
	} command;

protected:
	SchedDRequest() {
	}

};

#endif
