#include <iostream.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
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


// what this does is renumber things
//so if we have a four byte thing
// [4 3 2 1], it becomes something like [1 2 3 4]
// watch out for negative numbers (have to think about that)
int swap_byte_order(unsigned int  number) {
	unsigned int fb, sb, tb, ob;
	fb=number&0xFF;
	sb=number>>8;
	sb&=0xff;
	tb=number>>16;
	tb&=0xff;
	ob=number>>24;
	ob&=0xff;
	return
		(signed int) (fb<<24|
					  sb<<16|
					  tb<<8|
					  ob);
}
	


int fd1, fd2; //file descriptors for first and second file, respectively.
CheckpointFile f1, f2; //checkpoint files for each ones.
z_stream *pz1, *pz2; //checkpoint compression crap.
char * paddr1, *paddr2; //locations of *UNCOMPRESSED* mmaped files.
ArchSpecific *pArch=NULL; //pointer to the arch that you are using.


//assumes that pages are decompressed
int compare_pages(char * p1, char *p2, int &decile, float &matchPercent) {
	int bytes_matching=0;
	for (int i=0; i<pArch->getpagesize(); i++) {
		if (*p1++==*p2++) {
			bytes_matching++;
		}
	}
	
	matchPercent = bytes_matching / (float) pArch->getpagesize();
	decile= (int)(10.0*matchPercent);
	if (SUPER_VERBOSE) {
		if (decile==10 ){
			cout<<"\tSUPER_VERBOSE: PAGE MATCH"<<endl;
		}
		else {
			cout<<"\tSUPER_VERBOSE: PAGE NOMATCH "<<
				bytes_matching<<"/"<<pArch->getpagesize()<<" "<< matchPercent << endl;
		}
	} else {
		if (decile == 10) cout << "*";
		else cout << decile;
	}	
	return decile==10;
}
int compare_segment(CheckpointFile &ck1, CheckpointFile &ck2, char * paddr1, 
					char * paddr2, int segnumber, int &matches, int &pages) {
	int pgsize=pArch->getpagesize();
	//we are going to read this a page at a time:
	int bytes_to_read1=ck1.segmap[segnumber].len;
	int bytes_to_read2=ck2.segmap[segnumber].len;
	int pages_matching=0;
	int total_pages=0;
	float matchPercent;
	float matchAverage = 0;
	int decileval=0;
	char * p1, *p2;
	p1=paddr1+ck1.segmap[segnumber].file_loc;
	p2=paddr2+ck2.segmap[segnumber].file_loc;
	int histogram[11];
	memset(histogram, 0, 11*sizeof(int));
	if (strcmp(ck1.segmap[segnumber].name, ck2.segmap[segnumber].name) != 0) {
		cout << "WARNING:  Segments do not correspond, " 
			<< ck1.segmap[segnumber].name << " and " 
			<< ck2.segmap[segnumber].name << endl;
	}
	cout << "Comparing segment " << ck2.segmap[segnumber].name << ": " << endl;
	if ( ! SUPER_VERBOSE ) cout << "Per page analysis: ";

	int comparable_bytes;
	int new_bytes;
	if (bytes_to_read1 < bytes_to_read2) {
		comparable_bytes = bytes_to_read1;
		new_bytes = bytes_to_read2 - bytes_to_read1;
	} else {
		comparable_bytes = bytes_to_read2;
		new_bytes = bytes_to_read1 - bytes_to_read2;
	}

	while (comparable_bytes>0) {
		if (compare_pages(p1, p2, decileval, matchPercent)==1) {
			pages_matching++;
		} else {
			matchAverage += matchPercent;
		}	
		total_pages++;
		comparable_bytes-=pgsize;
		p1+=pgsize;
		p2+=pgsize;
		histogram[decileval]++;
	}
	while (new_bytes>0) {	// different size segments, pages are considered
							// match of 0 percent, mark them + in the analysis
		if ( SUPER_VERBOSE ) {
			cout<<"\tSUPER_VERBOSE: PAGE NOMATCH 0/" << pgsize  
				<< " 0 (New page)" << endl;
		} else {
			cout << "+";	// for page analysis, + means new
		}
		histogram[0]++;
		total_pages++;
		new_bytes-=pgsize;
	}

	matchAverage /= (total_pages - pages_matching);
	if ( ! SUPER_VERBOSE) cout << endl;
	cout<<"\tPages matching: "<<pages_matching <<" / "<<total_pages
		<< "   (ave. nonmatch: ";
	if (total_pages == pages_matching) {
		cout << "n/a";
	} else {
		cout << (int)(100*matchAverage) << "%";
	}
	cout << ") "<< endl;
	cout<<"\tHistogram: ";
	for (int i=0; i<11; i++) cout<<histogram[i]<<"  ";
	cout<<endl;
	for (int i = 0, start, end; i < 11; i++) {
		start = i * 10;
		end = (start == 100) ? 100 : start + 9;
		cout <<"\tVERBOSE: " << start << " to " << end <<" % match: "
			<< histogram[i] << endl;
	}
	
	// set return values
	matches += pages_matching;
	pages   += total_pages;
	return pages_matching==total_pages;
}			


