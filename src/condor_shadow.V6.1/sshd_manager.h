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

#ifndef SSHD_MANAGER_H
#define SSHD_MANAGER_H

#include "condor_common.h"
#include "extArray.h"

#include "sshd_info.h"

class SshdManager;  // to avoid dangling loop
#include "baseshadow.h"

class SshdManager : public Service {
public:
  SshdManager(BaseShadow * shadow);

  ~SshdManager();
  
  // from sshd_proc 
  //    return (filename. length, and file) x2
  int getKeys( int cmd, Stream * s);


  // from condor_sshd
  //    get SshInfo.
  int putInfo( int cmd, Stream* s );

  // from condor_ssh
  //    get    hostname
  //    return SshdInfo
  int getInfo( int cmd, Stream * s);

  // from condor_ssh
  //    return SshdInfo
  int getInfoAny( int cmd, Stream * s);

  // from parallel_master
  //    return number of registered info
  int getNumber( int cmd, Stream *);

  //
  //  delete files
  void cleanUp(void);

private:
  ExtArray<SshdInfo *> sshdInfoList;

  bool hasKey;
  char keyFileNameBase[_POSIX_PATH_MAX];
  char keyFileName[_POSIX_PATH_MAX];
  char pubkeyFileName[_POSIX_PATH_MAX];
  char contactFileName[_POSIX_PATH_MAX];

  BaseShadow * shadow;

  int createKeyFiles();
  void deleteFile(char * fileName);

  int sendFile( Stream *s, char * filename);

  int createContactFile();
};

#endif

