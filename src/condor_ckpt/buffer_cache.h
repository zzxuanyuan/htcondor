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

#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include "condor_common.h"
#include "file_state.h"

class CondorBlockInfo;

/**
This class buffers fixed-sized blocks of data for all virtual
files in a process.  When a read or writeback is necessary,
the appropriate unbuffered method is called in the OpenFileTable.
*/

class CondorBufferCache {
public:

	CondorBufferCache( int blocks, int block_size );
	~CondorBufferCache();

	/** read data from a file at a particular offset.  Take
	advantage of the buffer cache if possible.  Reading past
	the length of a file returns data of all zeroes. */

	ssize_t read( CondorFile *owner, off_t offset, char *data, size_t length );

	/** write data to a file at a particular offset.  If the appropriate
	blocks are in the buffer, write to the buffer and write back when
	convenient.  If the appropriate blocks are not in the buffer,
	then write through. */

	ssize_t write( CondorFile *owner, off_t offset, char *data, size_t length );

	/** Invalidate all blocks owned by this file, after flushing
	any dirty blocks. */

	void flush( CondorFile *owner );

	/** Flush all dirty blocks in the cache. Do not invalidate
	clean blocks. */

	void flush();

private:

	void	invalidate( CondorFile *owner, off_t offset, size_t length );
	int	find_block( CondorFile *owner, int order );
	int	find_or_load_block( CondorFile *owner, int order );
	int	find_lru_position( CondorFile *requestor );
	int	make_room( CondorFile *requestor );

	int	write_block( int position );
	int	read_block( CondorFile *owner, int position, int order );

	int		blocks;		// Number of blocks in this object
	int		block_size;	// Size of a block, in bytes
	char		*buffer;	// The buffer data
	CondorBlockInfo	*info;		// Info about each block
	int		time;		// Integer time for lru
};

#endif