//side effect--seeks to the beginning of the file
int read_header(int fd, CheckpointFile &cf) {
	int retval=lseek(fd, 0, SEEK_SET);
	int vals[2];
	//read two bytes
	retval=read(fd, vals, 2*sizeof(int));
	if (retval!=2*sizeof(int)) {
		cout<<"ERROR READING HEADER"<<endl;
	}
	// check for network to host stuff
	cout<<"MAGIC: "<<cf.MAGIC<<"\tCOMPRESS_MAGIC"<<cf.COMPRESS_MAGIC<<endl;
	cout<<"Value of first int "<< vals[0]<< " Swapped: "<<swap_byte_order(vals[0])<<endl;
	//unsigned int foo= (unsigned int) vals[0];
	//cout<<swap_byte_order(foo)<<endl;
	//cout<<"Value of foo"<<foo<<endl;
	
	if (vals[0]==cf.COMPRESS_MAGIC|| vals[0]==cf.MAGIC) {
		cf.needs_byte_swap=false;
	}
	else if (swap_byte_order(vals[0])==cf.COMPRESS_MAGIC||
			 swap_byte_order(vals[0])==cf.MAGIC) 
		{
			if (DEBUG) cout << "DEBUG: Need to convert from network order" << endl;
			cf.needs_byte_swap=true;
	}
	else {
		cout << "ERROR: CANNOT CONVERT FROM NETWORK ORDER" << endl;;
			exit (0);
	}
	// now make sure values are OK after converting	
	vals[0] = (cf.needs_byte_swap? swap_byte_order(vals[0]): vals[0]);
	vals[1]= (cf.needs_byte_swap? swap_byte_order(vals[1]): vals[1]);
	if (vals[0] == cf.COMPRESS_MAGIC) {
		cf.compressed = true;
	} else if (vals[0] == cf.MAGIC) {
		cf.compressed = false;
	} else {
		cout << "ERROR: MAGIC NUMBER UNKNOWN: " << vals[0] << endl;
		exit (0);
	}
	cf.n_segs=vals[1];
	if (DEBUG) 
		{cout<<"DEBUG: cf, compressed: "<<cf.compressed<<", num segs:  "
					<<cf.n_segs<<endl;
		}
	return 0;
}

//assumes that the checkpoint is created!
int read_segmap(int fd, CheckpointFile &cf) {
	if (DEBUG) cout<<"DEBUG: fd="<<fd << endl;
	int retval=lseek(fd,  HEADER_LENGTH, SEEK_SET);
	if (retval<0 ){
		cout<<"ERROR!!!"<<endl;
		return -1;
	}
	//this is tricky, but should work:
	retval=read(fd, cf.segmap, cf.n_segs*sizeof(SegInfo));
	if (DEBUG) cout<<"DEBUG: Read "<<retval<<" bytes (the segmaps)"<<endl;
	if (cf.needs_byte_swap) {
		for (int i = 0; i < cf.n_segs; i++) {
			cf.segmap[i].len = swap_byte_order (cf.segmap[i].len);//sizeof long needs sizeof int...
			cf.segmap[i].prot = swap_byte_order (cf.segmap[i].prot);
			cf.segmap[i].file_loc=swap_byte_order(cf.segmap[i].file_loc);
			cf.segmap[i].core_loc=swap_byte_order(cf.segmap[i].core_loc);
		}
	}
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
	cout<<"PZ= "<<pz<<endl;
	//pz->zalloc=zalloc;
	//pz->zfree=zfree;
	pz->zalloc=Z_NULL;
	pz->zfree=Z_NULL;
	pz->opaque=Z_NULL;
	
	int xxx=inflateInit(pz);
	cout<<"InflateInit: "<<xxx<<endl;
	return pz;	
}	


