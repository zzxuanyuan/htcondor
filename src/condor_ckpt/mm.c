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

#include <stdio.h>
#include <stdlib.h>
#include "condor_common.h"
#include "condor_debug.h"
#include "mm.h"

/* The file defines platform-independent memory map union and
   difference operations.

   They are used to prepare the address space of the restart process
   when restarting checkpoints of processes that have runtime-loaded
   (dynamic) Condor libraries.   */

#define MM_DEBUG
#ifdef MM_DEBUG
static int mmap_validate_union(mmap_region *, int,
							   mmap_region *, int,
							   mmap_region *, int);
static int mmap_validate_diff(mmap_region *, int,
							  mmap_region *, int,
							  mmap_region *, int);
static void mmap_save(mmap_region *, int);
static int mmap_equals_saved(mmap_region *, int);
#endif

#define ENDADDR(mrp)  ((mrp)->mm_addr + (mrp)->mm_len)

/* mmap_region array editing.  Shift the elements following NEXT one
   to the left or right.  Shifting to the left clobbers the element at
   NEXT; shifting to the right increases the length of the array by
   one.  LAST points to the last element in the array; NUM is the total
   number of elements in the array. */
#define MMAPSHIFTLEFT(next,last,num)            \
  do { 											\
	mmap_region *p;								\
	for (p = (next); p < (last); p++)			\
		*p = *(p+1);							\
	(last)--;									\
	(num)--;									\
  } while (0)

/* We can't put preprocessor-expanded symbols in this macro, so the
   MAX_MMAP_REGIONS (1024) and debug level (1) are hardcoded. */
#define MMAPSHIFTRIGHT(next,last,num)								\
  do {																\
	mmap_region *p;													\
	if ((num) + 1 > MAX_MMAP_REGIONS) {  							\
		dprintf(D_ALWAYS, "Exhausted static memory map memory\n");	\
        sleep(1);                                                   \
		Suicide();													\
	}																\
	for (p = (last); p > (next); p--)								\
		*(p+1) = *p;												\
	(last)++;														\
	(num)++;														\
  } while (0)


/* Print MR.  ID is identifying string printed in the header; it can
   be NULL. */
void
mmap_print(mmap_region *mr, int len, char *id)
{
	int i;
	if (id)
		dprintf(D_ALWAYS, "Memory map \"%s\" follows\n", id);
	else
		dprintf(D_ALWAYS, "Memory map follows\n");
	for (i = 0; i < len; i++)
		dprintf(D_ALWAYS,
				"seg[%02d]: addr = %#010x len = %#010x prot = %#04x\n",
				i, mr[i].mm_addr, mr[i].mm_len, mr[i].mm_prot);
}


/* Compare two mmap_region structures. */
static int
mmap_cmp(const void *ap, const void *bp)
{
	mmap_region *a = (mmap_region *)ap;
	mmap_region *b = (mmap_region *)bp;
	unsigned long a_addr = a->mm_addr;
	unsigned long b_addr = b->mm_addr;
	unsigned long a_len = a->mm_len;
	unsigned long b_len = b->mm_len;

	if (a_addr < b_addr)
		return -1;
	if (a_addr > b_addr)
		return 1;
	if (a_len < b_len)
		return -1;
	if (a_len > b_len)
		return 1;
	return 0;
}


/* Return non-zero if A and B intersect.  Regions are defined to be
   closed at the start address, open at the end address.  (E.g., if A
   ENDS at 0x1000 and B STARTS at 0x1000, A and B do NOT intersect.)  */
static int
mmap_intersectp(mmap_region *a, mmap_region *b)
{
	if (a->mm_addr == b->mm_addr)
		return 1;
	if (a->mm_addr < b->mm_addr && ENDADDR(a) > b->mm_addr)
		return 1;
	if (b->mm_addr < a->mm_addr	&& ENDADDR(b) > a->mm_addr)
		return 1;
	return 0;
}


/* Return non-zero if A and B are adjoined regions (i.e., the union of
   A and B is contiguous.)  A is assumed to be ordered before B, in
   the sense of mmap_cmp, and A is assumed to be disjoint from B.  A
   and B are not adjoined if they have disagreeing protection. */
static int
mmap_adjoinedp(mmap_region *a, mmap_region *b)
{
	/* Check protection. */
	if (a->mm_prot != b->mm_prot)
		return 0;

	if (b->mm_addr == ENDADDR(a))
		return 1;

	return 0;
}


