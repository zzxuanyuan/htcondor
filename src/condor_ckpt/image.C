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
#include "condor_mmap.h"

#if defined(Solaris)
#include <netconfig.h>		// for setnetconfig()
#endif

#include "condor_syscalls.h"
#include "condor_sys.h"
#include "image.h"
#include "file_table_interf.h"
#include "file_state.h"		// FileTab pointer
#include "condor_io.h"		// ReliSock pointer
#include "condor_debug.h"
static char *_FileName_ = __FILE__;

extern int _condor_in_file_stream;

// Objects whose position we must record over a ckpt
extern OpenFileTable *FileTab;
extern ReliSock *syscall_sock;

const int KILO = 1024;

extern "C" void report_image_size( int );
extern "C"	int	SYSCALL(int ...);

#ifdef SAVE_SIGSTATE
extern "C" void condor_save_sigstates();
extern "C" void condor_restore_sigstates();
#endif

#if defined(OSF1)
	extern "C" unsigned int htonl( unsigned int );
	extern "C" unsigned int ntohl( unsigned int );
#elif defined(HPUX)
#	include <netinet/in.h>
#elif defined(Solaris) && defined(sun4m)
    #define htonl(x)        (x)
    #define ntohl(x)        (x)
#elif defined(IRIX62)
    #include <sys/endian.h>
#elif defined(LINUX)
    #include <netinet/in.h>
#else
	extern "C" unsigned long htonl( unsigned long );
	extern "C" unsigned long ntohl( unsigned long );
#endif

#if defined(HPUX10)
extern "C" int _sigreturn();
#endif

void terminate_with_sig( int sig );
static void find_stack_location( RAW_ADDR &start, RAW_ADDR &end );
static int SP_in_data_area();
static void calc_stack_to_save();
extern "C" void _install_signal_handler( int sig, SIG_HANDLER handler );
extern "C" int open_ckpt_file( const char *name, int flags, size_t n_bytes );

Image MyImage;
static jmp_buf Env;
static RAW_ADDR SavedStackLoc;
volatile int InRestart = TRUE;
volatile int check_sig;		// the signal which activated the checkpoint; used
							// by some routines to determine if this is a periodic
							// checkpoint (USR2) or a check & vacate (TSTP).
static size_t StackSaveSize;
unsigned int _condor_numrestarts = 0;


/* Descriptors for saving restore debugging data */
static int fdm, fdd;

extern "C" void zeek ();

static void
pr_jmpbuf (jmp_buf buf)
{
     int i;
     unsigned int *p = (unsigned int *) buf;
     for (i = 0; i < sizeof (jmp_buf) / sizeof (unsigned int); i++)
	  dprintf (D_ALWAYS, "ZANDY:	buf[%d] = %#010x\n", i, p[i]);
}


static int
net_read(int fd, void *buf, int size)
{
	int		bytes_read;
	int		this_read;

	bytes_read = 0;
	do {
		this_read = read(fd, buf, size - bytes_read);
		if (this_read < 0) {
			return this_read;
		}
		bytes_read += this_read;
		buf += this_read;
	} while (bytes_read < size);
	return bytes_read;
}

/* Return the start address of the lowest dynamic object in the
   process.  When the process is restarted, any code loaded below this
   address will be safe, in that it will not be clobbered when the
   checkpoint image is restored.

   This is peculiar to Sparc Solaris 2.5.1 and should be moved to
   machdep.

   Assumptions:

   1. Dynamic libraries are loaded into descending addresses
   2. 0xe0000000 is a lower bound on dynamic segments and an upper
      bound on the junk below dynamic segments, like the text, data,
      and heap
   3. The segment map returned by PIOCMAP on /proc is sorted in
      ascending pr_vaddr order
                                                    -zandy 6/19/1998
 */
#include <sys/types.h> 
#include <sys/signal.h>
#include <sys/fault.h> 
#include <sys/syscall.h>
#include <sys/procfs.h>
static caddr_t
start_of_lowest_shlib ()
{
     int pfd;
     prmap_t *pmap;
     int npmap;
     int i;
     char proc[32];
     int scm;

     caddr_t segbound = (caddr_t) 0xe0000000;

     scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);

     /* Open /proc device for this process */
     sprintf (proc, "/proc/%d", getpid ());
     pfd = open (proc, O_RDONLY);
     if (pfd < 0) {
	  dprintf (D_ALWAYS, "Can't open /proc for this process\n");
	  Suicide ();
     }

     /* Get segment map for this process */
     if (ioctl (pfd, PIOCNMAP, &npmap) < 0) {
	  dprintf (D_ALWAYS, "Can't PIOCNMAP myself\n");
	  Suicide ();
     }
     pmap = (prmap_t *) malloc ((npmap + 1) * sizeof (prmap_t));
     if (! pmap) {
	  dprintf (D_ALWAYS, "Out of memory\n");
	  Suicide ();
     }
     if (ioctl (pfd, PIOCMAP, pmap) < 0) {
	  dprintf (D_ALWAYS, "Can't PIOCMAP myself\n");
	  Suicide ();
     }

     /* Find the first segment likely to be (to have once been, in a
        restarted process) a shared object. */
     for (i = 0; i < npmap && pmap[i].pr_vaddr < segbound; i++)
	  ;
     close (pfd);
     free (pmap);
     SetSyscalls (scm);
     if (i < npmap)
	  return pmap[i].pr_vaddr;
     else 
	  return NULL;
}

void
Header::Init()
{
	magic = MAGIC;
	n_segs = 0;
	low_shlib_start = 0;
	addr_of_Env = 0;
	addr_of_FileTab = 0;
	addr_of_syscall_sock = 0;
}

void
Header::Display()
{
	DUMP( " ", magic, 0x%X );
	DUMP( " ", n_segs, %d );
	DUMP( " ", low_shlib_start, 0x%X );
	DUMP( " ", addr_of_Env, 0x%X );
	DUMP( " ", addr_of_FileTab, 0x%X );
	DUMP( " ", addr_of_syscall_sock, 0x%X );
}

void
SegMap::Init( const char *n, RAW_ADDR c, long l, int p )
{
	strcpy( name, n );
	file_loc = -1;
	core_loc = c;
	len = l;
	prot = p;
}

void
SegMap::Display()
{
	DUMP( " ", name, %s );
	printf( " file_loc = %Lu (0x%X)\n", file_loc, file_loc );
	printf( " core_loc = %Lu (0x%X)\n", core_loc, core_loc );
	printf( " len = %d (0x%X)\n", len, len );
}


void
Image::SetFd( int f )
{
	fd = f;
	pos = 0;
}

void
Image::SetFileName( char *ckpt_name )
{
		// Save the checkpoint file name
	file_name = new char [ strlen(ckpt_name) + 1 ];
	strcpy( file_name, ckpt_name );

	fd = -1;
	pos = 0;
}