int setup_uncompressed_addr (int fd, CheckpointFile &ck, z_stream *pz, 
							char * &paddr) 
{
	unsigned long needed_len=
		ck.segmap[ck.n_segs-1].file_loc+ck.segmap[ck.n_segs-1].len;
	//amount of space that we need for this file.
	
	int first_segment_address=ck.segmap[0].file_loc;

	if (DEBUG) cout<<"DEBUG: Needed Length "<<needed_len<<endl;
	if (ck.compressed) {
		char * paddrtemp;
		//hack--determine the length of this file
		long cur_loc=lseek(fd, 0, SEEK_CUR);
		long end_loc=lseek(fd, 0, SEEK_END);
		if (DEBUG) cout<<"DEBUG: end_location is "<<end_loc<<endl;
		lseek(fd, cur_loc, SEEK_SET);
		//we should have a pointer to the beginning of a compressed checkpoint
		paddrtemp=(char *)mmap(0, end_loc, PROT_READ, MAP_PRIVATE, fd, 0);
		if (paddrtemp==MAP_FAILED) {
			cout<<"ERROR: MAP FaILED"<<endl;
		}
		else {
			if (DEBUG) cout<<"DEBUG: Address of temporary map: "
						<<(unsigned long)paddrtemp<<endl;
		}

		//ok. I wonder if this will work...
		paddr=(char *) malloc(needed_len);
		if (paddr==NULL) {
			cout<<"ERROR: malloc of "<<needed_len<<" bytes failed!"<<endl;
			return -1;
		}
		else {
			if (DEBUG) cout<<" DEBUG: address of final area "
							<< (unsigned long) paddr <<endl;
		}
		//now lets start decompressing...
		//I guess we'll just copy everything before the segments themselves...
		int xqqq=memcpy(paddr, paddrtemp, first_segment_address);
		if (DEBUG) { cout<<"xqqq"<<xqqq<<endl;}
		pz->total_out=0;
		pz->total_in=0;
		pz->next_in= (Bytef*) (unsigned long) (paddrtemp+first_segment_address);
		pz->avail_in=end_loc-first_segment_address;
		pz->next_out=(Bytef*) (unsigned long)(paddr+first_segment_address);
		pz->avail_out=needed_len-first_segment_address;
		//pz->avail_out=getpagesize();
		//now decompress;
		int xxx=0;
		if (DEBUG) cout << "DEBUG: " <<xxx<<" Def "<<pz->total_out<<" read "
						<<pz->total_in <<" avail in "<<pz->avail_in<<endl 
						<<"xxx "<<xxx<<endl;
		xxx=inflate(pz, Z_FINISH);
		if (DEBUG) cout << "DEBUG: " <<xxx<<" Def "<<pz->total_out<<" read "
						<<pz->total_in <<" avail in "<<pz->avail_in<<endl 
						<<"xxx "<<xxx<<endl;
		//now we should be done, unmap the file
		if (munmap(paddrtemp, end_loc)!=0 ){
			cout<<"ERROR: UNMAP FAILED"<<endl;
		}
	}
	else { //non compressed file...
		paddr=(char *)mmap(0, needed_len, PROT_READ, MAP_PRIVATE, fd, 0);
		if (paddr==MAP_FAILED ){
			perror("ERROR: MAP FAILED: ");
		}
	}
	return 0;
}