/* The input A and B must point to intersecting regions.  Output array
   C must point to space for at least two mmap_regions.  Returns the
   number of disjoint regions in the difference A-B; these regions are
   stored in C.  If more than one region is returned, they are ordered
   in C as defined by mmap_cmp. */
static int
mmap_region_diff(mmap_region *a, mmap_region *b, mmap_region *c)
{
	c[0].mm_prot = a->mm_prot;
	c[1].mm_prot = a->mm_prot;
	if (a->mm_addr == b->mm_addr) {
		if (ENDADDR(a) > ENDADDR(b)) {
			/* A    |---------|
			   B    |-----|          */
			c[0].mm_len  = a->mm_len - b->mm_len;
			c[0].mm_addr = ENDADDR(b);
			return 1;
		} else {
			/* A    |-----|      or   |-----|
               B    |---------|       |-----|  */
			return 0;
		}
	}

	if (a->mm_addr > b->mm_addr) {
		if (ENDADDR(a) > ENDADDR(b)) {
			/* A    |-----|
               B  |-----|          */
			c[0].mm_len  = a->mm_len - (ENDADDR(b) - a->mm_addr);
			c[0].mm_addr = ENDADDR(b);
			return 1;
		} else {
			/* A    |--|    or     |---| 
               B  |-----|		 |-----|    */
			return 0;
		}
	}

	/* Else case: a->mm_addr < b->mm_addr */
	if (ENDADDR(a) > ENDADDR(b)) {
		/* A    |-------|
		   B      |---|      */
		c[0].mm_len  = b->mm_addr - a->mm_addr;
		c[0].mm_addr = a->mm_addr;
		c[1].mm_len  = ENDADDR(a) - ENDADDR(b);
		c[1].mm_addr = ENDADDR(b); 
        return 2;
	} else {
		/* A    |----|   or  |------|
		   B      |---|        |----|  */
		c[0].mm_len  = b->mm_addr - a->mm_addr;
		c[0].mm_addr = a->mm_addr;
		return 1;
	}
}

/* The input MR is an array of NUM mmap_regions.  Upon return, MR is a
   cannonicalized set of mmap_regions covering exactly the same area
   as the input MR, with the following properties:

   (sorted by address)      if i < j then mr[i].mm_addr < mr[j].mm_addr
   (disjoint)               for all i,j mr[i] does not intersect mr[j]
   (minimal wrt protection) if mr[i] and mr[i+1] are adjoined, then
                            mr[i] and mr[i+1] have disagreeing protection
							(they cannot be combined without creating
							 a mmap_region with inconsistent protection)

   Upon return, NUM is the new length of MR.

   It is assumed that the input MR points to enough memory to hold the
   output MR.

   Note that it is possible for the input MR to contain two or more
   intersecting mmap_regions with disagreeing mm_prot fields.  The
   mm_prot fields of these intersections is undefined.  In practice,
   this is okay because either:

     The elements of MR come from a single source, such as a
     checkpoint header or a process memory map.  In this case,
     intersecting regions with disagreeing protection can't occur in
     valid input.

     Or, the elements of MR come from two or more sources.  In this
     case, MR will be used only for calculations about memory
     allocation, not to actually allocate memory.  Disambiguating
     protection conflicts is unnecessary.  */
