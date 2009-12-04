/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
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

#ifndef CONDOR_MACROS_H
#define CONDOR_MACROS_H

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

	/* When using pthreads, pthread_sigmask() in place of sigprocmask(),
	 * except in the ckpt or remote syscall code (which doesn't deal
	 * with pthreads anyhow).
	 */
#if !defined(IN_CKPT_LIB) && !defined(REMOTE_SYSCALLS) && defined(HAVE_PTHREAD_SIGMASK)
#	ifdef sigprocmask
#		undef sigprocmask
#	endif
#	define sigprocmask pthread_sigmask
#endif


#endif /* CONDOR_MACROS_H */
