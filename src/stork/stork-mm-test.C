#include "condor_common.h"
#include "Set.h"
#include "stork-mm.h"
#include "dc_match_maker_lease.h"
#include "condor_config.h"

list<DCMatchMakerLease*>mylist;
int main ( int argc, char **argv )
{
	DCMatchMakerLease	l;
	mylist.push_back( &l );
	mylist.remove( &l);

	config();
	Termlog = 1;
	dprintf_config("TOOL",2);

	StorkMatchMaker	mm;
	printf( "mm size %d @ %p\n", sizeof(mm), &mm );
	const char *result = NULL;
	result = mm.getTransferDirectory(NULL);
	printf("TODD dest = %s\n", result ? result : "(NULL)" );
	result = mm.getTransferDirectory(NULL);
	printf("TODD dest = %s\n", result ? result : "(NULL)" );

	
}