void
Image::SetMode( int syscall_mode )
{
	if( syscall_mode & SYS_LOCAL ) {
		mode = STANDALONE;
	} else {
		mode = REMOTE;
	}
}

/* Evict the shared library that was loaded by dlopen at startup.
   dlclose is the right way to do this, but we don't have the
   necessary handle on the library.  Instead, munmap the library's
   segments.  This deletes the segments from the address space, which
   is all that matters as far as keeping the process from growing with
   each ckpt, but we don't know if we're not leaving the rt loader in a
   fog by not telling it about the unload, i.e., by not calling
   dlclose.  Are you debugging a problem in a program that uses
   dlopen?  See if dlclose, rather than munmap, is required here.
                                                     -zandy 6/19/1998
 */
int
Image::unloadGangrenousSegments ()
{
     int pfd;
     prmap_t *pmap;
     int npmap;
     int i;
     char proc[32];
     caddr_t segbound = (caddr_t) 0xe0000000;
     int scm;

     scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
     
     /* Open /proc device for this process */
     sprintf (proc, "/proc/%d", getpid ());
     pfd = open (proc, O_RDONLY);
     if (pfd < 0) {
	  dprintf (D_ALWAYS, "Can't open /proc for this process\n");
	  Suicide ();
     }
     
     /* Get segment map for this process */
     if (ioctl (pfd, PIOCNMAP, &npmap) < 0) {
	  dprintf (D_ALWAYS, "Can't PIOCNMAP myself\n");
	  Suicide ();
     }
     pmap = (prmap_t *) malloc ((npmap + 1) * sizeof (prmap_t));
     if (! pmap) {
	  dprintf (D_ALWAYS, "Out of memory\n");
	  Suicide ();
     }
     if (ioctl (pfd, PIOCMAP, pmap) < 0) {
	  dprintf (D_ALWAYS, "Can't PIOCMAP myself\n");
	  Suicide ();
     }
     close (pfd);

     /* Unmap segments at addresses lower than the checkpointed shared
        segments. */
     for (i = 0; i < npmap; i++)
	  if ((RAW_ADDR) pmap[i].pr_vaddr < head.low_shlib_start
	      && pmap[i].pr_vaddr >= segbound) {
	       dprintf (D_ALWAYS,
			"(unloadGangrenousSegments) Unmapping %#010x\n",
			pmap[i].pr_vaddr);
	       if (0 > munmap ((char *) pmap[i].pr_vaddr, pmap[i].pr_size)) {
		    dprintf (D_ALWAYS, 
			     "(unloadGangrenousSegments) munmap failed\n");
		    Suicide ();
	       }
	  }

     SetSyscalls (scm);
     return 0;
}

/*
  These actions must be done on every startup, regardless whether it is
  local or remote, and regardless whether it is an original invocation
  or a restart.
*/
extern "C"
void
_condor_prestart( int syscall_mode )
{
     dprintf (D_ALWAYS, "ZANDY: In prestart\n");
	MyImage.SetMode( syscall_mode );

		// Initialize open files table
	InitFileState();

	calc_stack_to_save();

	/* On the first call to setnetconfig(), space is malloc()'ed to store
	   the net configuration database.  This call is made by Solaris during
	   a socket() call, which we do inside the Checkpoint signal handler.
	   So, we call setnetconfig() here to do the malloc() before we are
	   in the signal handler.  (Doing a malloc() inside a signal handler
	   can have nasty consequences.)  -Jim B.  */
	/* Note that the floating point code below is needed to initialize
	   this process as a process which potentially uses floating point
	   registers.  Otherwise, the initialization is possibly not done
	   on restart, and we will lose floats on context switches.  -Jim B. */
#if defined(Solaris)
	setnetconfig();
#if defined(sun4m)
	float x=23, y=14, z=256;
	if ((x+y)>z) {
		dprintf(D_ALWAYS,
				"Internal error: Solaris floating point test failed\n");
		Suicide();
	}
	z=x*y;
#endif
#endif
}

extern "C" void
_install_signal_handler( int sig, SIG_HANDLER handler )
{
	int		scm;
	struct sigaction action;

	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

#if defined(HPUX10)
	action.sa_sigaction = handler;
#else
	action.sa_handler = handler;
#endif
	sigemptyset( &action.sa_mask );
	/* We do not want "recursive" checkpointing going on.  So block SIGUSR2
		during a SIGTSTP checkpoint, and vice-versa.  -Todd Tannenbaum, 8/95 */
	if ( sig == SIGTSTP )
		sigaddset(&action.sa_mask,SIGUSR2);
	if ( sig == SIGUSR2 )
		sigaddset(&action.sa_mask,SIGTSTP);
	
#if defined(HPUX10)
	action.sa_flags = SA_SIGINFO;	/* so our handler is passed the context */
#else
	action.sa_flags = 0;
#endif

	if( sigaction(sig,&action,NULL) < 0 ) {
		dprintf(D_ALWAYS, "can't install sighandler for sig %d: %s\n",
				sig, strerror(errno));
		Suicide();
	}

	SetSyscalls( scm );
}

/* For now, write the information we need at restart to a common file.
   I don't know how to tell the restarting process where the ckpt is
   without forcing it to connect to the shadow.  -zandy 6/20/1998 */
static void
tell_vic_about_the_ckpt (RAW_ADDR low_shlib_start)
{
     int scm, fd;

     scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
     fd = open ("/common/tmp/zandy/ckptinfo",
		O_CREAT | O_TRUNC | O_WRONLY, 0664);
     if (fd < 0) {
	  dprintf (D_ALWAYS, "ZANDY: Can't open ckptinfo: %d\n",
		   errno);
	  Suicide ();
     }
     if (sizeof (RAW_ADDR) != write (fd, &low_shlib_start,
				     sizeof (RAW_ADDR))) {
	  dprintf (D_ALWAYS, "ZANDY: Error writing ckptinfo\n");
	  Suicide ();
     }
     close (fd);
     SetSyscalls (scm);
}

