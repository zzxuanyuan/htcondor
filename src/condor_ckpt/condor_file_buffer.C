
#include "condor_file_buffer.h"
#include "condor_file_warning.h"
#include "file_state.h"

class CondorChunk {
public:
	CondorChunk( int b, int s ) {
		begin = b;
		size = s;
		last_used = 0;
		dirty = 0;
		next = 0;
		data = new char[size];
	}

	~CondorChunk() {
		delete [] data;
	}

	int begin;
	int size;
	int last_used;
	char *data;
	int dirty;
	CondorChunk *next;
};

CondorChunk * combine( CondorChunk *a, CondorChunk *b )
{
	int begin, end;
	CondorChunk *r;

	begin = MIN(a->begin,b->begin);
	end = MAX(a->begin+a->size,b->begin+b->size);

	r = new CondorChunk(begin,end-begin);

	r->dirty = a->dirty && b->dirty;
	r->last_used = MAX( a->last_used, b->last_used );

	if( a->dirty && !b->dirty ) {
		memcpy( &r->data[b->begin-begin], b->data, b->size );
		memcpy( &r->data[a->begin-begin], a->data, a->size );
	} else {
		memcpy( &r->data[a->begin-begin], a->data, a->size );
		memcpy( &r->data[b->begin-begin], b->data, b->size );
	}

	return r;
}

int overlaps( CondorChunk *a, CondorChunk *b ) 
{
	return
		(
			(a->begin>=b->begin) &&
			(a->begin<(b->begin+b->size))
		) || (
			(b->begin>=a->begin) &&
			(b->begin<(a->begin+a->size))
		);
}

int adjacent( CondorChunk *a, CondorChunk *b )
{
	return
		( (a->begin+a->size)==b->begin )
		||
		( (b->begin+b->size)==a->begin )
		;
}


int contains( CondorChunk *c, int position )
{
	return
		(c->begin<=position)
		&&
		((c->begin+c->size)>position)
		;
}

int should_combine( CondorChunk *a, CondorChunk *b )
{
	return
		overlaps(a,b)
		||
		(	a->dirty
			&&
			b->dirty
			&&
			( (a->size+b->size) < FileTab->get_buffer_block_size() )
			&&
			adjacent(a,b)
		)
		;
}


CondorChunk * absorb( CondorChunk *head, CondorChunk *c )
{
	CondorChunk *next, *combined;

	if(!head) return c;

	if( should_combine( head, c ) ) {
		next = head->next;
		combined = combine( head, c );
		delete head;
		delete c;
		return absorb( next, combined );
	} else {
		head->next = absorb( head->next, c );
		return head;
	}
}

CondorFileBuffer::CondorFileBuffer( CondorFile *o )
{
	init();
	kind = "buffer";
	original = o;
	head = 0;
	time = 0;
}

CondorFileBuffer::~CondorFileBuffer()
{
	delete original;
}

void CondorFileBuffer::dump()
{
	original->dump();
}

int CondorFileBuffer::open( const char *path, int flags, int mode )
{
	int result;

	result = original->open(path,flags,mode);

	strcpy(name,original->get_name());

	readable = original->is_readable();
	writeable = original->is_writeable();
	size = original->get_size();

	return result;
}

int CondorFileBuffer::close()
{
	flush(1);
	report(1);
	return original->close();
}

