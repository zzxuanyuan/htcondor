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
#include <sys/utsname.h>
//#include <sys/systeminfo.h>
#include <zlib.h> 
#include "secondattempt.h"

int fd1, fd2; //file descriptors for first and second file, respectively.
CheckpointFile f1, f2; //checkpoint files for each ones.
z_stream *pz1, *pz2; //checkpoint compression crap.
char * paddr1, *paddr2; //locations of *UNCOMPRESSED* mmaped files.
ArchSpecific *pArch=NULL; //pointer to the arch that you are using.



int test_and_set_architecture(int magic) {
	if (pArch) return 0;
	bool cf_is_my_endian= (magic==CheckpointFile::MAGIC||
						   magic==CheckpointFile::COMPRESS_MAGIC);
	if (!cf_is_my_endian && 
		(swap_byte_order(magic)!=CheckpointFile::MAGIC) &&
		(swap_byte_order(magic)!=CheckpointFile::COMPRESS_MAGIC))
		{
			cout<<"ERROR: bad magic number"<<endl;
			exit (-1);
		}
	const int BUFFER_SIZE=25;
	char *buffer=NULL;
//	char buffer[BUFFER_SIZE];
//	sysinfo(SI_ARCHITECTURE , buffer, BUFFER_SIZE);
	struct utsname myInfo;
	if (uname(&myInfo)<0) {
		cout<<"ERROR: utsname didn't work"<<endl;
		exit(2);
	}
	buffer=myInfo.machine;
	
	
	//hack because linux has an i686.
	if (!strncmp(buffer, "i", 1))
		{
		if (DEBUG){
			cout<<"DEBUG: Machine is an x86"<<endl;
		}
		if (cf_is_my_endian) {
			pArch=new Suni386Specific();
		}
		else {
		   pArch=new Sun4Specific();
		}
	}
	else if (!strncmp(buffer, "sun", 3)) {
		if (DEBUG) {
			cout<<"DEBUG: Machine is a Sparc"<<endl;
		}
		if (cf_is_my_endian) {
			pArch=new Sun4Specific();
		}
		else {
			pArch=new Suni386Specific();
		}
	}
	else {
		cout<<"ERROR: Unknown machine type "<< buffer<<endl;
		exit(3);
	}
	return 0;
}
// what this does is renumber things
//so if we have a four byte thing
// [4 3 2 1], it becomes something like [1 2 3 4]
// watc out for negative numbers (have to think about that)
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
	if (decile == 10) cout << "*";
	else cout << decile;
	return decile==10;
}
int compare_segment(CheckpointFile &ck1, CheckpointFile &ck2, char * paddr1, 
					char * paddr2, int ck1_segnumber, int ck2_segnumber, 
					int &matches, int &pages) 
{
	int pgsize=pArch->getpagesize();
	//we are going to read this a page at a time:
	int bytes_to_read1=ck1.segmap[ck1_segnumber].len;
	int bytes_to_read2=ck2.segmap[ck2_segnumber].len;
	int pages_matching=0;
	int total_pages=0;
	float matchPercent;
	float matchAverage = 0;
	int decileval=0;
	char * p1, *p2;
	p1=paddr1+ck1.segmap[ck1_segnumber].file_loc;
	p2=paddr2+ck2.segmap[ck2_segnumber].file_loc;
	int histogram[11];
	memset(histogram, 0, 11*sizeof(int));
	cout << "Comparing segment " << ck2.segmap[ck2_segnumber].name << ": \n"; 
	cout << "Per page analysis: ";

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
		cout << "+";	// for page analysis, + means new
		histogram[0]++;
		total_pages++;
		new_bytes-=pgsize;
	}

	matchAverage /= (total_pages - pages_matching);
	cout << endl;
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
		exit (-1);
	}
	// check for network to host stuff
	test_and_set_architecture(vals[0]);

	if (cf.needs_byte_swap = ! (vals[0]==cf.COMPRESS_MAGIC||vals[0]==cf.MAGIC)){
		if (DEBUG) cout << "DEBUG: Need to swap bytes for different arch\n"; 
		vals[0] = swap_byte_order(vals[0]);
	}
			
	if (DEBUG) 
		cout<<"MAGIC: "<<cf.MAGIC<<"\tCOMPRESS_MAGIC "<<cf.COMPRESS_MAGIC
			<< ", cf.magic is " << vals[0] <<endl;
	if (vals[0] != cf.MAGIC && vals[0] != cf.COMPRESS_MAGIC) {
		cout << "ERROR: CANNOT CONVERT FROM NETWORK ORDER" << endl;;
		exit (0);
	}

	// now make sure values are OK after converting	
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
	//this is predicated on being able to read the seginfos properly--
	// DO NOT CHANGE THE STRUCTURE
	// as it messes up alignment.
	SegInfo2 * segs=new SegInfo2[cf.n_segs];
	retval=read(fd, segs, cf.n_segs*sizeof(SegInfo2));
	for (int i=0; i<cf.n_segs; i++) {
		strcpy(cf.segmap[i].name, segs[i].name);
		cf.segmap[i].file_loc= segs[i].file_loc;
		cf.segmap[i].core_loc=segs[i].core_loc;
		cf.segmap[i].len=segs[i].len;
		cf.segmap[i].len=segs[i].len;

		if (cf.needs_byte_swap) {
			#define CURRSEG  cf.segmap[i]
			#define SBO(x) swap_byte_order(x)
			CURRSEG.file_loc=SBO(CURRSEG.file_loc);
			CURRSEG.core_loc=SBO(CURRSEG.core_loc);
			CURRSEG.len=SBO(CURRSEG.len);
			#undef CURRSEG
			#undef SBO(x)
		}
	}
	if (segs) delete [] segs;

	return 0;
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
	if (DEBUG) { cout<<"PZ= "<<pz<<endl; }
	pz->zalloc=Z_NULL;
	pz->zfree=Z_NULL;
	pz->opaque=Z_NULL;
	
	int xxx=inflateInit(pz);
	if (DEBUG) { cout<<"InflateInit: "<<xxx<<endl; }
	return pz;	
}	