/*
  Save checkpoint information about our process in the "image" object.  Note:
  this only saves the information in the object.  You must then call the
  Write() method to get the image transferred to a checkpoint file (or
  possibly moved to another process.
*/
void
Image::Save()
{
#if !defined(Solaris) && !defined(LINUX) && !defined(IRIX53)
	RAW_ADDR	stack_start, stack_end;
#else
	RAW_ADDR	addr_start, addr_end;
	int             numsegs, prot, rtn, stackseg;
#endif
	RAW_ADDR	data_start, data_end;
	ssize_t		pos;
	int			i;

	head.Init();

#if !defined(Solaris) && !defined(LINUX) && !defined(IRIX53)

		// Set up data segment
	data_start = data_start_addr();
	data_end = data_end_addr();
	AddSegment( "DATA", data_start, data_end, 0 );

		// Set up stack segment
	find_stack_location( stack_start, stack_end );
	AddSegment( "STACK", stack_start, stack_end, 0 );

#else

	// Note: the order in which the segments are put into the checkpoint
	// file is important.  The DATA segment must be first, followed by
	// all shared library segments, and then followed by the STACK
	// segment.  There are two reasons for this: (1) restoring the 
	// DATA segment requires the use of sbrk(), which is in libc.so; if
	// we mess up libc.so, our calls to sbrk() will fail; (2) we 
	// restore segments in order, and the STACK segment must be restored
	// last so that we can immediately return to user code.  - Jim B.

	numsegs = num_segments();

#if !defined(IRIX53) && !defined(Solaris)

	// data segment is saved and restored as before, using sbrk()
	data_start = data_start_addr();
	data_end = data_end_addr();
	dprintf( D_ALWAYS, "data start = 0x%lx, data end = 0x%lx\n",
			data_start, data_end );
	AddSegment( "DATA", data_start, data_end, 0 );

#else

	// sbrk() doesn't give reliable values on IRIX53 and Solaris
	// use ioctl info instead
	data_start=MAXLONG;
	data_end=0;
	for( i=0; i<numsegs; i++ ) {
		rtn = segment_bounds(i, addr_start, addr_end, prot);
		if (rtn == 3) {
			if (data_start > addr_start)
				data_start = addr_start;
			if (data_end < addr_end)
				data_end = addr_end;
		}
	}
	AddSegment( "DATA", data_start, data_end, prot );
#endif	
	/* Note the beginning of the lowest shared library and the
	   location of the jumpbuf that will store the context
	   captured in Checkpoint().  When restarting, we use these
	   values to temporarily load the restore lib in a safe place,
	   and to later jump from that lib to the one we're currently
	   executing in. -zandy 6/19/1998 */
	head.low_shlib_start = (RAW_ADDR) start_of_lowest_shlib ();
	if (head.low_shlib_start == NULL) {
	     dprintf (D_ALWAYS, "Couldn't find start of lowest shared lib\n");
	     Suicide ();
	} else
	     dprintf (D_ALWAYS,
		      "(Image::Save) %#010x is the lowest shlib start\n",
		      head.low_shlib_start);
	head.addr_of_Env = (RAW_ADDR) &Env;
	dprintf (D_ALWAYS, "ZANDY: Here's the jmpbuf before:\n");
	pr_jmpbuf (Env);
	head.addr_of_FileTab = (RAW_ADDR) FileTab;
	head.addr_of_syscall_sock = (RAW_ADDR) syscall_sock;
	tell_vic_about_the_ckpt (head.low_shlib_start);  // This is temporary

	for( i=0; i<numsegs; i++ ) {
		rtn = segment_bounds(i, addr_start, addr_end, prot);
		switch (rtn) {
		case -1:
			dprintf( D_ALWAYS, "Internal error, segment_bounds returned -1\n");
			Suicide();
			break;
		case 0: case 1:

		     // Used to be only case 0 here.  1 indicates the
		     // text segment, which ordinarily we don't save.
		     // However, the text segment test in
		     // segment_bounds is broken when the condor lib
		     // is dynamically loaded.  When that gets fixed,
		     // undo this to only add in case 0, and restore
		     // case 1 to be similar to case 2.  -zandy 7/8/1998
#if defined(LINUX)
			addr_end=find_correct_vm_addr(addr_start, addr_end, prot);
#endif
			AddSegment( "SHARED LIB", addr_start, addr_end, prot);
			break;
		case 2:
			stackseg = i;	// don't add STACK segment until the end
			break;
		case 3:
			break;		// don't add DATA segment again
		default:
			dprintf( D_ALWAYS, "Internal error, segment_bounds"
					 "returned unrecognized value\n");
			Suicide();
		}
	}	
	// now add stack segment
	rtn = segment_bounds(stackseg, addr_start, addr_end, prot);
	AddSegment( "STACK", addr_start, addr_end, prot);
	dprintf( D_ALWAYS, "stack start = 0x%lx, stack end = 0x%lx\n",
			addr_start, addr_end);
	dprintf( D_ALWAYS, "Current segmap dump follows\n");
	display_prmap();

#endif

		// Calculate positions of segments in ckpt file
	pos = sizeof(Header) + head.N_Segs() * sizeof(SegMap);
	for( i=0; i<head.N_Segs(); i++ ) {
		pos = map[i].SetPos( pos );
	}

	if( pos < 0 ) {
		dprintf( D_ALWAYS, "Internal error, ckpt size calculated is %d\n", pos );
		Suicide();
	}

	dprintf( D_ALWAYS, "Size of ckpt image = %d bytes\n", pos );
	len = pos;

	valid = TRUE;
}

ssize_t
SegMap::SetPos( ssize_t my_pos )
{
	file_loc = my_pos;
	return file_loc + len;
}

void
Image::Display()
{
	int		i;

	printf( "===========\n" );
	printf( "Ckpt File Header:\n" );
	head.Display();
	for( i=0; i<head.N_Segs(); i++ ) {
		printf( "Segment %d:\n", i );
		map[i].Display();
	}
	printf( "===========\n" );
}

void
Image::AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end, int prot )
{
	long	len = end - start;
	int idx = head.N_Segs();

	if( idx >= MAX_SEGS ) {
		dprintf( D_ALWAYS, "Don't know how to grow segment map yet!\n" );
		Suicide();
	}
	head.IncrSegs();
	map[idx].Init( name, start, len, prot );
}

char *
Image::FindSeg( void *addr )
{
	int		i;
	if( !valid ) {
		return NULL;
	}
	for( i=0; i<head.N_Segs(); i++ ) {
		if( map[i].Contains(addr) ) {
			return map[i].GetName();
		}
	}
	return NULL;
}

BOOL
SegMap::Contains( void *addr )
{
	return ((RAW_ADDR)addr >= core_loc) && ((RAW_ADDR)addr < core_loc + len);
}


#define USER_DATA_SIZE 256
#if defined(PVM_CHECKPOINTING)
extern "C" user_restore_pre(char *, int);
extern "C" user_restore_post(char *, int);
char	global_user_data[USER_DATA_SIZE];
#endif

