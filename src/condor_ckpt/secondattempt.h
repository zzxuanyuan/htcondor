#ifndef _secondattempt_h_
#define _secondattempt_h_ 

typedef unsigned long RAW_ADDR;
bool DEBUG = true;
#define SUPER_VERBOSE false


//this class is never used. it just tells us how big the structure is.
struct SegInfo2 {
	char name[14];
	off_t		file_loc; //beware--this will be in whatever order the host wrote it in/
	RAW_ADDR	core_loc; //this will be too.
	long		len;  //this will be as well..
	int			prot;	//as will this...
	void Display();
};



//an abstraction of a segment within a ckpt file
struct SegInfo {
	char name[14];
	off_t		file_loc; //beware--this will be in whatever order the host wrote it in/
	RAW_ADDR	core_loc; //this will be too.
	long		len;  //this will be as well..
	int			prot;	//as will this...
	void Display();
	bool matched;
};

//an abstraction of a checkpoint file
struct CheckpointFile {
	static const int COMPRESS_MAGIC=0xfeafeb;
	static const int MAGIC = 0xfeafea;
	bool compressed;
	bool needs_byte_swap;
	bool ints_are_longs;
	int n_segs;
	SegInfo segmap[256];
};
int initialize_checkpoint_architecture(char * arch);
int compare_pages(char *p1, char*p2, int &decile);
int read_header(int fd, CheckpointFile &ck);
int read_segmap(int fd, CheckpointFile &ck);
z_stream * initialize_zstream(z_stream *pz);
int ntoh (int network_int, CheckpointFile &ck);
int swap_byte_order(unsigned int);
int findMatchingSegment(CheckpointFile &f1, CheckpointFile &f2, int ck1_segNo); 
const int HEADER_LENGTH=1024-64; //this works. not sure why --
								//probably annoying alignment issues

class ArchSpecific {
 public:
	static const char  * const  SPARC="SPARC" ;
	static const char * const INTEL="INTEL";
	virtual int getpagesize()=0;
};
class Sun4Specific: public ArchSpecific {
 public:
	virtual int getpagesize() {return 8192;}
};
class Suni386Specific: public ArchSpecific {
 public:
	virtual int getpagesize() {return 4096;}
};

	

#endif