int CondorFileBuffer::read(int offset, char *data, int length)
{
	CondorChunk *c=0;
	int piece=0;
	int bytes_read=0;
	int hole_top;

	read_count++;

	// If the user is attempting to read past the end
	// of the file, chop off that access here.

	if((offset+length)>size) {
		length = size-offset;
	}

	while(length) {

		// hole_top keeps track of the lowest starting data point
		// in case we have created a virtual hole

		hole_top = MIN( size, offset+length );

		// Scan through all the data chunks.
		// If one overlaps with the beginning of the
		// request, then copy that data.

		for( c=head; c; c=c->next ) {
			if( contains(c,offset) ) {
				piece = MIN(c->begin+c->size-offset,length);
				memcpy(data,&c->data[offset-c->begin],piece);
				offset += piece;
				data += piece;
				length -= piece;
				bytes_read += piece;
				c->last_used = time++;
				break;
			} else {
				if((c->begin<hole_top)&&(c->begin>offset)) {
					hole_top = c->begin;
				}
			}
		}

		// If that worked, try it again.

		if(c) continue;

		// Now, consider the logical size of the buffer file
		// and the size of the actual file.  If we are less
		// than the former, but greater than the latter, simply
		// fill the hole with zeroes and continue above.

		piece = hole_top-offset;

		if( offset<size && offset>=original->get_size() ) {
			memset(data,0,piece);
			offset += piece;
			data += piece;
			length -= piece;
			bytes_read += piece;
			continue;
		}

		// Otherwise, make a new chunk.  Try to read a
		// full block of data, but don't waste time by reading
		// past hole_top.

		c = new CondorChunk(offset,MIN(FileTab->get_buffer_block_size(),piece));
		piece = original->read(offset,c->data,c->size);
		if(piece<0) {
			delete c;
			if(bytes_read==0) bytes_read=-1;
			break;
		} else if(piece==0) {
			delete c;
			break;
		} else {
			c->size = piece;
			head = absorb( head, c );
		}
	}

	trim();

	if(bytes_read>0) read_bytes += bytes_read;

	return bytes_read;
}

int CondorFileBuffer::write(int offset, char *data, int length)
{
	CondorChunk *c=0;

	write_count++;

	c = new CondorChunk(offset,length);
	memcpy(c->data,data,length);
	c->dirty = 1;
	c->last_used = time++;

	head = absorb( head, c );

	trim();

	if((offset+length)>get_size()) {
		set_size(offset+length);
	}

	if(length>0)  {
		write_bytes += length;
	}

	return length;
}

int CondorFileBuffer::fcntl( int cmd, int arg )
{
	return original->fcntl(cmd,arg);
}

int CondorFileBuffer::ioctl( int cmd, int arg )
{
	return original->ioctl(cmd,arg);
}

int CondorFileBuffer::ftruncate( size_t length )
{
	flush(1);
	size = length;
	return original->ftruncate(length);
}

int CondorFileBuffer::fsync()
{
	flush(0);
	return original->fsync();
}

void CondorFileBuffer::checkpoint()
{
	flush(0);
	report(0);
	original->checkpoint();
}

void CondorFileBuffer::suspend()
{
	flush(0);
	report(0);
	original->suspend();
}

void CondorFileBuffer::resume(int count)
{
	original->resume(count);
}

int CondorFileBuffer::map_fd_hack()
{
	return original->map_fd_hack();
}

int CondorFileBuffer::local_access_hack()
{
	return original->local_access_hack();
}

void CondorFileBuffer::trim()
{
	CondorChunk *best_chunk,*i;

	best_chunk = head;

	while(1) {
		int space_used = 0;
		double best_ratio = 0;

		for( i=head; i; i=i->next ) {
			int ratio = benefit_cost(i);
			if(ratio>best_ratio) {
				best_ratio = ratio;
				best_chunk = i;
			}
			space_used += i->size;

		}

		if( space_used <= FileTab->get_buffer_size() ) return;

		evict( best_chunk );
	}
}

#define LATENCY   (.001)
#define BANDWIDTH (10.0*1024*1024/8)

double CondorFileBuffer::benefit_cost( CondorChunk *c )
{
	double csize = c->size;
	return (csize*(time-c->last_used+1)) /( (csize/size + c->dirty)*(LATENCY+csize/BANDWIDTH) );
}

void CondorFileBuffer::flush( int deallocate )
{
	CondorChunk *c,*n;

	for(c=head;c;c=n) {
		clean(c);
		n = c->next;
		if(deallocate) {
			delete c;
		}
	}

	if(deallocate) head=0;
}

void CondorFileBuffer::clean( CondorChunk *c )
{
	if(c && c->dirty) {
		int result = original->write(c->begin,c->data,c->size);
		if(!result) _condor_file_warning("Unable to write buffered data to %s!",original->get_name());
		c->dirty = 0;
	}
}

void CondorFileBuffer::evict( CondorChunk *c )
{
	CondorChunk *i;

	clean(c);

	if( c==head ) {
		head = c->next;
		delete c;
	} else {
		for( i=head; i; i=i->next ) {
			if( i->next==c ) {
				i->next = c->next;
				delete c;
			}
		}
	}
}