/*
  Given an "image" object containing checkpoint information which we have
  just read in, this method actually effects the restart.
*/
void
Image::Restore()
{
	int		save_fd = fd;
	char	user_data[USER_DATA_SIZE];
	int x, scm;

#if defined(PVM_CHECKPOINTING)
	user_restore_pre(user_data, sizeof(user_data));
#endif

	scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	fdm = open ("/common/tmp/zandy/rest.map",
		    O_CREAT | O_TRUNC | O_WRONLY,
		    0664);
	if (fdm < 0) {
	     dprintf (D_ALWAYS, "ZANDY: Can't open rest.map\n");
	     Suicide ();
	}
	fdd = open ("/common/tmp/zandy/rest.data",
		    O_CREAT | O_TRUNC | O_WRONLY,
		    0664);
	if (fdm < 0) {
	     dprintf (D_ALWAYS, "ZANDY: Can't open rest.data\n");
	     Suicide ();
	}
	dprintf (D_ALWAYS, "ZANDY: fdm is %d (%x)\n", fdm, &fdm);
	SetSyscalls (scm);


		// Overwrite our data segment with the one saved at checkpoint
		// time *and* restore any saved shared libraries.
	RestoreAllSegsExceptStack();

//	dprintf (D_ALWAYS, "ZANDY: Zeekin' it\n");
//	zeek ();
//	dprintf (D_ALWAYS, "ZANDY: Zeeked it\n");

	// We just blew away the heap.  Restore the FileTab and
	// syscall_sock pointers, which point into the heap. (I think
	// there is a typing reason for doing FileTab in another
	// module.) -zandy  7/27/1998
	RestoreFileTab (GetAddrOfFileTab ());
	syscall_sock = (ReliSock *)(GetAddrOfsyscall_sock ());

		// We have just overwritten our data segment, so the image
		// we are working with has been overwritten too.  Fortunately,
		// the only thing that has changed is the file descriptor.
	fd = save_fd;

#if defined(PVM_CHECKPOINTING)
	memcpy(global_user_data, user_data, sizeof(user_data));
#endif

	dprintf (D_ALWAYS, "ZANDY: About to tmpstk it\n");

		// Now we're going to restore the stack, so we move our execution
		// stack to a temporary area (in the data segment), then call
		// the RestoreStack() routine.
	ExecuteOnTmpStk( RestoreStack );

		// RestoreStack() also does the jump back to user code
	dprintf( D_ALWAYS, "Error: reached code past the restore point!\n" );
	Suicide();
}

/* don't assume we have libc.so in a good state right now... - Jim B. */
static int mystrcmp(const char *str1, const char *str2)
{
	while (*str1 != '\0' && *str2 != '\0' && *str1 == *str2) {
		str1++;
		str2++;
	}
	return (int) *str1 - *str2;
}

void
Image::RestoreSeg( const char *seg_name )
{
	int		i;

	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp(seg_name,map[i].GetName()) == 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				dprintf(D_ALWAYS, "SegMap::Read() failed!\n");
				Suicide();
			} else {
				return;
			}
		}
	}
	dprintf( D_ALWAYS, "Can't find segment \"%s\"\n", seg_name );
	fprintf( stderr, "CONDOR ERROR: can't find segment \"%s\" on restart\n",
			 seg_name );
	exit( 1 );
}

void Image::RestoreAllSegsExceptStack()
{
	int		i;
	int		save_fd = fd;

#if defined(Solaris) || defined(IRIX53)
	dprintf( D_ALWAYS, "Current segmap dump follows\n");
	display_prmap();
#endif
	for( i=0; i<head.N_Segs(); i++ ) {
		if( mystrcmp("STACK",map[i].GetName()) != 0 ) {
			if( (pos = map[i].Read(fd,pos)) < 0 ) {
				dprintf(D_ALWAYS, "SegMap::Read() failed!\n" );
				Suicide();
			}
		}
		else if (i<head.N_Segs()-1) {
			dprintf( D_ALWAYS, "Checkpoint file error: STACK is not the "
					"last segment in ckpt file.\n");
			fprintf( stderr, "CONDOR ERROR: STACK is not the last segment "
					 " in ckpt file.\n" );
			exit( 1 );
		}
		fd = save_fd;
	}

}

void
RestoreStack()
{

#if defined(ALPHA)			
	unsigned int nbytes;		// 32 bit unsigned
#else
	unsigned long nbytes;		// 32 bit unsigned
#endif
	int		status;
	RAW_ADDR        env;
	int scm;

	dprintf (D_ALWAYS, "ZANDY: About to cream stack\n");
	MyImage.RestoreSeg( "STACK" );
	dprintf (D_ALWAYS, "ZANDY: Stack creamed\n");

	scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	dprintf (D_ALWAYS, "ZANDY: fdm is %d (%x)\n", fdm, &fdm);
	close (fdm);
	close (fdd);
	SetSyscalls (scm);

		// In remote mode, we have to send back size of ckpt informaton
	if( MyImage.GetMode() == REMOTE ) {
		nbytes = MyImage.GetLen();
		nbytes = htonl( nbytes );
		status = write( MyImage.GetFd(), &nbytes, sizeof(nbytes) );
		dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE RECEIVED OK\n" );

		SetSyscalls( SYS_REMOTE | SYS_MAPPED );
	} else {
		SetSyscalls( SYS_LOCAL | SYS_MAPPED );
	}

#if defined(PVM_CHECKPOINTING)
	user_restore_post(global_user_data, sizeof(global_user_data));
#endif

	/* We may be executing in a ckpt lib that was explicitly
	   loaded at run time (e.g., for executables not relinked with
	   Condor syscall_lib).  This lib is not that same as the lib
	   that was saved in the checkpoint file; in particular, Env
	   in this lib is uninitialized.  Make sure Env is the context
	   that was saved at checkpoint time. -zandy 6/18/1998 */
	env = MyImage.GetAddrOfEnv ();
	Env = *((jmp_buf*) env);

	/* We should close the fd *before* we return to the original
           library.  (See non-zero return from setjmp in Checkpoint).
           -zandy 7/19/1998 */
/*	MyImage.Close (); */

	dprintf (D_ALWAYS, "ZANDY: About to restore the Checkpoint\n");

	LONGJMP( Env, 1 );
}

int
Image::Write()
{
	dprintf( D_FULLDEBUG, "Image::Write(): fd %d file_name %s\n",
			 fd, file_name?file_name:"(NULL)");
	if (fd == -1) {
		return Write( file_name );
	} else {
		return Write( fd );
	}
}


