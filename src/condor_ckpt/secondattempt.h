#ifndef _secondattempt_h_
#define _secondattempt_h_ 

typedef unsigned long RAW_ADDR;
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
	bool compressed;
	int n_segs;
	SegInfo segmap[256];
};

int compare_pages(char *p1, char*p2, int &decile);
int read_header(int fd, CheckpointFile &ck);
int read_segmap(int fd, CheckpointFile &ck);
z_stream * initialize_zstream(z_stream *pz);

const int HEADER_LENGTH=1024-64; //this works. not sure why--probably annoying alignment issues

//const int HEADER_LENGTH=1024-2*sizeof(int)-sizeof(RAW_ADDR); //length of the header.
#endif
