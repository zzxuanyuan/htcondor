#include "condor_common.h"
#include "Set.h"
#include "stork-mm.h"
#include "dc_match_lite_lease.h"

list<DCMatchLiteLease*>mylist;
int main ( void )
{
	DCMatchLiteLease	l;
	mylist.push_back( &l );
	mylist.remove( &l);

	StorkMatchMaker	mm;
	printf( "mm size %d @ %p\n", sizeof(mm), &mm );

	
}