/**
* this function searches the second ckpt file for a segment which appears
* to correspond to the specified segment in the first ckpt file
*/



//greg's approach: 
//if this is a data segment, find a data segment.
//note: this is one of the only reasonable uses of the goto statement...
int findMatchingSegment(CheckpointFile &f1, CheckpointFile &f2, int ck1_segNo) {
	char * type = f1.segmap[ck1_segNo].name;
	SegInfo *sp=&f1.segmap[ck1_segNo];

	int retval;

	//if it is a data segment, find a data segment
	if (!strcmp(type, "DATA")){
		for (int i=0; i<f2.n_segs; i++ ){
			if (!strcmp("DATA", f2.segmap[i].name)) {
				//found it--return it
				retval=i;
				goto done;
			}
		}
	}
	// ok. we've failed to find it.  maybe it is stack segment
	if (!strcmp(type,"STACK")){
		//look from the back, since that's where stack segments live
		for (int i=f2.n_segs-1; i>=0; i--) {
			if (!strcmp("STACK", f2.segmap[i].name)){
				retval=i;	
				goto done;

			}
		}
	}
	//ok. then it has to be something else.
	for (int i=0; i<f2.n_segs; i++ ){
		SegInfo *sp2=&f2.segmap[i];
		if (sp2->matched) { // already been matched
			continue;
		}
		//only bother if they are the same name
		if (!strcmp(sp->name, sp2->name)) {
			if (sp->core_loc==sp2->core_loc) {
				//then we have it.
				retval=i;
				goto done;
			}
			// ok if the start addresses didn't line up, 
			// perhaps their end addresses do?
			if (sp->core_loc+sp->len==sp2->core_loc+sp2->len) {
				retval=i;
				goto done;
			}
		}
	}

	if (DEBUG) {
		cout << "WARNING: Unable to match segment of type " << type << endl;
	}
	retval=-1;
	goto end;
 
 done:
	if (DEBUG) cout<<"Matched segment "<<ck1_segNo<<" to segment "<<retval;
	f1.segmap[ck1_segNo].matched = f2.segmap[retval].matched = true;
 end:
	return retval;
}

