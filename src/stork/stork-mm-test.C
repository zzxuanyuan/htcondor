#include "condor_common.h"
#include "Set.h"
#include "stork-mm.h"
#include "dc_match_lite_lease.h"

class Nick
{
	public:
	Nick( void ) { strcpy( stupid, "nick" ); };
	~Nick( void ) { };
	int operator ==( const Nick & ) { return 1; };

private:
	char	stupid[1024];
};

#if 1
class Nick2
{
public:
	Nick2( void ) { };
	~Nick2( void ) { } ;
	int operator ==( const Nick2 & ) { return 1; };

private:
	Set<Nick> nickset;
};
#endif

list<DCMatchLiteLease*>mylist;
int main ( void )
{
	DCMatchLiteLease	l;
	mylist.push_back( &l );
	mylist.remove( &l);

	StorkMatchMaker	mm;
	printf( "mm size %d @ %p\n", sizeof(mm), &mm );

#if 0
	Nick	nick;
	printf( "nick size %d @ %p\n", sizeof(nick), &nick );

	Nick2	nick2;
	printf( "nick2 size %d @ %p\n", sizeof(nick2), &nick2 );


	StorkMatchEntry	sme;
	printf( "sme size %d @ %p\n", sizeof(sme), &sme );


	char	buf3[64];
	StorkMatchMaker	*pmm;
	char	*pch = new char(1024);
	char	buf4[64];
	pmm = new StorkMatchMaker( );
	printf( "pmm is at %p, sizes %d %d\n", pmm, sizeof(pmm), sizeof(*pmm) );

	Nick	*pnick = new Nick;
	printf( "pnick is at %p, sizes %d %d\n", pnick, sizeof(pnick), sizeof(*pnick) );

	delete pmm;
	delete pch;
#endif
	
}