void
mmap_canonicalize(mmap_region *mr, int *num_mr)
{
	mmap_region *curr, *next, *last;
	int num = *num_mr;
	int nd;
	mmap_region diff[2];

#ifdef MM_DEBUG
	mmap_save(mr, *num_mr);
#endif

	if (num < 2) return;

	qsort(mr, num, sizeof(mmap_region), mmap_cmp);

	/* Check consecutive pairs of elements in MR for intersection and
	   adjoinedness.  When an intersecting or adjoined pair is found,
	   correct it. */
	curr = &mr[0];
	next = &mr[1];
	last = &mr[num-1];
	while (curr < last) {
		/* The ordering defined in mmap_cmp holds here.  Modifications
		   to MR within this loop will preserve the ordering. */
		   
		if (mmap_intersectp(curr, next)) {
			/* Don't increment CURR or NEXT at the end of this block;
			   the reorganization we do here could make CURR and NEXT
			   adjoining, and we'll check that on the next
			   iteration. */

			if (curr->mm_addr == next->mm_addr) {
				/* By mmap_cmp order, CURR is smaller than NEXT. */
				nd = mmap_region_diff(next, curr, diff);
				if (nd == 0)
					/* CURR and NEXT are the same region.  NEXT is
					   redundant. */
					MMAPSHIFTLEFT(next, last, num);
				else
					*next = diff[0];
				continue;
			}

			/* NEXT starts after CURR.  There are three cases. */
			/* CASE 1:  CURR   |------|
			            NEXT     |----|      */
			if (ENDADDR(curr) == ENDADDR(next)) {
			    mmap_region_diff(curr, next, diff);
				*curr = diff[0];
				continue;
			}

			/* CASE 2:  CURR   |------|
			            NEXT     |-------|   */
			if (ENDADDR(curr) < ENDADDR(next)) {
				unsigned long currend = ENDADDR(curr);
				MMAPSHIFTRIGHT(next, last, num);

				mmap_region_diff(next, curr, diff);
				*(next+1) = diff[0];

				mmap_region_diff(curr, next, diff);
				*curr = diff[0];

				next->mm_len = currend - next->mm_addr;

				/* We may have destroyed the ordering of elements
                   after CURR (e.g., if before this block,
                   (STARTADDR(NEXT+1) == ENDADDR(CURR) &&
                   ENDADDR(NEXT+1) < ENDADDR(NEXT))). */
				qsort(mr, num, sizeof(mmap_region), mmap_cmp);				

				continue;
			}

			/* CASE 3:  CURR   |------|
			            NEXT     |--|        */
			MMAPSHIFTRIGHT(next, last, num);
			mmap_region_diff(curr, next, diff);
			*curr = diff[0];
			*(next+1) = diff[1];

			/* We may have destroyed the ordering of elements after
               CURR (e.g., if before this block,
			   (STARTADDR(NEXT+1) == ENDADDR(NEXT) &&
			   ENDADDR(NEXT+1) < ENDADDR(CURR))). */
			qsort(mr, num, sizeof(mmap_region), mmap_cmp);				

			continue;
		}

		if (mmap_adjoinedp(curr, next)) {
			/* Don't increment CURR or NEXT at the end of this block;
			   the consolidation could make CURR and the new NEXT
			   adjoining, and we'll check that on the next
			   iteration. */

			/* Consolidate NEXT into CURR. */
			curr->mm_len += next->mm_len;
			/* NEXT is now redundant.  */
			MMAPSHIFTLEFT(next, last, num);
			continue;
		}	
		
		curr++;
		next++;
	}	
	
	*num_mr = num;
#ifdef MM_DEBUG
	if (! mmap_equals_saved(mr, *num_mr)) {
		dprintf(D_ALWAYS, "MMAP DEBUG canonicalize failed\n");
		sleep(1);
		Suicide();
	}
	dprintf(D_ALWAYS, "MMAP DEBUG canonicalize validated\n");
#endif

}

/* Temporary space for mmap_union */
static mmap_region tmpmmap[2*MAX_MMAP_REGIONS];
static int lentmp;

/* Return the canonicalized union of A and B in U, and the length of U
   in LENUP.  A and B need not be canonical. */
void
mmap_union(mmap_region *a, int lena,
		   mmap_region *b, int lenb,
		   mmap_region *u, int *lenup)
{
	if (lena + lenb > 2*MAX_MMAP_REGIONS) {
		dprintf(D_ALWAYS, "Exhausted static memory map memory\n");
		sleep(1);
		Suicide();
	}
	memcpy(tmpmmap, a, lena * sizeof(mmap_region));
	memcpy(&tmpmmap[lena], b, lenb * sizeof(mmap_region));
	lentmp = lena + lenb;
	mmap_canonicalize(tmpmmap, &lentmp);
	if (lentmp > MAX_MMAP_REGIONS) {
		dprintf(D_ALWAYS, "Exhausted static memory map memory\n");
		sleep(1);
		Suicide();
	}
	memcpy(u, tmpmmap, lentmp * sizeof(mmap_region));
	*lenup = lentmp;

#ifdef MM_DEBUG
	if (! mmap_validate_union(a, lena, b, lenb, u, *lenup)) {
		dprintf(D_ALWAYS, "MMAP DEBUG: union failed\n");
		mmap_print(u, *lenup, "Failed Union");
		sleep(1);
		Suicide();
	}
	dprintf(D_ALWAYS, "MMAP DEBUG: Union validated\n");
#endif
}


/* Return the cannonicalized difference A - B in D, and the length of
   D in LENDP.  A and B must be canonical.  */
