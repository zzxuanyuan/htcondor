#include <iostream.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <sys/mman.h>
#include "/s/zlib/include/zlib.h"
#include "secondattempt.h"

//these functions get used by zlib.
void *
zalloc(voidpf opaque, uInt items, uInt size)
{
	malloc(size*items);
}
void
zfree(voidpf opaque, voidpf address)
{
	free(address);
}


int fd1, fd2; //file descriptors for first and second file, respectively.
CheckpointFile f1, f2; //checkpoint files for each ones.
z_stream *pz1, *pz2; //checkpoint compression crap.
char * paddr1, *paddr2; //locations of *UNCOMPRESSED* mmaped files.


//assumes that pages are decompressed
int compare_pages(char * p1, char *p2, int &decile) {
	int bytes_matching=0;
	for (int i=0; i<getpagesize(); i++) {
		if (*p1++==*p2++) {
			bytes_matching++;
		}
	}
	
	decile= (int)(10.0*bytes_matching/(float)getpagesize());
	if (decile==10 ){
		cout<<"VERBOSE: PAGE MATCH"<<endl;
	}
	else {
		cout<<"VERBOSE: PAGE NOMATCH "<<
			bytes_matching<<"/"<<getpagesize()<<" "<<
			(bytes_matching/(float) getpagesize())<<endl;
	}
	return decile==10;
}
int compare_segment(CheckpointFile &ck1, CheckpointFile &ck2, char * paddr1, 
					char * paddr2, int segnumber) {
	int pgsize=getpagesize();
	//we are going to read this a page at a time:
	int bytes_to_read1=ck1.segmap[segnumber].len;
	int bytes_to_read2=ck2.segmap[segnumber].len;
	int pages_matching=0;
	int total_pages=0;
	char * p1, *p2;
	p1=paddr1+ck1.segmap[segnumber].file_loc;
	p2=paddr2+ck2.segmap[segnumber].file_loc;
	int histogram[11];
	memset(histogram, 0, 11*sizeof(int));
	while (bytes_to_read1>0 && bytes_to_read2>0) {
		int decileval=0;
		if (compare_pages(p1, p2, decileval)==1) {
			pages_matching++;
		}
		total_pages++;
		bytes_to_read1-=pgsize;
		bytes_to_read2-=pgsize;
		p1+=pgsize;
		p2+=pgsize;
		histogram[decileval]++;
	}
	cout<<"Pages matching "<<pages_matching <<" Total Pages"<<total_pages<<endl;
	cout<<"Histogram: ";
	for (int i=0; i<11; i++) cout<<histogram[i]<<"  ";
	cout<<endl;
	return pages_matching==total_pages;
}			

//side effect--seeks to the beginning of the file
int read_header(int fd, CheckpointFile &cf) {
	int retval=lseek(fd, 0, SEEK_SET);
	int vals[2];
	//read two bytes
	retval=read(fd, vals, 2*sizeof(int));
	if (retval!=2*sizeof(int)) {
		cerr<<"ERROR READING HEADER"<<endl;
	}
	cf.compressed=vals[0]==cf.COMPRESS_MAGIC;
	cf.n_segs=vals[1];
	cout <<"cf info"<<cf.compressed<<" "<<cf.n_segs<<endl;
	return 0;
}
//assumes that the checkpoint is created!
int read_segmap(int fd, CheckpointFile &cf) {
	cerr<<"fd="<<fd;
	int retval=lseek(fd,  HEADER_LENGTH, SEEK_SET);
	if (retval<0 ){
		cerr<<"ERROR!!!"<<endl;
		return -1;
	}
	//this is tricky, but should work:
	retval=read(fd, cf.segmap, cf.n_segs*sizeof(SegInfo));
	cout<<"Read "<<retval<<"bytes"<<endl;
}




