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


#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "syscall_numbers.h"
#include "condor_debug.h"
#include "condor_file_info.h"
static char *_FileName_ = __FILE__;

int open_file_stream( const char *local_path, int flags, size_t *len );
/*
  Open a stream for writing our checkpoint information.  Since we are in
  the "remote" startup file, this is the remote version.  We do it with
  a "pseudo system call" to the shadow.
*/
int
open_ckpt_file( const char *name, int flags, size_t n_bytes )
{
	char			file_name[ _POSIX_PATH_MAX ];
	int				status;

	return open_file_stream( name, flags, &n_bytes );
}

void
report_image_size( int kbytes )
{
	dprintf( D_ALWAYS, "Sending Image Size Report of %d kilobytes\n", kbytes );
	REMOTE_syscall( CONDOR_image_size, kbytes );
}

/*
  After we have updated our image size and rusage, we ask the shadow
  for a bitmask which specifies checkpointing options, defined in
  condor_includes/condor_ckpt_mode.h.  A return of -1 or 0 signifies
  that all default values should be used.  Thus, if the shadow does not
  support this call, the job will checkpoint with default options.  The
  job sends the signal which triggered the checkpoint so the shadow
  knows if this is a periodic or vacate checkpoint.
*/
int
get_ckpt_mode( int sig )
{
	return REMOTE_syscall( CONDOR_get_ckpt_mode, sig );
}

/*
  If the shadow tells us to checkpoint slowly in get_ckpt_mode(), we need
  to ask for a speed.  The return value is in KB/s.
*/
int
get_ckpt_speed()
{
	return REMOTE_syscall( CONDOR_get_ckpt_speed );
}

void
unblock_signals()
{
	sigset_t	sig_mask;
	int			scm;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		/* unblock signals */
	sigfillset( &sig_mask );
	if( sigprocmask(SIG_UNBLOCK,&sig_mask,0) < 0 ) {
		dprintf( D_ALWAYS, "sigprocmask failed in unblock_signals: %s",
				 strerror(errno));
		Suicide();
	}

	SetSyscalls( scm );

	dprintf( D_ALWAYS, "Unblocked all signals\n" );
}

#define UNIT 10000

#if defined(ALPHA)
#	define LIM (2000 * UNIT)
#elif defined(AIX32)
#	define LIM (225 * UNIT)
#elif defined(SPARC)
#	define LIM (260 * UNIT)
#elif defined(ULTRIX43)
#	define LIM (170 * UNIT)
#elif defined(HPPAR)
#	define LIM (260 * UNIT)
#elif defined(LINUX)
#	define LIM (200 * UNIT)
#elif defined(Solaris)
#	define LIM (260 * UNIT)
#elif defined(SGI)
#	define LIM (200 * UNIT)
#endif

#if 1
	delay()
	{
		int		i;

		for( i=0; i<LIM; i++ )
			;
	}
#else
	delay(){}
#endif
#define B_NET(x) (((long)(x)&IN_CLASSB_NET)>>IN_CLASSB_NSHIFT)
#define B_HOST(x) ((long)(x)&IN_CLASSB_HOST)
#define HI(x) (((long)(x)&0xff00)>>8)
#define LO(x) ((long)(x)&0xff)


void
display_ip_addr( unsigned int addr )
{
	int		net_part;
	int		host_part;

	if( IN_CLASSB(addr) ) {
		net_part = B_NET(addr);
		host_part = B_HOST(addr);
		dprintf( D_FULLDEBUG, "%d.%d", HI(B_NET(addr)), LO(B_NET(addr)) );
		dprintf( D_FULLDEBUG, ".%d.%d\n", HI(B_HOST(addr)), LO(B_HOST(addr)) );
	} else {
		dprintf( D_FULLDEBUG, "0x%x\n", addr );
	}
}

/*
  Open a standard file (0, 1, or 2), given its fd number.
*/
void
open_std_file( int which )
{
	char	name[ _POSIX_PATH_MAX ];
	char	buf[ _POSIX_PATH_MAX + 50 ];
	int		pipe_fd;
	int		answer;
	int		status;

		/* The ckpt layer assumes the process is attached to a terminal,
		   so these are "pre_opened" in our open file table.  Here we must
		   get rid of those entries so we can open them properly for
		   remotely running jobs.
		*/
	close( which );

	status =  REMOTE_syscall( CONDOR_std_file_info, which, name, &pipe_fd );
	if( status == IS_PRE_OPEN ) {
		answer = pipe_fd;			/* it's a pipe */
	} else {
		switch( which ) {			/* it's an ordinary file */
		  case 0:
			answer = open( name, O_RDONLY, 0 );
			break;
		  case 1:
		  case 2:
			answer = open( name, O_WRONLY, 0 );
			break;
		}
	}
	if( answer < 0 ) {
		sprintf( buf, "Can't open \"%s\"", name );
		REMOTE_syscall(CONDOR_perm_error, buf );
		dprintf( D_ALWAYS, buf );
		Suicide();
	} else {
		if( answer != which ) {
			dup2( answer, which );
		}
	}
}

void
set_iwd()
{
	char	iwd[ _POSIX_PATH_MAX ];
	char	buf[ _POSIX_PATH_MAX + 50 ];

	if( REMOTE_syscall(CONDOR_get_iwd,iwd) < 0 ) {
		REMOTE_syscall(
			CONDOR_perm_error,
			"Can't determine initial working directory"
		);
		dprintf( D_ALWAYS, "Can't determine initial working directory\n" );
		Suicide();
	}
	if( REMOTE_syscall(CONDOR_chdir,iwd) < 0 ) {
		sprintf( buf, "Can't open working directory \"%s\"", iwd );
		REMOTE_syscall( CONDOR_perm_error, buf );
		dprintf( D_ALWAYS, "Can't chdir(%s)\n", iwd );
		Suicide();
	}
	Set_CWD( iwd );
}

void
get_ckpt_name()
{
	char	ckpt_name[ _POSIX_PATH_MAX ];
	int		status;

	status = REMOTE_syscall( CONDOR_get_ckpt_name, ckpt_name );
	if( status < 0 ) {
		dprintf( D_ALWAYS, "Can't get checkpoint file name!\n" );
		Suicide(0);
	}
	dprintf( D_ALWAYS, "Checkpoint file name is \"%s\"\n", ckpt_name );
	init_image_with_file_name( ckpt_name );
}

#if 0
void
debug_msg( const char *msg )
{
	int		status;

	status = syscall( SYS_write, DebugFd, msg, strlen(msg) );
	if( status < 0 ) {
		exit( errno );
	}
}
#endif