void
mmap_diff(mmap_region *mra, int lena,
		  mmap_region *mrb, int lenb,
		  mmap_region *mrd, int *lendp)
{
	int i, j;
	int lendiff;                       /* Length of difference list */
	mmap_region wkl[MAX_MMAP_REGIONS]; /* Worklist */
	int lenwkl;                        /* Worklist length */
	mmap_region *p;                    /* Current worklist element */
	mmap_region *q;                    /* Element of MRB */

	/* The worklist contains all elements of A, or subdivisions of
       elements of A, that need to be checked for intersection against
       the elements of B.  When a non-intersecting element is found in
       the worklist, it is inserted into the difference list MRD.
       When a worklist element P is found to insersect with an element
       Q of B, the P is replaced with (P-Q) in the worklist, and the
       intersection of P and Q is discarded.  The worklist is operated
       on like a stack; the order of elements in the worklist is
       unimportant. */

	/* Initialize the worklist to the elements of A */
	memcpy(wkl, mra, lena * sizeof(mmap_region));
	lenwkl = lena;
	
	lendiff = 0;

	while (lenwkl > 0) {
		int intersects = 0;

		/* Pop current element from worklist */
		p = &wkl[--lenwkl];

		/* Test for intersection against elements in MRB.
		   Stop when an intersection is found. */
		for (q = mrb; q <= &mrb[lenb-1]; q++) {
			if (mmap_intersectp(p, q)) {
				int nd;
				mmap_region diff[2];
				intersects = 1;
				nd = mmap_region_diff(p, q, diff);
				if (lenwkl + nd > MAX_MMAP_REGIONS) {
					dprintf(D_ALWAYS, "Exhausted static memory map memory\n");
					sleep(1);
					Suicide();
				}
				/* Push P-Q into the worklist */
				while (nd > 0)
					wkl[lenwkl++] = diff[--nd];
				break;
			} 
		}
		if (! intersects) {
			/* P had no intersection with any element of B.  Insert
			   it in the difference list.  */
			mrd[lendiff++] = *p;
		}
	}

	mmap_canonicalize(mrd, &lendiff);
	*lendp = lendiff;

#ifdef MM_DEBUG
	if (! mmap_validate_diff(mra, lena, mrb, lenb, mrd, *lendp)) {
		dprintf(D_ALWAYS, "MMAP DEBUG: diff failed\n");
		mmap_print(mrd, *lendp, "Failed Diff");
		sleep(1);
		Suicide();
	}
	dprintf(D_ALWAYS, "MMAP DEBUG: Difference validated\n");
#endif
}

#ifdef MM_DEBUG

/* These routines are tests for mmap_canonicalize, mmap_union and
   mmap_diff. */

/* Return non-zero if the ADDR is contained in some region of MR. */
static int
mmap_contains_addr(unsigned long addr, mmap_region *mr, int len)
{
	mmap_region *p;
	for (p = mr; p <= &mr[len-1]; p++)
		if (p->mm_addr <= addr && addr < ENDADDR(p))
			return 1;
	return 0;
}


/* Return non-zero if X is a subset of Y. */
static int
mmap_validate_subset(mmap_region *x, int lenx,
					 mmap_region *y, int leny)
{
	mmap_region *xp, *yp;
	for (xp = x; xp <= &x[lenx-1]; xp++) {
		int okay = 0;
		for (yp = y; yp <= &y[leny-1]; yp++)
			if (xp->mm_addr >= yp->mm_addr && xp->mm_addr < ENDADDR(yp)) {
				/* XP starts in YP */
				if (ENDADDR(xp) <= ENDADDR(yp))
					okay = 1;
				else {
					/* XP must spill into one or more regions adjoined
                       to YP. */
					mmap_region *zp = yp + 1;
					/* Don't call mmap_adjoinedp -- it checks protection */
					while (zp <= &y[leny-1] && zp->mm_addr == ENDADDR(yp)) {
						if (ENDADDR(xp) <= ENDADDR(zp)) {
							okay = 1;
							break;
						}
						yp++;
						zp++;
					}
				}
				break;
			}
		if (! okay) {
			dprintf(D_ALWAYS,
					"Subset failed seg %d (addr = %#010x  len = %#010x)\n",
					xp - x, xp->mm_addr, xp->mm_len);
			return 0;
		}
	}
	return 1;
}