/*
  Set up a stream to write our checkpoint information onto, then write
  it.  Note: there are two versions of "open_ckpt_stream", one in
  "local_startup.c" to be linked with programs for "standalone"
  checkpointing, and one in "remote_startup.c" to be linked with
  programs for "remote" checkpointing.  Of course, they do very different
  things, but in either case a file descriptor is returned which we
  should access in LOCAL and UNMAPPED mode.
*/
int
Image::Write( const char *ckpt_file )
{
	int	fd;
	int	status;
	int	scm;
	int bytes_read;
	char	tmp_name[ _POSIX_PATH_MAX ];
#if defined(ALPHA)
	unsigned int  nbytes;		// 32 bit unsigned
#else
	unsigned long  nbytes;		// 32 bit unsigned
#endif

	if( ckpt_file == 0 ) {
		ckpt_file = file_name;
	}

		// Generate tmp file name
	dprintf( D_ALWAYS, "Checkpoint name is \"%s\"\n", ckpt_file );

		// Open the tmp file
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	tmp_name[0] = '\0';
	/* For now we comment out the open_url() call because we are currently
	 * inside of a signal handler.  POSIX says malloc() is not safe to call
	 * in a signal handler; open_url calls malloc() and this messes up
	 * checkpointing on SGI IRIX6 bigtime!!  so it is commented out for
	 * now until we get rid of all mallocs in open_url() -Todd T, 2/97 */
	// if( (fd=open_url(ckpt_file,O_WRONLY|O_TRUNC|O_CREAT,len)) < 0 ) {
		sprintf( tmp_name, "%s.tmp", ckpt_file );
		dprintf( D_ALWAYS, "Tmp name is \"%s\"\n", tmp_name );
		if ((fd = open_ckpt_file(tmp_name, O_WRONLY|O_TRUNC|O_CREAT,
								len)) < 0)  {
				dprintf( D_ALWAYS, "ERROR:open_ckpt_file failed, aborting ckpt\n");
				return -1;
		}
	// }  // this is the matching brace to the open_url; see comment above

		// Write out the checkpoint
	if( Write(fd) < 0 ) {
		return -1;
	}

		// Have to check close() in AFS
	dprintf( D_ALWAYS, "About to close ckpt fd (%d)\n", fd );
	if( close(fd) < 0 ) {
		dprintf( D_ALWAYS, "Close failed!\n" );
		return -1;
	}
	dprintf( D_ALWAYS, "Closed OK\n" );

	SetSyscalls( scm );

		// We now know it's complete, so move it to the real ckpt file name
	if (tmp_name[0] != '\0') {
		dprintf(D_ALWAYS, "About to rename \"%s\" to \"%s\"\n",
				tmp_name, ckpt_file);
		if( rename(tmp_name,ckpt_file) < 0 ) {
			dprintf( D_ALWAYS, "rename failed, aborting ckpt\n" );
			return -1;
		}
		dprintf( D_ALWAYS, "Renamed OK\n" );
	}

		// Report
	dprintf( D_ALWAYS, "USER PROC: CHECKPOINT IMAGE SENT OK\n" );

		// In remote mode we update the shadow on our image size
	if( MyImage.GetMode() == REMOTE ) {
		report_image_size( (MyImage.GetLen() + KILO - 1) / KILO );
	}

	return 0;
}