int setup_uncompressed_addr (int fd, CheckpointFile &ck, z_stream *pz, 
							char * &paddr) 
{
	unsigned long needed_len=
		ck.segmap[ck.n_segs-1].file_loc+ck.segmap[ck.n_segs-1].len;
	//amount of space that we need for this file.

	int first_segment_address=ck.segmap[0].file_loc;

	if (DEBUG){
		cout<<"DEBUG: first segment address"<<first_segment_address<<endl;
		cout<<"DEBUG: Needed Length "<<needed_len<<endl;
	}
	if (ck.compressed) {
		char * paddrtemp;
		//hack--determine the length of this file
		long cur_loc=lseek(fd, 0, SEEK_CUR);
		long end_loc=lseek(fd, 0, SEEK_END);
		if (DEBUG) cout<<"DEBUG: end_location is "<<end_loc<<endl;
		lseek(fd, cur_loc, SEEK_SET);//reset the file pointer.
		//we should have a pointer to the beginning of a compressed checkpoint
		paddrtemp=(char *)mmap(0, end_loc, PROT_READ, MAP_PRIVATE, fd, 0);
		if (paddrtemp==MAP_FAILED) {
			cout<<"ERROR: MAP FaILED"<<endl;
			exit (-1);
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
			if (DEBUG){
				cout<<"DEBUG: address of final area "<< (void *)&(paddr[0]) <<" ends  "
					<< (void *)&(paddr[needed_len-1]) <<endl;}
		}
		//now lets start decompressing...
		//I guess we'll just copy everything before the segments themselves...
		int xqqq=(int)memcpy(paddr, paddrtemp, first_segment_address);
		if (DEBUG) { cout<<"DEBUG: value of paddr"<<(void *)xqqq<<endl;}
		pz->total_out=0;
		pz->total_in=0;
		pz->next_in= (Bytef*) (unsigned long) (paddrtemp+first_segment_address);
		pz->avail_in=end_loc-first_segment_address;
		pz->next_out=(Bytef*) (unsigned long)(paddr+first_segment_address);
		pz->avail_out=needed_len-first_segment_address;
		//pz->avail_out=getpagesize();
		//now decompress;
		int xxx=0;
		if (DEBUG) { 
			cout<< "DEBUG: "<< "current value "<<xxx
				<< " total_in= "<<pz->total_in<< " total_out="<<pz->total_out<<
				" avail_in="<<pz->avail_in<<" avail_out="<<pz->avail_out<<endl <<
				"next out "<< (void *)pz->next_out<<endl;
		}	
		xxx=inflate(pz, Z_FINISH);
		if (DEBUG) { 
			cout<< "DEBUG: "<< "current value "<<xxx
				<< " total_in= "<<pz->total_in<< " total_out="<<pz->total_out<<
				" avail_in="<<pz->avail_in<<" avail_out="<<pz->avail_out<<	"next out "<< (void *)pz->next_out<<endl;
		}	
		//now that we are done, we should see how much extra space there is.


		
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
	if (arch==NULL||!strcmp(arch, ArchSpecific::INTEL)) {
		if (DEBUG) {
			if ( arch == NULL ) 
			cout<<"DEBUG: No architecture specificed--defaulting to INTEL\n";
			else
			cout << "DEBUG: INTEL specified\n";
		}
		pArch=new Suni386Specific();
	}
	else if (!strcmp(arch, ArchSpecific::SPARC)) {
		if (DEBUG) cout << "DEBUG: SPARC specified\n";
		pArch=new Sun4Specific();
	}
	else {
		cout<<"ERROR: Unknown Architecture "<<arch<<endl;
		exit(2);
	}
	return retval;
}
void
usage(char * me) {
	cout << "Usage: " << me << " ckpt_1 ckpt_2  -a arch(SPARC|INTEL) -d\n";
}

int 
add_up_unmatched_segs (CheckpointFile& ckpt, int& total_pages) { 
	int unmatched = 0, pages = 0, tmp;

	for (int i = 0; i < ckpt.n_segs; i++) {
		if ( ! ckpt.segmap[i].matched) {
			cout << "Unmatched segment " << ckpt.segmap[i].name << ": \n"; 
			unmatched++;
			pages = tmp = ckpt.segmap[i].len / pArch->getpagesize(); 
			total_pages += pages;
			cout << "Per page analysis: ";
			while (tmp--) {
				cout <<"+";
			}
			cout<<"\n\tPages matching: 0 / "<<pages<< "   (ave. nonmatch: 0%)\n";
			cout<<"\tHistogram: " << pages << " 0 0 0 0 0 0 0 0 0\n";
		}
	}
	return unmatched;
}

//first thing is file
int main(int argc, char ** argv ) {

	int curArg = 1;
	fd1=open(argv[curArg++], O_RDONLY);
	fd2=open(argv[curArg++], O_RDONLY);
	//bool arch_set = false;
	DEBUG = false;
	while (curArg < argc) {
		if (!strcmp(argv[curArg], "-d")) {
			DEBUG = true;
		} else if ( !strcmp(argv[curArg], "-a")) {
			//initialize_checkpoint_architecture(argv[++curArg]);
			//arch_set = true;
		} else {
			usage(argv[0]);
			exit (-1);
		}
		curArg++;
	}
	
	//ok. lets try this:
	//we now initialize the checkpoint architecture in the read_hdeader
	read_header(fd1, f1);
	read_header(fd2, f2);
	if (DEBUG) cout<<"DEBUG: Read Headers"<<endl;
	
	read_segmap(fd1, f1);
	read_segmap(fd2, f2);
	
	if (DEBUG) {
		cout << "DEBUG: Display segmap 1:" << endl;
		for (int i=0; i<f1.n_segs; i++) 
			{f1.segmap[i].Display();}
		cout << "DEBUG: Display segmap 2:" << endl;
		for (int i=0; i<f2.n_segs; i++) 
			{f2.segmap[i].Display();}
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
	if (DEBUG) cout<<"DEBUG: " << "paddr1 is " << (void *)paddr1
					<< ", paddr2 is " <<(void *)paddr2 <<endl;
	
	int matching_segs=0;
	int total_matches = 0;
	int total_pages = 0;
	int fewer_segs = (f1.n_segs < f2.n_segs) ? f1.n_segs : f2.n_segs;
	int more_segs  = (f1.n_segs < f2.n_segs) ? f2.n_segs : f1.n_segs;

	int ck2_segNo;
	for (int ck1_segNo = 0;  ck1_segNo < fewer_segs; ck1_segNo++) {
		{
		if (-1 == (ck2_segNo = findMatchingSegment(f1, f2, ck1_segNo)))
			continue;	// no matching segment
		}
		if (compare_segment(f1, f2, paddr1, paddr2, ck1_segNo, ck2_segNo,
				total_matches,total_pages)) 
		{
			matching_segs++;
		}
	}
	// now add up all the unmatched pages from the unmatched segments
	int unmatched_segs = 0;
	int unmatched_pages = 0;
	unmatched_segs += add_up_unmatched_segs (f1, unmatched_pages); 
	unmatched_segs += add_up_unmatched_segs (f2, unmatched_pages); 
	total_pages += unmatched_pages;
	cout<<"Total segment matches:  "<< matching_segs<<" / "<< more_segs <<endl;
	if (unmatched_segs != more_segs - fewer_segs) {
		cout << "WARNING: "<< (unmatched_segs - (more_segs - fewer_segs)) << " segments were not matched,"
				<< " resulting in " << unmatched_pages << " unmatched pages.\n";
	}

	float matchPercent = (float) total_matches / total_pages * 100;
	cout << "Total page matches: " << total_matches << " / " << total_pages 
		<< "  (" << (int) matchPercent << "%)" << endl;

return 0;
}
