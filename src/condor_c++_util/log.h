/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#if !defined(_LOG_H)
#define _LOG_H

/* 
   This defines a base class for logs of data structure operations.  
   The logs are meant to be strictly ascii (for example, no '\0' 
   characters).  A log entry is a line containing a single classad. 
   I.e., a classad followed by a '\n'.  The classad contains an
   integer valued attribute named OpType.  Remaining attributes are
   specific to the operation in question.  The Play() method is defined 
   to perform the operation on the data structure passed in as an 
   argument.  The argument is of type (void *) for generality.
*/

#include "condor_classad.h"

class LogRecord : public ClassAd {
public:
	
	LogRecord();
	virtual ~LogRecord();

	bool Write( Sink * );
	bool Read( Source * );

	virtual bool Check( void* ) { return( true ); };
	virtual bool Play( void* ) { return( true ); };
};

#endif