#define DUMP( leader, name, fmt ) \
	printf( "%s%s = " #fmt "\n", leader, #name, name )

void SegInfo::Display(){
	DUMP( " ", name, %s );
	printf( " file_loc = %Lu (0x%X)\n", file_loc, file_loc );
	printf( " core_loc = %Lu (0x%X)\n", core_loc, core_loc );
	printf( " len = %d (0x%X)\n", len, len );
}

//note: this is a pointer.
z_stream * initialize_zstream(z_stream *pz) {
	pz= (z_stream *) malloc(sizeof(z_stream));
	pz->zalloc=zalloc;
	pz->zfree=zfree;
	pz->opaque=NULL;
	inflateInit(pz);
	return pz;	
}	
	
int setup_uncompressed_addr (int fd, CheckpointFile &ck, z_stream *pz, char * &paddr) {
	unsigned long needed_len=ck.segmap[ck.n_segs-1].file_loc+ck.segmap[ck.n_segs-1].len;
	//amount of space that we need for this file.
	
	int first_segment_address=ck.segmap[0].file_loc;

	cerr<<"Needed Length "<<needed_len<<endl;
	if (ck.compressed) {
		char * paddrtemp;
		//hack--determine the length of this file
		long cur_loc=tell(fd);
		long end_loc=lseek(fd, 0, SEEK_END);
		cerr<<"end_location is "<<end_loc<<endl;
		lseek(fd, cur_loc, SEEK_SET);
		//we should have a pointer to the beginning of a compressed checkpoint
		paddrtemp=mmap(0, end_loc, PROT_READ, MAP_PRIVATE, fd, 0);
		if (paddrtemp==MAP_FAILED) {
			cerr<<"MAP FaILED"<<endl;
		}
		else {
			cerr<<"Address of temporary map: "<<(unsigned long)paddrtemp<<endl;
		}

		//ok. I wonder if this will work...
		paddr=(char *) malloc(needed_len);
		if (paddr==NULL) {
			cerr<<"malloc of "<<needed_len<<" bytes failed!"<<endl;
			return -1;
		}
		else {
			cerr<<" address of final area "<< (unsigned long) paddr<<endl;
		}
		//now lets start decompressing...
		//I guess we'll just copy everything before the segments themselves...
		memcpy(paddr, paddrtemp, first_segment_address);
		pz->total_out=0;
		pz->total_in=0;
		pz->next_in= (Bytef*)   (unsigned long) (   paddrtemp+first_segment_address);
		pz->avail_in=end_loc-first_segment_address;
		pz->next_out=(Bytef*) (unsigned long)(paddr+first_segment_address);
		pz->avail_out=needed_len-first_segment_address;
		//pz->avail_out=getpagesize();
		//now decompress;
		int xxx=0;
		/*
		cerr<<"avail in "<<pz->avail_in<<endl;
		while ((xxx=inflate(pz, Z_SYNC_FLUSH))==Z_OK) {
			pz->avail_out=getpagesize();	
			pz->next_out=(Bytef*) (unsigned long)(paddr+first_segment_address+pz->total_out);
			cerr<<xxx<<" Def "<<pz->total_out<<" read "<<pz->total_in<<" avail in "<<pz->avail_in<<endl;
		} 
		*/
		xxx=inflate(pz, Z_FINISH);
		cerr<<xxx<<" Def "<<pz->total_out<<" read "<<pz->total_in<<" avail in "<<pz->avail_in<<endl;
		cerr<<"xxx "<<xxx<<endl;
		//now we should be done, unmap the file
		if (munmap(paddrtemp, end_loc)!=0 ){
			cerr<<"UNMAP FIALED"<<endl;
		}
	}
	else { //non compressed file...
		paddr=mmap(0, needed_len, PROT_READ, MAP_FIXED, fd, 0);
		if (paddr==MAP_FAILED ){
			cerr<<"MAP FAILED"<<endl;
		}
	}
	return 0;
}


		
		


	






//first thing is file
int main(int argc, char ** argv ) {
	
//	if (argc!=2) {exit(1);}	
	fd1=open(argv[1], O_RDONLY);
	fd2=open(argv[2], O_RDONLY);

	//ok. lets try this:
	read_header(fd1, f1);
	read_header(fd2, f2);
	
	read_segmap(fd1, f1);
	read_segmap(fd2, f2);
	
	//for (int i=0; i<f1.n_segs; i++) {
	//f1.segmap[i].Display();
	//}

	pz1=initialize_zstream(pz1);
	pz2=initialize_zstream(pz2);

	cerr<<pz1;
	paddr1=NULL;
	paddr2=NULL;
	setup_uncompressed_addr(fd1, f1, pz1, paddr1);
	setup_uncompressed_addr(fd2, f2, pz2, paddr2);
	cerr<<paddr1<<paddr2<<endl;
	
	int matching_segs=0;
	for (int i=0; i<f1.n_segs; i++) {
		if (compare_segment(f1, f2, paddr1, paddr2, i)) {
			matching_segs++;
		}
	}
	cout<<"There were "<< matching_segs<<" out of "<<f1.n_segs<<endl;

return 0;
}
