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

#ifndef MM_H
#define MM_H

/* A simple structure for storing memory map information simplifies
   the restart code.  Using SegMaps would require giving C++ linkage
   and runtime support to the dynamically loaded restart library. */
typedef struct {
	unsigned long mm_addr;
	unsigned long mm_len;
	int mm_prot;
} mmap_region;

/* To avoid dynamic memory allocation during restart, assume we'll
   have no more than this many disjoint regions of memory allocated
   any given time while restarting.  */
#define MAX_MMAP_REGIONS  1024

#if defined(__cplusplus)
extern "C" {
#endif

/* Platform-independent operations */
void mmap_print(mmap_region *mr, int len, char *id);
void mmap_canonicalize(mmap_region *mr, int *num);
void mmap_union(mmap_region *a, int lena, mmap_region *b, int lenb,
				mmap_region *u, int *lenu);
void mmap_diff(mmap_region *a, int lena, mmap_region *b, int lenb,
			   mmap_region *d, int *lend);

/* Platform-dependent operations */
int mmap_get_process_mmap(mmap_region *mr, int *len);
int mmap_mmap(mmap_region *mr, int len);
int mmap_munmap(mmap_region *mr, int len);
int mmap_brk(void *);

#if defined(__cplusplus)
}
#endif

#endif /* MM_H */
