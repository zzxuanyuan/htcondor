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
#include "../condor_ckpt/image.h"
#include "../condor_ckpt/mm.h"
#include <dlfcn.h>     /* dlopen */
static char *_FileName_ = __FILE__;


/* Startup for runtime-linked jobs.  Both first-time job startup and
   checkpoint restart are handled here.

   We cannot interpose Condor code between _start and main, as
standard jobs do.  Instead we use LD_PRELOAD to load the Condor
libraries before any other shlibs are loaded by the app.  The first
library we load, librtlink_init.so, has a .init section that calls
rtlink_init().

   A side effect of LD_PRELOAD is that Condor system calls are also
loaded before the standard definitions in libc, and consequently the
app's syscalls resolve to Condor, not libc.

   To restart a Condor checkpoint we load a second copy of the restart
code, librtlink_restart.so.  Before it is loaded, we mmap all pages in
memory that will be occupied by the checkpoint image to ensure that
the restart code and data occupies safe memory -- memory that it will
not overwrite as the checkpoint is restored.

   rtlink_librestart_entry() is the interface between
libcondor_syscall_lib and librtlink_restart.  Once it has been called,
control and all data references remain within librtlink_restart until
the checkpoint image is restored.  We use the runtime loader (via a
call to dlsym()) to ensure that we call the definition in
librtlink_restart.

   Keep in mind that two copies of the Condor libraries are present as
the checkpoint is restarted.  Initialization and other operations that
appear to be repetition of previous work are really operations in
distinct libraries.  For example, there are two calls to
_condor_prestart() in the restart path here, but one happens from
libcondor_syscall_lib, the other librtlink_restart.

*/
   
static SegMap ckptsegmap[MAX_MMAP_REGIONS];

/* We need several memory maps.  Use static memory to avoid 
   dealing with malloc.  */

/* The mmap of the checkpointed process.  Equivalent to the SegMap
   stored in the header of the checkpoint image. */
static mmap_region ckptmmap[MAX_MMAP_REGIONS];
static int numckptmmap;

/* The mmap of this process. */
static mmap_region procmmap[MAX_MMAP_REGIONS];
static int numprocmmap;

/* The union of CKPTMMAP and PROCMMAP.  Defines every page that must
   be allocated before it is safe to load secondary restart
   library. */
static mmap_region unionmmap[MAX_MMAP_REGIONS];
static int numunionmmap;

/* The difference of UNIONMMAP and PROCMMAP.  Defines every page that
   we must mmap() before loading the secondary restart library. */
static mmap_region diffmmap[MAX_MMAP_REGIONS];
static int numdiffmmap;

/* The map of the pages that must be unloaded after the process is
   restarted.  */
static mmap_region unldmmap[MAX_MMAP_REGIONS];
static int numunldmmap;


extern "C" {
/* Defined in remote_startup_common.c */
extern void unblock_signals();
extern void display_ip_addr( unsigned int addr );
extern void open_std_file( int which );
extern void set_iwd();
extern int open_ckpt_file( const char *name, int flags, size_t n_bytes );
extern void get_ckpt_name();

extern void _condor_disable_uid_switching();
}

/* Defined in condor_ckpt/image.C */
extern void segmap_to_mmap(SegMap *, int, mmap_region *);
extern int condor_net_read(int fd, void *buf, int size);
extern void restart();
extern volatile int InRestart;
extern int condor_rtlink;

static void rtlink_start();
static void rtlink_restart();

static int
mystrcmp(const char *str1, const char *str2)
{
	while (*str1 != '\0' && *str2 != '\0' && *str1 == *str2) {
		str1++;
		str2++;
	}
	return (int) *str1 - *str2;
}

/* Return the brk of the ckpt memory map SEGMAP.  Condor defines the
   brk of the checkpointed process as the end address of the segment
   named "DATA" in SEGMAP. */
static unsigned long
segmap_brk(SegMap *segmap, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (! mystrcmp(segmap[i].GetName(), "DATA"))
			return (unsigned long) (segmap[i].GetLoc() + segmap[i].GetLen());
	}
	return 0;
}


/* Condor _start for runtime-linked jobs.  We do not interpose a start
   function before main(), as is done for vanilla jobs.  Instead, we
   place a call to this function in the .init section of the
   runtime-loaded Condor syscall library, and the runtime loader will
   call it as soon as it is loaded.  See rtlink_init.c for the details
   of how it is called.  */

extern "C" {
void
rtlink_init()
{
	char *restart;
	// The starter always sets this variable for RTLINK jobs.  If
	// TRUE, we are restarting a checkpoint; FALSE means the job is
	// starting for the first time.
	restart = getenv("CONDOR_RTLINK_RESTART");
	if (restart) {
		condor_rtlink = 1;
		if (! strcmp(restart, "TRUE")) {
			dprintf(D_ALWAYS, "Restarting a runtime-linked job\n");
			rtlink_restart();
			dprintf(D_ALWAYS, "Unexpected return to runtime-linked job startup\n");
			Suicide();
		} else {
			dprintf(D_ALWAYS, "Starting a runtime-linked job\n");
			rtlink_start();
		}

	} else {
		dprintf(D_ALWAYS, "This program has not been correctly started as a Condor job\n");
		Suicide();
	}
}
}