//corny function to initialize an archspecific
int initialize_checkpoint_architecture(char * arch) {
	int retval=0;
	if (arch==NULL||!strcmp(arch, ArchSpecific::SPARC)) {
		cout<<"No architecture specificed--defaulting to SUN"<<endl;
		pArch=new Sun4Specific();
	}
	else if (!strcmp(arch, ArchSpecific::INTEL)) {
		pArch=new Suni386Specific();
	}
	else {
		cout<<"Unknown Architecture "<<arch<<endl;
		exit(2);
	}
	return retval;
}



//first thing is file
int main(int argc, char ** argv ) {
	//test swap bute order
	//int xa, xb, xc, xd;
//	xa=0x000000AA;
//	xb=0x0000BB00;
	//xc=0x00CC0000;
	//xd=0xDD000000;
	//int ax, bx, cx, dx;
	//ax=0xAA000000;
	//bx=0xBB0000;
	//cx=0xCC00;
	//dx=0xDD;
	//#define SPACE " "
	//cout<< "INPUT:" <<xa<<SPACE<<xb<<SPACE<<xc<<SPACE<<xd<<endl;
	//cout<<"CORRECT: "<<ax<<SPACE<<bx<<SPACE<<cx<<SPACE<<dx<<endl;
	//cout<<"Actual: "<<swap_byte_order(xa)<<SPACE<<
	//swap_byte_order(xb)<<SPACE<<
	//swap_byte_order(xc)<<SPACE<<
	//swap_byte_order(xd)<<SPACE<<endl;	
	cout<<sizeof(off_t)<<endl;
	if (argc != 4 && argc != 5) {
		cout << "Usage: " << argv[0] << " ckpt_1 ckpt_2 arch(SPARC|INTEL)  [debug]" << endl;
		exit(1);
	}	
	fd1=open(argv[1], O_RDONLY);
	fd2=open(argv[2], O_RDONLY);
	DEBUG = (argc == 5);  // a fourth arg will turn on debugging
	initialize_checkpoint_architecture(argv[3]);
	
	//ok. lets try this:
	read_header(fd1, f1);
	read_header(fd2, f2);
	cout<<"Read Headers"<<endl;
	
	read_segmap(fd1, f1);
	read_segmap(fd2, f2);
	
	if (DEBUG) {
		cout << "DEBUG: Display segmap 1:" << endl;
		for (int i=0; i<f1.n_segs; i++) f1.segmap[i].Display();
	}	

	pz1=initialize_zstream(pz1);
	pz2=initialize_zstream(pz2);

	if (DEBUG) cout<<"DEBUG: pz1: " << pz1 << endl;
	paddr1=NULL;
	paddr2=NULL;
	setup_uncompressed_addr(fd1, f1, pz1, paddr1);
	setup_uncompressed_addr(fd2, f2, pz2, paddr2);
	if (paddr1 == NULL || paddr2 == NULL) {
		cout <<"ERROR: could not create paddr. (Network order problem?)"<<endl;
		exit (0);
	}
	if (DEBUG) cout<<"DEBUG: " << "paddr1 is " << paddr1
					<< ", paddr2 is " <<paddr2 <<endl;
	
	int matching_segs=0;
	int total_matches = 0;
	int total_pages = 0;
	int fewer_segs = (f1.n_segs < f2.n_segs) ? f1.n_segs : f2.n_segs;
	for (int i=0; i<fewer_segs; i++) {
		if (compare_segment(f1,f2,paddr1,paddr2,i,total_matches,total_pages)) 
		{
			matching_segs++;
		}
	}
	cout<<"Total segment matches:  "<< matching_segs<<" / "<< fewer_segs <<endl;
	if (f1.n_segs != f2.n_segs) {
		cout << "WARNING: An unequal number of segments in the ckpt files:"
			<< f1.n_segs << " and " << f2.n_segs << endl;
	}  
	float matchPercent = (float) total_matches / total_pages * 100;
	cout << "Total page matches: " << total_matches << " / " << total_pages 
		<< "  (" << (int) matchPercent << "%)" << endl;

return 0;
}
