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
#include "image.h"
#include "mm.h"
#include <sys/procfs.h>		// for /proc calls
#include <sys/mman.h>		// for mmap() test
#include "condor_debug.h"
#include "condor_syscalls.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* a few notes:
 * this is a pretty rough port
 *
 * data_start_addr() is basically an educated guess based on dump(1)
 * it is probably not entirely correct, but does seem to work!
 *
 * stack_end_addr() is generally well known for sparc machines
 * however it doesn't seem to appear in solaris header files
 * 
 * JmpBufSP_Index() was derived by dumping the jmp_buf, allocating a
 * large chunk of memory on the stack, and dumping the jmp_buf again
 * whichever value changed by about sizeof(chuck) is the stack pointer
 *
 */

/*
  Return starting address of the data segment
*/
#if defined(X86)
#include <sys/elf_386.h>
#else
#include <sys/elf_SPARC.h>
#endif
extern int _etext;
long
data_start_addr()
{
#if defined(X86)
	return (  (((long)&_etext) + ELF_386_MAXPGSZ) + 0x8 - 1 ) & ~(0x8-1);
#else
	return (  (((long)&_etext) + ELF_SPARC_MAXPGSZ) + 0x8 - 1 ) & ~(0x8-1);
#endif
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	return (long)sbrk(0);
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
	return TRUE;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
#if defined(X86)
	return 4;
#else
	return 1;	
#endif
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	long	answer;

	jmp_buf env;
	(void)SETJMP( env );
	return JMP_BUF_SP(env) & ~1023; // Curr sp, rounded down
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
	/* return 0xF8000000; -- for sun4[c] */
#if defined(X86)
	return 0x8048000; /* -- for x86 */
#else
	return 0xF0000000; /* -- for sun4m */
#endif
}

/*
  Patch any registers whose values should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	// Nothing needed
}

// static prmap_t *my_map = NULL;
static prmap_t my_map[ MAX_SEGS ];
static int	prmap_count = 0;
static int	text_loc = -1, stack_loc = -1, heap_loc = -1;
// static int      mmap_loc = -1;

/*
  Find the segment in my_map which contains the address in addr.
  Used to find the text segment.
*/
int
find_map_for_addr(caddr_t addr)
{
	int		i;

	for (i = 0; i < prmap_count; i++) {
		if (addr >= my_map[i].pr_vaddr &&
			addr <= my_map[i].pr_vaddr + my_map[i].pr_size){
			return i;
		}
	}
	return -1;
}

/*
  Return the number of segments to be checkpointed.  Note that this
  number includes the text segment, which should be ignored.  On error,
  returns -1.
*/
int
num_segments( )
{
	int	fd;
	char	buf[100];

	int scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
	sprintf(buf, "/proc/%d", SYSCALL(SYS_getpid));
	fd = SYSCALL(SYS_open, buf, O_RDWR, 0);
	if (fd < 0) {
		return -1;
	}
	SYSCALL(SYS_ioctl, fd, PIOCNMAP, &prmap_count);
	if (prmap_count > MAX_SEGS) {
		dprintf( D_ALWAYS, "Don't know how to grow segment map yet!\n" );
		Suicide();
	}		
/*	if (my_map != NULL) {
		free(my_map);
	}
	my_map = (prmap_t *) malloc(sizeof(prmap_t) * (prmap_count + 1));  */
	SYSCALL(SYS_ioctl, fd, PIOCMAP, my_map);
	/* find the text segment by finding where this function is
	   located */
	text_loc = find_map_for_addr((caddr_t) num_segments);
	/* identify the stack segment by looking for the bottom of the stack,
	   because the top of the stack may be in the data area, often the
	   case for programs which utilize threads or co-routine packages. */
	if ( StackGrowsDown() )
		stack_loc = find_map_for_addr((caddr_t) stack_end_addr());
	else
		stack_loc = find_map_for_addr((caddr_t) stack_start_addr());
	heap_loc = find_map_for_addr((caddr_t) data_start_addr());
//	mmap_loc = find_map_for_addr((caddr_t) mmap);
	if (SYSCALL(SYS_close, fd) < 0) {
		dprintf(D_ALWAYS, "close: %s", strerror(errno));
	}
	SetSyscalls( scm );
	return prmap_count;
}

/*
  Assigns the bounds of the segment specified by seg_num to start
  and long, and the protections to prot.  Returns -1 on error, 1 if
  this segment is the text segment, 2 if this segment is the stack
  segment, 3 if this segment is in the data segment, and 0 otherwise.
*/
int
segment_bounds( int seg_num, RAW_ADDR &start, RAW_ADDR &end, int &prot )
{
	if (my_map == NULL)
		return -1;
	start = (long) my_map[seg_num].pr_vaddr;
	end = start + my_map[seg_num].pr_size;
	prot = my_map[seg_num].pr_mflags;
//	if (seg_num == mmap_loc)
//	  fprintf(stderr, "Checkpointing segment containing mmap.\n"
//		  "Segment %d (0x%lx - 0x%lx) contains mmap.\n",
//		  seg_num, my_map[seg_num].pr_vaddr,
//		  my_map[seg_num].pr_vaddr+my_map[seg_num].pr_size);
	if (seg_num == text_loc)
		return 1;
	else if (seg_num == stack_loc)
		return 2;
	else if (seg_num == heap_loc ||
		 ((unsigned)my_map[seg_num].pr_vaddr >= (unsigned)data_start_addr() &&
		  (unsigned)my_map[seg_num].pr_vaddr <= (unsigned)data_end_addr()))
	        return 3;
	return 0;
}