static void
rtlink_start()
{
		/* 
		   Since we're the user job, we want to just disable any
		   priv-state switching.  We would, eventually, anyway, since
		   we'd realize we weren't root, but then we hit the somewhat
		   expensive init_condor_ids() code in uids.c, which is
		   segfaulting on HPUX10 on a machine using YP.  So, we just
		   call a helper function here that sets the flags in uids.c
		   how we want them so we don't do switching, period.
		   -Derek Wright 7/21/98 */
	_condor_disable_uid_switching();

	/* now setup signal handlers, etc */
	_condor_prestart( SYS_REMOTE );

	init_syscall_connection( FALSE );
	unblock_signals();
	SetSyscalls(SYS_REMOTE|SYS_MAPPED);
	set_iwd();
	get_ckpt_name();
	open_std_file( 0 );
	open_std_file( 1 );
	open_std_file( 2 );
	InRestart = FALSE;
	dprintf(D_ALWAYS, "Job initialized by runtime loaded library\n");
}

static void
rtlink_restart()
{
	char ckpt_name[_POSIX_PATH_MAX];
	int rv;
	int fd;
	Header header;
	void *handle;
	void (*entry)(int, unsigned long, unsigned long, unsigned long);
	int i;
	unsigned long addrEnv;
	void *newbrk;
	char buf[256];
	char *libp;

	dprintf(D_ALWAYS, "Remote restart starting\n");
	_condor_prestart(SYS_REMOTE);
	init_syscall_connection(FALSE);
	InRestart = TRUE;
	rv = REMOTE_syscall(CONDOR_get_ckpt_name, ckpt_name);
	if (0 > rv) {
		dprintf(D_ALWAYS, "Can't get checkpoint file name!\n");
		Suicide();
	}
	dprintf(D_ALWAYS, "Got checkpoint name\n");
	fd = open_ckpt_file(ckpt_name, O_RDONLY, 0);
	if (0 > fd) {
		dprintf(D_ALWAYS, "open_ckpt_file failed: %s\n",
				strerror(errno));
		Suicide();
	}
	dprintf(D_ALWAYS, "Opened checkpoint file\n");
	rv = condor_net_read(fd, &header, sizeof(header));
	if (rv != sizeof(header)) {
		dprintf(D_ALWAYS, "Error reading ckpt header\n"); 
		Suicide();
	}
	dprintf(D_ALWAYS, "Read ckpt header\n");
	numckptmmap = header.N_Segs();
	if (numckptmmap > MAX_MMAP_REGIONS) {
		dprintf(D_ALWAYS, "Exhausted static memory map memory\n");
		Suicide();
	}
	dprintf(D_ALWAYS, "Header says there are %d segments\n", numckptmmap);

	rv = condor_net_read(fd, ckptsegmap, numckptmmap * sizeof(SegMap));
	if (rv != numckptmmap * sizeof(SegMap)) {
		dprintf(D_ALWAYS, "Error reading ckpt segmap\n");
		Suicide();
	}
	close(fd);
	segmap_to_mmap(ckptsegmap, numckptmmap, ckptmmap);
	mmap_canonicalize(ckptmmap, &numckptmmap);

	/* Resetting the brk is tricky for RTLINK jobs.  Here the goal is
	   to ensure the brk is at least as high as it in the ckpt image.
	   After the ckpt image is restored, we set it again, in case the
	   brk grew during the restart phase.

	   In general, beware that the OS forbids advancing the brk into
	   pages that have mmapped. */
	newbrk = (void *) segmap_brk(ckptsegmap, numckptmmap);
	mmap_brk(newbrk);

	if (0 > mmap_get_process_mmap(procmmap, &numprocmmap)) {
		dprintf(D_ALWAYS, "Error reading process memory map\n");
		Suicide();
	}

	/* Compute the mmap of pages that we must allocate before 
	   loading the real restart library. */
	mmap_canonicalize(procmmap, &numprocmmap);
	mmap_union(ckptmmap, numckptmmap,
			   procmmap, numprocmmap,
			   unionmmap, &numunionmmap);
	mmap_diff(unionmmap, numunionmmap,
			  procmmap, numprocmmap,
			  diffmmap, &numdiffmmap);

	/* Allocate these pages. */
	mmap_mmap(diffmmap, numdiffmmap);

	/* Load the library. */
	
	libp = getenv("CONDOR_RTLINK_LIBDIR");
	if (!libp) {
		dprintf(D_ALWAYS, "I don't know the path to the restart library\n");
		Suicide();
	}
	sprintf(buf, "%s/librtlink_restart.so", libp);
	handle = dlopen(buf, RTLD_LAZY);
	if (! handle) {
		dprintf(D_ALWAYS, "Can't load restart library %s\n", buf);
		Suicide();
	}

	/* Reopen the checkpoint file. */
	fd = open_ckpt_file(ckpt_name, O_RDONLY, 0);
	if (0 > fd) {
		dprintf(D_ALWAYS, "open_ckpt_file failed: %s\n",
				strerror(errno));
		Suicide();
	}
	dprintf(D_ALWAYS, "Reopened ckpt file fd = %d\n", fd);

	entry = ((void (*)(int, unsigned long, unsigned long, unsigned long))
			 dlsym(handle, "rtlink_librestart_entry"));
	if (! entry) {
		dprintf(D_ALWAYS, "Couldn't find entry function in librestart\n");
		Suicide();
	}
	entry(fd, header.addrEnv, header.addrFileTab, header.addrSyscallSock);

	dprintf(D_ALWAYS, "Got to the end of remote_restart\n");
	sleep(1);
}

extern "C" {
void
rtlink_librestart_entry(int fd, unsigned long addrEnv, unsigned long addrFileTab,
						unsigned long addrSyscallSock)
{
	condor_rtlink = 1;
	_condor_disable_uid_switching();
	init_image_with_librestart(fd, addrEnv, addrFileTab, addrSyscallSock);
	_condor_prestart(SYS_REMOTE);
	init_syscall_connection(FALSE);
	dprintf(D_ALWAYS, "About to call restart\n");
	InRestart = FALSE;
	restart();
	dprintf(D_ALWAYS, "Boy, oh boy, we shouldn't be here\n");
	Suicide();
}
}
