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

 

#ifndef _IMAGE_H
#define _IMAGE_H

#include "condor_common.h"
#include "machdep.h"
#include "mm.h"


#define NAME_LEN 64
typedef unsigned long RAW_ADDR;
typedef int BOOL;

const int MAGIC = 0xfeafea;
const int COMPRESS_MAGIC = 0xfeafeb;
const int SEG_INCR = 25;
const int  MAX_SEGS = 200;
const int ALT_HEAP_SIZE = 10*1024*1024;	// 10MB
const int RESERVED_HEAP = 1024*1024*1024; // 1GB

class Header {
public:
	void Init();
	void IncrSegs() { n_segs += 1; }
	int	N_Segs() { return n_segs; }
	RAW_ADDR AltHeap() { return alt_heap; }
	void AltHeap(RAW_ADDR loc) { alt_heap = loc; }
	void Display();
	int Magic() { return magic; }
	void ResetMagic();
	RAW_ADDR addrEnv;
	RAW_ADDR addrFileTab;
	RAW_ADDR addrSyscallSock;
private:
	int		magic;
	int		n_segs;
	RAW_ADDR	alt_heap;
	char	pad[ 1024 - 2 * sizeof(int) - NAME_LEN - 4 * sizeof(RAW_ADDR)];
};

class SegMap {
public:
	void Init( const char *name, RAW_ADDR core_loc, long len, int prot );
	ssize_t Read( int fd, ssize_t pos );
	ssize_t Write( int fd, ssize_t pos );
	ssize_t SetPos( ssize_t my_pos );
	BOOL Contains( void *addr );
	char *GetName() { return name; }
	RAW_ADDR GetLoc() { return core_loc; }
	long GetLen() { return len; }
	int GetProt() { return prot; }
	void MSync();
	void Display();
private:
	char		name[14];
	off_t		file_loc;
	RAW_ADDR	core_loc;
	long		len;
	int			prot;		// segment protection mode
};

typedef enum { STANDALONE, REMOTE } ExecutionMode;

class Image {
public:
	void Save();
	int	Write();
	int Write( int fd );
	int Write( const char *name );
	int Read();
	int Read( int fd );
	int Read( const char *name );
	void Close();
	void Restore();
	char *FindSeg( void *addr );
	void Display();
	void RestoreSeg( const char *seg_name );
	void RestoreAllSegsExceptStack();
	void SetFd( int fd );
	void SetFileName( char *ckpt_name );
	void SetMode( int syscall_mode );
	void MSync();
#if defined(COMPRESS_CKPT)
	void *FindAltHeap();
#endif
	ExecutionMode	GetMode() { return mode; }
	size_t			GetLen()  { return len; }
	int				GetFd()   { return fd; }

	// When ckpts are restored by the runtime-loaded restart library,
	// the environment saved at ckpt is unavailable, because it is in
	// a different shlib.  We save a pointer to it in the ckpt
	// header, and when the ckpt image is restored, we use it to read
	// the environment.
	RAW_ADDR		GetAddrEnv() 			{ return head.addrEnv; }
	void SetAddrEnv(RAW_ADDR addr) 		   	{ head.addrEnv = addr; }

	// These objects are dynamically allocated at two times: in the
	// initialization of the job, and in the initialization of a ckpt
	// restart.  It is a problem if allocator returns a different
	// pointer at these times.  Before the restart code has copied the
	// ckpt heap image back into the process, these pointers are
	// valid.  But once the heap image is copied, these pointers are no
	// longer valid, since they are different than what the allocator
	// returned when the process was initialized.
    // So, we save their addresses during a ckpt and restore them
	// after the heap has been copied back.
    // There may be other objects with this problem --
	// anything that is dyn allocated by both the job initialization and
	// the restart code.
	RAW_ADDR 		GetAddrSyscallSock()	{ return head.addrSyscallSock; }
	RAW_ADDR		GetAddrFileTab()		{ return head.addrFileTab; }
	void SetAddrSyscallSock(RAW_ADDR addr) 	{ head.addrSyscallSock = addr; }
	void SetAddrFileTab(RAW_ADDR addr)	   	{ head.addrFileTab = addr; }
	void UnloadRestartLibrary();

	void ResetBrk();
protected:
	RAW_ADDR	GetStackLimit();
	void AddSegment( const char *name, RAW_ADDR start, RAW_ADDR end,
			int prot );
	void SwitchStack( char *base, size_t len );
	char	*file_name;
	Header	head;
	SegMap	map[ MAX_SEGS ];
	ExecutionMode	mode;	// executing in standalone/remote mode
	BOOL	valid;		// initialized and ready to write ckpt file or restore
	int		fd;		// descriptor pointing to ckpt file
	ssize_t	pos;	// position in ckpt file of seg currently reading/writing
	size_t	len;	// size of our ckpt file
};

/* We would like to access the global image from elsewhere. */
extern Image MyImage;

void RestoreStack();

#if defined(HPUX10)
extern "C" void Checkpoint( int, siginfo_t *, void * );
#else
extern "C" void Checkpoint( int, int, void * );
#endif

extern "C" {
	void ckpt();
	void restart();
	void init_image_with_file_name( char *ckpt_name );
	void init_image_with_file_descriptor( int ckpt_fd );
	void init_image_with_librestart(int fd, RAW_ADDR env,
									RAW_ADDR filetab,
									RAW_ADDR syssock);
	void _condor_prestart( int syscall_mode );
	void Suicide();
}

#define DUMP( leader, name, fmt ) \
	printf( "%s%s = " #fmt "\n", leader, #name, name )


#include "condor_fix_setjmp.h"


long data_start_addr();
long data_end_addr();
long stack_start_addr();
long stack_end_addr();
BOOL StackGrowsDown();
int JmpBufSP_Index();
void ExecuteOnTmpStk( void (*func)() );
void patch_registers( void  *);
#if defined(Solaris) || defined(IRIX53)
     int find_map_for_addr(caddr_t addr);
     int num_segments( );
     int segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end,
	int &prot );
     void display_prmap();
	 extern "C" int open_ckpt_file( const char *name,
								   int flags, size_t n_bytes );
#endif
#if defined(LINUX)
extern "C" {
     int find_map_for_addr(long addr);
     int num_segments( );
     int segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end,
	int &prot );
     void display_prmap();
     unsigned long find_correct_vm_addr(unsigned long, unsigned long, int);
};
#endif

#	define JMP_BUF_SP(env) (((long *)(env))[JmpBufSP_Index()])

#endif /* _IMAGE_H */
