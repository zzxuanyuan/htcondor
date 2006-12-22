/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "qmgmt.h"

TransferDaemon::TransferDaemon(MyString fquser, TDMode status)
{
	m_fquser = fquser;
	m_status = status;
}

TransferDaemon::~TransferDaemon()
{
}

void
TransferDaemon::set_fquser(MyString fquser)
{
	m_fquser = fquser;
}

MyString
TransferDaemon::get_fquser(void)
{
	return m_fquser;
}

void
TransferDaemon::set_status(TDMode tds)
{
	m_status = tds;
}

TDMode
TransferDaemon::get_status()
{
	return m_status;
}

void
TransferDaemon::set_sinful(MyString sinful)
{
	m_sinful = sinful;
}

MyString
TransferDaemon::get_sinful()
{
	return m_sinful;
}