/* Return non-zero if U is the union of A and B. */
static int
mmap_validate_union(mmap_region *a, int lena,
					mmap_region *b, int lenb,
					mmap_region *u, int lenu)
{
	mmap_region *p;
	unsigned long addr;

	/* Check that U contains A and B */
	/* dprintf(D_ALWAYS, "Validate UNION: Subset(a,u)\n"); */
	if (! mmap_validate_subset(a, lena, u, lenu))
		return 0;
	/* dprintf(D_ALWAYS, "Validate UNION: Subset(b,u)\n"); */
	if (! mmap_validate_subset(b, lenb, u, lenu))
		return 0;

	/* Check that everything in U is in either A or B */
	/* dprintf(D_ALWAYS, "Validate UNION: Subset(u,union(a,b))\n"); */
	for (p = u; p <= &u[lenu-1]; p++)
		/* We assume that regions are comprised of pages of size no
           smaller that 0x2000.  Here we check that all pages in P are
           in either A or B. */
		for (addr = p->mm_addr; addr < ENDADDR(p); addr += 0x2000) {
			int ina = mmap_contains_addr(addr, a, lena);
			int inb = mmap_contains_addr(addr, b, lenb);
			if (!ina && !inb) {
				dprintf(D_ALWAYS,
					"Validate UNION failed U addr %#010 (seg %d) (%s, %s)\n",
						p - u,
						ina ? "INA" : "NOT INA",
						inb ? "INB" : "NOT INB");
				return 0;
			}
		}
	return 1;
}

/* Return non-zero if D is the difference A-B. */
static int
mmap_validate_diff(mmap_region *a, int lena,
				   mmap_region *b, int lenb,
				   mmap_region *d, int lend)
{
	mmap_region *p;
	unsigned long addr;

	/* Every page in A must be in B and NOT in D, or vice versa. */
	for (p = a; p < &a[lena-1]; p++)
		for (addr = p->mm_addr; addr < ENDADDR(p); addr += 0x2000) {
			int inb = mmap_contains_addr(addr, b, lenb);
			int ind = mmap_contains_addr(addr, d, lend);
			if (inb == ind) {
				dprintf(D_ALWAYS,
					"Validate DIFF failed A addr %#010 (seg %d) (%s, %s)\n",
						p - a,
						inb ? "INB" : "NOT INB",
						ind ? "IND" : "NOT IND");
				return 0;
			}
		}

	/* Every page in D must be in A and NOT in B. */
	for (p = d; p < &d[lend-1]; p++)
		for (addr = p->mm_addr; addr < ENDADDR(p); addr += 0x2000) {
			int ina = mmap_contains_addr(addr, a, lena);
			int inb = mmap_contains_addr(addr, b, lenb);
			if (inb || !ina) {
				dprintf(D_ALWAYS,
					"Validate UNION failed D addr %#010 (seg %d) (%s, %s)\n",
						p - d,
						ina ? "INA" : "NOT INA",
						inb ? "INB" : "NOT INB");
				return 0;
			}
		}
	return 1;
}

static mmap_region mmapsaved[2*MAX_MMAP_REGIONS];
static int numsaved;

/* Save A for comparison with MMAP_EQUALS_SAVED. */
static void
mmap_save(mmap_region *a, int lena)
{
	memcpy(mmapsaved, a, lena * sizeof(mmap_region));
	numsaved = lena;
}

/* Compare A for equality to the last mmap saved by mmap_save. */
static int
mmap_equals_saved(mmap_region *a, int lena)
{
	mmap_region *p;
	unsigned long addr;
	
	/* Check that A is a subset of SAVED */
	for (p = a; p < &a[lena-1]; p++)
		for (addr = p->mm_addr; addr < ENDADDR(p); addr += 0x2000)
			if (! mmap_contains_addr(addr, mmapsaved, numsaved)) {
				dprintf(D_ALWAYS, "A is not a subset of SAVED\n");
				dprintf(D_ALWAYS,
						"failed segment %d (addr %#010x - %#010x)\n",
						p - a, p->mm_addr, ENDADDR(p));
				mmap_print(a, lena, "Broken A");
				mmap_print(mmapsaved, numsaved, "Broken SAVED");
				return 0;
			}

	/* Check that SAVED is a subset of A */
	for (p = mmapsaved; p < &mmapsaved[numsaved-1]; p++)
		for (addr = p->mm_addr; addr < ENDADDR(p); addr += 0x2000)
			if (! mmap_contains_addr(addr, a, lena)) {
				dprintf(D_ALWAYS, "SAVED is not a subset of A\n");
				dprintf(D_ALWAYS,
						"failed segment %d (addr %#010x - %#010x)\n",
						p - mmapsaved, p->mm_addr, ENDADDR(p));
				mmap_print(a, lena, "Broken A");
				mmap_print(mmapsaved, numsaved, "Broken SAVED");
				return 0;
			}
			
	return 1;
}

#endif /* MM_DEBUG */
