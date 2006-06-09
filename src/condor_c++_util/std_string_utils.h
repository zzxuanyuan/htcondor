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

#ifndef _STD_STRING_UTILS_H_
#define  _STD_STRING_UTILS_H_

#include "condor_common.h"

// sprintf interface to a std::string.  Return the number of characters
// written, or negative value upon error.
int string_vprintf(std::string & s, const char *format, va_list args);
int string_printf(std::string & s, const char *format, ...)
#ifdef __GNUC__
__attribute__((__format__(__printf__, 2, 3)))
#endif
;

// Read the contents of path into buf.  Limit sizeof buf to maxRead.  If
// maxRead is less than 0, the entire contents of path are read into buf.
// Use this value with caution!
// Return the number of bytes read, or -1 upon an error.
int string_readfile(const std::string & path, std::string & buf, int maxRead);

#endif // _STD_STRING_UTILS_H_