/*
  Write our checkpoint "image" to a given file descriptor.  At this level
  it makes no difference whether the file descriptor points to a local
  file, a remote file, or another process (for direct migration).
*/
int
Image::Write( int fd )
{
	int		i;
	int		pos = 0;
	int		nbytes;
	int		ack;
	int		status;

	int scm;

		// Write out the header
	if( (nbytes=write(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;
	dprintf( D_ALWAYS, "Wrote headers OK\n" );


	scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	fdm = open ("/common/tmp/zandy/ckpt.map",
		    O_CREAT | O_TRUNC | O_WRONLY,
		    0664);
	if (fdm < 0) {
	     dprintf (D_ALWAYS, "ZANDY: Can't open ckpt.map\n");
	     Suicide ();
	}
	fdd = open ("/common/tmp/zandy/ckpt.data",
		    O_CREAT | O_TRUNC | O_WRONLY,
		    0664);
	if (fdm < 0) {
	     dprintf (D_ALWAYS, "ZANDY: Can't open ckpt.data\n");
	     Suicide ();
	}
	SetSyscalls (scm);

		// Write out the SegMaps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=write(fd,&map[i],sizeof(map[i]))) < 0 ) {
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Wrote SegMap[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Wrote all SegMaps OK\n" );

		// Write out the Segments
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=map[i].Write(fd,pos)) < 0 ) {
			dprintf( D_ALWAYS, "Write() of segment %d failed\n", i );
			dprintf( D_ALWAYS, "errno = %d, nbytes = %d\n", errno, nbytes );
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Wrote Segment[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Wrote all Segments OK\n" );

	scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	close (fdd);
	close (fdm);
	SetSyscalls (scm);

	
		/* When using the stream protocol the shadow echo's the number
		   of bytes transferred as a final acknowledgement. */
	if( _condor_in_file_stream ) {
		status = net_read( fd, &ack, sizeof(ack) );
		if( status < 0 ) {
			dprintf( D_ALWAYS, "Can't read final ack from the shadow\n" );
			return -1;
		}

		ack = ntohl( ack );	// Ack is in network byte order, fix here
		if( ack != len ) {
			dprintf( D_ALWAYS, "Ack - expected %d, but got %d\n", len, ack );
			return -1;
		}
	}

	return 0;
}


/*
  Read in our checkpoint "image" from a given file descriptor.  The
  descriptor could point to a local file, a remote file, or another
  process (in the case of direct migration).  Here we only read in
  the image "header" and the maps describing the segments we need to
  restore.  The Restore() function will do the rest.
*/
int
Image::Read()
{
	int		i;
	int		nbytes;

		// Make sure we have a valid file descriptor to read from
	if( fd < 0 && file_name && file_name[0] ) {
//		if( (fd=open_url(file_name,O_RDONLY,0)) < 0 ) {
			if( (fd=open_ckpt_file(file_name,O_RDONLY,0)) < 0 ) {
				dprintf( D_ALWAYS, "open_ckpt_file failed: %s",
						 strerror(errno));
				return -1;
			}
//		}		// don't use URL library -- Jim B.
	}

		// Read in the header
	if( (nbytes=net_read(fd,&head,sizeof(head))) < 0 ) {
		return -1;
	}
	pos += nbytes;
	dprintf( D_ALWAYS, "Read headers OK\n" );

		// Read in the segment maps
	for( i=0; i<head.N_Segs(); i++ ) {
		if( (nbytes=net_read(fd,&map[i],sizeof(SegMap))) < 0 ) {
			return -1;
		}
		pos += nbytes;
		dprintf( D_ALWAYS, "Read SegMap[%d] OK\n", i );
	}
	dprintf( D_ALWAYS, "Read all SegMaps OK\n" );

	return 0;
}

void
Image::Close()
{
	if( fd < 0 ) {
		dprintf( D_ALWAYS, "Image::Close - file not open!\n" );
	}
	close( fd );
	/* The next checkpoint is going to assume the fd is -1, so set it here */
	fd = -1;
}

ssize_t
SegMap::Read( int fd, ssize_t pos )
{
	int		nbytes;
	char *orig_brk;
	char *cur_brk;
	char	*ptr;
	int		bytes_to_go;
	int		read_size;
	long	saved_len = len;
	int 	saved_prot = prot;
	RAW_ADDR	saved_core_loc = core_loc;

	if( pos != file_loc ) {
		dprintf( D_ALWAYS, "Checkpoint sequence error (%d != %d)\n", pos,
				 file_loc );
		Suicide();
	}

	if( mystrcmp(name,"DATA") == 0 ) {
		orig_brk = (char *)sbrk(0);
		if( orig_brk < (char *)(core_loc + len) ) {
			brk( (char *)(core_loc + len) );
		}
		cur_brk = (char *)sbrk(0);
	}

#if defined(Solaris) || defined(IRIX53) || defined(LINUX)
	else if ( mystrcmp(name,"SHARED LIB") == 0) {
		int zfd, segSize = len;
		if ((zfd = SYSCALL(SYS_open, "/dev/zero", O_RDWR)) == -1) {
			dprintf( D_ALWAYS,
					 "Unable to open /dev/zero in read/write mode.\n");
			dprintf( D_ALWAYS, "open: %s\n", strerror(errno));
			Suicide();
		}

	  /* Some notes about mmap:
	     - The MAP_FIXED flag will ensure that the memory allocated is
	       exactly what was requested.
	     - Both the addr and off parameters must be aligned and sized
	       according to the value returned by getpagesize() when MAP_FIXED
	       is used.  If the len parameter is not a multiple of the page
	       size for the machine, then the system will automatically round
	       up. 
	     - Protections must allow writing, so that the dll data can be
	       copied into memory. 
	     - Memory should be private, so we don't mess with any other
	       processes that might be accessing the same library. */

//		fprintf(stderr, "Calling mmap(loc = 0x%lx, size = 0x%lx, "
//			"prot = %d, fd = %d, offset = 0)\n", core_loc, segSize,
//			prot|PROT_WRITE, zfd);
#if defined(Solaris) || defined(LINUX)
		if ((MMAP((MMAP_T)core_loc, (size_t)segSize,
				  prot|PROT_WRITE,
				  MAP_PRIVATE|MAP_FIXED, zfd,
				  (off_t)0)) == MAP_FAILED) {
#elif defined(IRIX53)
		if (MMAP((caddr_t)saved_core_loc, (size_t)segSize,
				 (saved_prot|PROT_WRITE)&(~MA_SHARED),
				 MAP_PRIVATE|MAP_FIXED, zfd,
				 (off_t)0) == MAP_FAILED) {
#endif

			dprintf(D_ALWAYS, "mmap: %s", strerror(errno));
			dprintf(D_ALWAYS, "Attempted to mmap /dev/zero at "
				"address 0x%lx, size 0x%lx\n", saved_core_loc,
				segSize);
			dprintf(D_ALWAYS, "Current segmap dump follows\n");
			display_prmap();
			Suicide();
		}

		/* WARNING: We have potentially just overwritten libc.so.  Do
		   not make calls that are defined in this (or any other)
		   shared library until we restore all shared libraries from
		   the checkpoint (i.e., use mystrcmp and SYSCALL).  -Jim B. */

		if (SYSCALL(SYS_close, zfd) < 0) {
			dprintf( D_ALWAYS,
					 "Unable to close /dev/zero file descriptor.\n" );
			dprintf( D_ALWAYS, "close: %s\n", strerror(errno));
			Suicide();
		}
	}		
#endif		

		// This overwrites an entire segment of our address space
		// (data or stack).  Assume we have been handed an fd which
		// can be read by purely local syscalls, and we don't need
		// to need to mess with the system call mode, fd mapping tables,
		// etc. as none of that would work considering we are overwriting
		// them.
	bytes_to_go = saved_len;
	ptr = (char *)saved_core_loc;
	while( bytes_to_go ) {
		read_size = bytes_to_go > 4096 ? 4096 : bytes_to_go;
#if defined(Solaris) || defined(IRIX53) || defined(LINUX)
		nbytes =  SYSCALL(SYS_read, fd, (void *)ptr, read_size );
#else
		nbytes =  syscall( SYS_read, fd, (void *)ptr, read_size );
#endif
		if( nbytes < 0 ) {
			dprintf(D_ALWAYS, "in Segmap::Read(): fd = %d, read_size=%d\n", fd,
				read_size);
			dprintf(D_ALWAYS, "Error=%d, core_loc=%x\n", errno, core_loc);
			return -1;
		}
		bytes_to_go -= nbytes;
		ptr += nbytes;
	}
	{
	     RAW_ADDR addr_end = core_loc + len;
	     int scm, type;
	     scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	     write (fdm, &core_loc, sizeof (core_loc));
	     write (fdm, &addr_end, sizeof (addr_end));
	     write (fdm, &prot, sizeof (prot));
	     if (! mystrcmp (name, "SHARED LIB"))
		  type = 1; /* SHARED */
	     else if (! mystrcmp (name, "DATA"))
		  type = 0; /* DATA */
	     else if (! mystrcmp (name, "STACK"))
		  type = 2; /* STACK */
	     else {
		  dprintf (D_ALWAYS, "ZANDY: Map name is screwy\n");
		  Suicide ();
	     }
	     write (fdm, &type, sizeof (type));
	     write (fdd, (char *) core_loc, len);
	     SetSyscalls (scm);
	}
	return pos + len;
}

ssize_t
SegMap::Write( int fd, ssize_t pos )
{
	if( pos != file_loc ) {
		dprintf( D_ALWAYS, "Checkpoint sequence error (%d != %d)\n",
				 pos, file_loc );
		Suicide();
	}
	dprintf( D_ALWAYS, "write(fd=%d,core_loc=0x%lx,len=0x%lx)\n",
			fd, core_loc, len );
	{
	     RAW_ADDR addr_end = core_loc + len;
	     int scm, type;
	     scm = SetSyscalls (SYS_LOCAL | SYS_UNMAPPED);
	     write (fdm, &core_loc, sizeof (core_loc));
	     write (fdm, &addr_end, sizeof (addr_end));
	     write (fdm, &prot, sizeof (prot));
	     if (! mystrcmp (name, "SHARED LIB"))
		  type = 1; /* SHARED */
	     else if (! mystrcmp (name, "DATA"))
		  type = 0; /* DATA */
	     else if (! mystrcmp (name, "STACK"))
		  type = 2; /* STACK */
	     else {
		  dprintf (D_ALWAYS, "ZANDY: Map name is screwy\n");
		  Suicide ();
	     }
	     write (fdm, &type, sizeof (type));
	     write (fdd, (char *) core_loc, len);
	     SetSyscalls (scm);
	}
	return write(fd,(void *)core_loc,(size_t)len);
}

extern "C" {

/*
  This is the signal handler which actually effects a checkpoint.  This
  must be implemented as a signal handler, since we assume the signal
  handling code provided by the system will save and restore important
  elements of our context (register values, etc.).  A process wishing
  to checkpoint itself should generate the correct signal, not call this
  routine directory, (the ckpt()) function does this.
  8/95: And now, SIGTSTP means "checkpoint and vacate", and other signal
  that gets here (aka SIGUSR2) means checkpoint and keep running (a 
  periodic checkpoint). -Todd Tannenbaum
*/
void
#if defined( HPUX10 )
Checkpoint( int sig, siginfo_t *code, void *scp )
#else
Checkpoint( int sig, int code, void *scp )
#endif
{
	int		scm, p_scm;
	int		do_full_restart = 1; // set to 0 for periodic checkpoint
	int		write_result;

		// No sense trying to do a checkpoint in the middle of a
		// restart, just quit leaving the current ckpt entact.
	    // WARNING: This test should be done before any other code in
	    // the signal handler.
	if( InRestart ) {
		if ( sig == SIGTSTP )
			Suicide();		// if we're supposed to vacate, kill ourselves
		else
			return;			// if periodic ckpt or we're currently ckpting
	}
	InRestart = TRUE;	// not strictly true, but needed in our saved data
	check_sig = sig;

	dprintf( D_ALWAYS, "Entering Checkpoint()\n" );

	if( MyImage.GetMode() == REMOTE ) {
		scm = SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
#if !defined(PVM_CHECKPOINTING)
		if ( MyImage.GetFd() != -1 ) {
			// Here we make _certain_ that fd is -1.  on remote checkpoints,
			// the fd is always new since we open a fresh TCP socket to the
			// shadow.  I have detected some buggy behavior where the remote
			// job prematurely exits with a status 4, and think that it is
			// related to the fact that fd is _not_ -1 here, so we make
			// certain.  Hopefully the real bug will be found someday and
			// this "patch" can go away...  -Todd 11/95
			
		dprintf(D_ALWAYS,"WARNING: fd is %d for remote checkpoint, should be -1\n",MyImage.GetFd());
		MyImage.SetFd( -1 );
		}
#endif

	} else {
		scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	}


	if ( sig == SIGTSTP ) {
		dprintf( D_ALWAYS, "Got SIGTSTP\n" );
	} else {
		dprintf( D_ALWAYS, "Got SIGUSR2\n" );
	}

#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		wait_up = 1;
	while( wait_up )
		;
#endif
	if( SETJMP(Env) == 0 ) {	// Checkpoint
		dprintf( D_ALWAYS, "About to save MyImage\n" );
#ifdef SAVE_SIGSTATE
		dprintf( D_ALWAYS, "About to save signal state\n" );
		condor_save_sigstates();
		dprintf( D_ALWAYS, "Done saving signal state\n" );
#endif
		SaveFileState();
		MyImage.Save();
		write_result = MyImage.Write();
		if ( sig == SIGTSTP ) {
			/* we have just checkpointed; now time to vacate */
			dprintf( D_ALWAYS,  "Ckpt exit\n" );
			SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			if ( write_result == 0 ) {
				terminate_with_sig( SIGUSR2 );
			} else {
				Suicide();
			}
			/* should never get here */
		} else {
			/* we have just checkpointed, but this is a periodic checkpoint.
			 * so, update the shadow with accumulated CPU time info if we
			 * are not standalone, and then continue running. -Todd Tannenbaum */
			if ( MyImage.GetMode() == REMOTE ) {

				// first, reset the fd to -1.  this is normally done in
				// a call in remote_startup, but that will not get called
				// before the next periodic checkpoint so we must clear
				// it here.
				MyImage.SetFd( -1 );

				if ( write_result == 0 ) {  /* only update if write was happy */
				/* now update shadow with CPU time info.  unfortunately, we need
				 * to convert to struct rusage here in the user code, because
				 * clock_tick is platform dependent and we don't want CPU times
				 * messed up if the shadow is running on a different architecture */
				struct tms posix_usage;
				struct rusage bsd_usage;
				long clock_tick;

				p_scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
				memset(&bsd_usage,0,sizeof(struct rusage));
				times( &posix_usage );
#if defined(OSF1)
			    clock_tick = CLK_TCK;
#else
        		clock_tick = sysconf( _SC_CLK_TCK );
#endif
				bsd_usage.ru_utime.tv_sec = posix_usage.tms_utime / clock_tick;
				bsd_usage.ru_utime.tv_usec = posix_usage.tms_utime % clock_tick;
				(bsd_usage.ru_utime.tv_usec) *= 1000000 / clock_tick;
				bsd_usage.ru_stime.tv_sec = posix_usage.tms_stime / clock_tick;
				bsd_usage.ru_stime.tv_usec = posix_usage.tms_stime % clock_tick;
				(bsd_usage.ru_stime.tv_usec) *= 1000000 / clock_tick;
				SetSyscalls( SYS_REMOTE | SYS_UNMAPPED );
				(void)REMOTE_syscall( CONDOR_send_rusage, (void *) &bsd_usage );
				SetSyscalls( p_scm );
				}  /* end of if write_result == 0 */
				
			}
			do_full_restart = 0;
			dprintf(D_ALWAYS, "Periodic Ckpt complete, doing a virtual restart...\n");
			LONGJMP( Env, 1);
		}
	} else {					// Restart
#undef WAIT_FOR_DEBUGGER
#if defined(WAIT_FOR_DEBUGGER)
	int		wait_up = 1;
	while( wait_up )
		;
#endif

	dprintf (D_ALWAYS, "ZANDY: Just returned from longjump\n");


		/* We may have been restored by a ckpt lib that was
		   explicitly loaded at run time by the restoring
		   process (e.g., for executables not relinked with
		   Condor syscall_lib).  We don't need this library:
		   there was already a ckpt lib in the ckpt image (we
		   are executing in it now).  Unload it, lest future
		   ckpts grow for no purpose. -zandy 6/18/1998 */
		dprintf (D_ALWAYS, "About to unload gangrenous segments\n");
		if (0 != MyImage.unloadGangrenousSegments ()) {
		     dprintf (D_ALWAYS, "Error unloading gangrenous segments");
		     Suicide ();
		} else
		     dprintf (D_ALWAYS, "Unloaded gangrenous segments\n");

		/* Quiz: Why couldn't we have done this before?
		   -zandy 7/21/1998 */
		_install_signal_handler( SIGTSTP, Checkpoint);
		_install_signal_handler( SIGUSR2, Checkpoint);
		_install_signal_handler( SIGUSR1, Suicide);

		/* Quick and dirty fix.  This is not general.
		   -zandy 7/16/1998 */
		SetSyscalls (SYS_REMOTE | SYS_MAPPED);

		if ( do_full_restart ) {
			scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
			patch_registers( scp );
			/* I think the Close should happen before the
			   code jumps to the original lib -- so it's
			   done in RestoreStack now.  -zandy 7/15/1998
			   MyImage.Close();
			*/

			if( MyImage.GetMode() == REMOTE ) {
				SetSyscalls( SYS_REMOTE | SYS_MAPPED );
			} else {
				SetSyscalls( SYS_LOCAL | SYS_MAPPED );
			}
			RestoreFileState();
			dprintf( D_ALWAYS, "Done restoring files state\n" );
		} else {
			patch_registers( scp );
		}

#ifdef HPUX10
    /* TODD'S SCARY FIX TO THE HPUX10.X FORTRAN PROBLEM ------
     * reset the return-pointer in the current stack frame
     * to _sigreturn, as it sometimes is a screwed-up address during
     * a restart with HPUX10 Fortran. weird trampoline code in HPUX f77?
	 * We find the return-pointer in the stack frame by adding an offset (16)
	 * from the address of the 1st parameter on the frame. (in this case, &sig)
     * WARNING: we are only dealing here with 32-bit RP addresses!  We
     * may need to make this patch more intelligent someday.
     * WANRING: do not move this code to a different procedure/func,
     * we need to twiddle _this_ stack frame and &sig is only in scope
     * here in Checkpoint().  -Todd, 4/97 */
	    *((unsigned int *)( ((unsigned int) &sig)+16 )) = (unsigned int) _sigreturn;
#endif

#ifdef SAVE_SIGSTATE
		dprintf( D_ALWAYS, "About to restore signal state\n" );
		condor_restore_sigstates();
		dprintf( D_ALWAYS, "Done restoring signal state\n" );
#endif

		_condor_numrestarts++;
		SetSyscalls( scm );
		dprintf( D_ALWAYS, "About to return to user code\n" );
		InRestart = FALSE;
		return;
	}
}

void
init_image_with_file_name( char *ckpt_name )
{
	MyImage.SetFileName( ckpt_name );
}

void
init_image_with_file_descriptor( int fd )
{
	MyImage.SetFd( fd );
}


/*
  Effect a restart by reading in an "image" containing checkpointing
  information and then overwriting our process with that image.
*/
void
restart( )
{
	InRestart = TRUE;

	if (MyImage.Read() < 0) Suicide();
	MyImage.Restore();
}

}	// end of extern "C"



/*
  Checkpointing must by implemented as a signal handler.  This routine
  generates the required signal to invoke the handler.
  ckpt() = periodic ckpt
  ckpt_and_exit() = ckpt and "vacate"
*/
extern "C" {
void
ckpt()
{
	int		scm;

	dprintf( D_ALWAYS, "About to send CHECKPOINT signal to SELF\n" );
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	kill( getpid(), SIGUSR2 );
	SetSyscalls( scm );
}
void
ckpt_and_exit()
{
	int		scm;

	dprintf( D_ALWAYS, "About to send CHECKPOINT and EXIT signal to SELF\n" );
	scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	kill( getpid(), SIGTSTP );
	SetSyscalls( scm );
}

/*
** Some FORTRAN compilers expect "_" after the symbol name.
*/
void
ckpt_() {
    ckpt();
}
void
ckpt_and_exit_() {
    ckpt_and_exit();
}
}   /* end of extern "C" */

/*
  Arrange to terminate abnormally with the given signal.  Note: the
  expectation is that the signal is one whose default action terminates
  the process - could be with a core dump or not, depending on the sig.
*/
void
terminate_with_sig( int sig )
{
	sigset_t	mask;
	pid_t		my_pid;
	struct sigaction act;

	/* Note: If InRestart, avoid accessing any non-local data, as it
	   may be corrupt.  This includes calling dprintf().  Also avoid
	   calling any libc functions, since libc might be corrupt during
	   a restart. Since sigaction and sigsuspend are defined in
	   signals_support.C, they should be safe to call. -Jim B. */

		// Make sure all system calls handled "straight through"
	if (!InRestart) SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );

		// Make sure we have the default action in place for the sig
	if( sig != SIGKILL && sig != SIGSTOP ) {
#ifdef HPUX10
		act.sa_handler = SIG_DFL;
#else
		act.sa_handler = (SIG_HANDLER)SIG_DFL;
#endif
		// mask everything so no user-level sig handlers run
		sigfillset( &act.sa_mask );
		act.sa_flags = 0;
		errno = 0;
		if( sigaction(sig,&act,0) < 0 ) {
			if (!InRestart) dprintf(D_ALWAYS, "sigaction: %s\n",
									strerror(errno));
			Suicide();
		}
	}

		// Send ourself the signal
	my_pid = SYSCALL(SYS_getpid);
	if (!InRestart) {
		dprintf( D_ALWAYS, "About to send signal %d to process %d\n",
				sig, my_pid );
	}
	if( SYSCALL(SYS_kill, my_pid, sig) < 0 ) {
		EXCEPT( "kill" );
	}

		// Wait to die... and mask all sigs except the one to kill us; this
		// way a user's sig won't sneak in on us - Todd 12/94
	sigfillset( &mask );
	sigdelset( &mask, sig );
	sigsuspend( &mask );

		// Should never get here
	EXCEPT( "Should never get here" );

}

/*
  We have been requested to exit.  We do it by sending ourselves a
  SIGKILL, i.e. "kill -9".
*/
void
Suicide()
{
	terminate_with_sig( SIGKILL );
}

static void
find_stack_location( RAW_ADDR &start, RAW_ADDR &end )
{
	if( SP_in_data_area() ) {
		dprintf( D_ALWAYS, "Stack pointer in data area\n" );
		if( StackGrowsDown() ) {
			end = stack_end_addr();
			start = end - StackSaveSize;
		} else {
			start = stack_start_addr();
			end = start + StackSaveSize;
		}
	} else {
		start = stack_start_addr();
		end = stack_end_addr();
	}
}

extern "C" double atof( const char * );

const size_t	MEG = (1024 * 1024);

void
calc_stack_to_save()
{
	char	*ptr;

	ptr = getenv( "CONDOR_STACK_SIZE" );
	if( ptr ) {
		StackSaveSize = (size_t) (atof(ptr) * MEG);
	} else {
		StackSaveSize = MEG * 2;	// default 2 megabytes
	}
}

/*
  Return true if the stack pointer points into the "data" area.  This
  will often be the case for programs which utilize threads or co-routine
  packages.
*/
static int
SP_in_data_area()
{
	RAW_ADDR	data_start, data_end;
	RAW_ADDR	SP;

	data_start = data_start_addr();
	data_end = data_end_addr();

	if( StackGrowsDown() ) {
		SP = stack_start_addr();
	} else {
		SP = stack_end_addr();
	}

	return data_start <= SP && SP <= data_end;
}

extern "C" {
void
_condor_save_stack_location()
{
	SavedStackLoc = stack_start_addr();
}

#if defined( X86 ) && defined( Solaris ) 
int
__CERROR() 
{
	return errno;
}
#endif


#if defined( X86 ) && defined( Solaris26 ) 
int
__CERROR64()
{
	return errno;
}
#endif


} /* extern "C" */
