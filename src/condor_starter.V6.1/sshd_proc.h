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

#ifndef _CONDOR_SSHD_PROC_H
#define _CONDOR_SSHD_PROC_H

#include "condor_common.h"
#include "condor_classad.h"
#include "vanilla_proc.h"


class SshdProc : public VanillaProc
{
public:

    SshdProc( ClassAd * jobAd );
    virtual ~SshdProc();

/** 
 * 1.) replace the executable
 * 2.) get keys 
 * 3.) set the environment variables
 *     - KEY
 *     - OPEN_SSHD, SSH location
 */
  virtual int StartJob();

  virtual void Suspend();

  /** This is here just so we can print out a log message, since
	we don't expect this will ever get called. */
  virtual void Continue();

  virtual bool PublishUpdateAd( ClassAd* ad );

  inline char * getKeyBaseName(){
	return baseFileName;
  }

private:
  int getKeys();

  int alterExec();

  int alterEnv();  

  char * baseFileName;

  char shadow_contact[100];

  int readStoreFile(Stream * s, char * filename, int mode);

  

};

#endif