struct ma_flags {
	int	flag_val;
	char	*flag_name;
} MA_FLAGS[] = {{MA_READ, "MA_READ"},
				{MA_WRITE, "MA_WRITE"},
				{MA_EXEC, "MA_EXEC"},
				{MA_SHARED, "MA_SHARED"},
				{MA_BREAK, "MA_BREAK"},
				{MA_STACK, "MA_STACK"}};

/*
  For use in debugging only.  Displays the segment map of the current
  process.
*/
void
display_prmap()
{
	int		i, j;

	num_segments();
	for (i = 0; i < prmap_count; i++) {
	  dprintf( D_ALWAYS, "addr = 0x%p, size = 0x%lx, offset = 0x%x",
		 my_map[i].pr_vaddr, my_map[i].pr_size, my_map[i].pr_off);
	  for (j = 0; j < sizeof(MA_FLAGS) / sizeof(MA_FLAGS[0]); j++) {
	    if (my_map[i].pr_mflags & MA_FLAGS[j].flag_val) {
	      dprintf( D_ALWAYS, " %s", MA_FLAGS[j].flag_name);
	    }
	  }
	  dprintf( D_ALWAYS, "\n");
	}
}

#if 0
/* Convert protection flags returned by /proc to protection flags 
   used by mmap(). */
static int
proc_to_mmap_prot(int mflags)
{
	int prot = 0;
	if (mflags & MA_READ)
		prot |= PROT_READ;
	if (mflags & MA_WRITE)
		prot |= PROT_WRITE;
	if (mflags & MA_EXEC)
		prot |= PROT_EXEC;
	return prot;
}
#endif

/* Return the memory map of this process in MR.  LEN is the number of
   regions in the map. */
int
mmap_get_process_mmap(mmap_region *mr, int *len)
{
	prmap_t prmap[MAX_MMAP_REGIONS];
	int num_prmap;
	char buf[32];
	int i, fd;

	/* Read the memory map of this process.  These ioctls are obsolete
	   in Solaris 2.6+, but used for Solaris 2.5.1 compatibility.
	   Only 2.5.1 documents them, in the proc(4) manpage. */
	sprintf(buf, "/proc/%05d", getpid());
	fd = open(buf, O_RDONLY);
	if (0 > fd) {
		dprintf(D_ALWAYS, "Can't open /proc on myself\n");
		sleep(1);
		Suicide();
	}
	if (0 > ioctl(fd, PIOCNMAP, &num_prmap)) {
		dprintf(D_ALWAYS, "Can't read process memory map\n");
		sleep(1);
		Suicide();
	}
	/* PIOCMAP returns NUM_PRMAP + 1 elements; the last one is all
       zeros. */
	if (num_prmap > MAX_MMAP_REGIONS - 1) {
		dprintf(D_ALWAYS, "Exhausted static memory map memory\n");
		sleep(1);
		Suicide();
	}
	if (0 > ioctl(fd, PIOCMAP, prmap)) {
		dprintf(D_ALWAYS, "Can't read process memory map: %s\n");
		sleep(1);
		Suicide();
	}
	close(fd);

	/* Convert to mmap_regions */
	for (i = 0; i < num_prmap; i++) {
		mr[i].mm_addr = (unsigned long) prmap[i].pr_vaddr;
		mr[i].mm_len = (unsigned long) prmap[i].pr_size;
		/* TODO: Convert these flags? */
		mr[i].mm_prot = (int) prmap[i].pr_mflags;
	}

	*len = num_prmap;
	return 0;
}


/* MMAP all pages in MR. */
int
mmap_mmap(mmap_region *mr, int len)
{
	mmap_region *p;
	int fd;

	/* Use syscall to avoid Condor I/O. */
	fd = syscall(SYS_open, "/dev/zero", O_RDWR);

	if (0 > fd) {
		dprintf(D_ALWAYS, "Can't open /dev/zero\n");
		sleep(1);
		Suicide();
	}

	for (p = mr; p <= &mr[len-1]; p++) {
		void *pa = (void *) syscall(SYS_mmap, (caddr_t) p->mm_addr,
									(size_t) p->mm_len,
									PROT_READ,
									MAP_PRIVATE|MAP_FIXED, fd, 0);
		if (MAP_FAILED == pa) {
			dprintf(D_ALWAYS,
					"Can't allocate segment %d (addr = %#010 len = %#010)\n",
					p - mr, p->mm_addr, p->mm_len);
			sleep(1);
			Suicide();
		}
		dprintf(D_ALWAYS, "MM DID MMAP %#010x - %#010x\n",
				p->mm_addr, p->mm_addr + p->mm_len);
	}
	syscall(SYS_close, fd);
	return 0;
}

/* MUNMAP all pages in MR. */
int
mmap_munmap(mmap_region *mr, int len)
{
	mmap_region *p;
	for (p = mr; p <= &mr[len-1]; p++) {
		dprintf(D_ALWAYS, "UNMAPPING: %#010x - %#010x\n",
				p->mm_addr, p->mm_addr + p->mm_len);
		if (0 > syscall(SYS_munmap, p->mm_addr, p->mm_len)) {
			dprintf(D_ALWAYS, "Error unmapping region %#010x - %#010x\n",
					p->mm_addr, p->mm_addr + p->mm_len);
			Suicide();
		}
	}
	return 0;
}


/* Set the brk to NEWBRK.  */
int
mmap_brk(void *newbrk)
{
	if (0 > brk(newbrk)) {
		dprintf(D_ALWAYS, "Can't set the brk to %#010x (%s)\n",
				newbrk, strerror(errno));
		Suicide();
	}
	return 0;
}
