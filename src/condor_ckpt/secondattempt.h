#ifndef _secondattempt_h_
#define _secondattempt_h_ 

typedef unsigned long RAW_ADDR;
bool DEBUG = false;
#define SUPER_VERBOSE false

//an abstraction of a segment within a ckpt file
struct SegInfo {
	char name[14];
	off_t		file_loc;
	RAW_ADDR	core_loc;
	long		len;
	int			prot;	
	void Display();
	
};

//an abstraction of a checkpoint file
struct CheckpointFile {
	static const int COMPRESS_MAGIC=0xfeafeb;
	static const int MAGIC = 0xfeafea;
	bool compressed;
	bool use_ntoh;
	bool ints_are_longs;
	int n_segs;
	SegInfo segmap[256];
};

int compare_pages(char *p1, char*p2, int &decile);
int read_header(int fd, CheckpointFile &ck);
int read_segmap(int fd, CheckpointFile &ck);
z_stream * initialize_zstream(z_stream *pz);
int ntoh (int network_int, CheckpointFile &ck);

//const int HEADER_LENGTH=1024-2*sizeof(int)-sizeof(RAW_ADDR); //doesn't work?
const int HEADER_LENGTH=1024-64; //this works. not sure why --
								//probably annoying alignment issues

#endif
