
#include "StorkJobId.h"
#include "stork_job_ad.h"
#include <stdio.h>
#include <assert.h>
using std::string;


void sub(void)
{
	return;
}

int main(int argc, char* argv[] )
{
	StorkJobId jobId;
	classad::PrettyPrint unparser;
    string adbuffer;

	printf("%s\n", jobId.fmt() );
	++jobId;
	printf("%s\n", jobId.fmt() );

	classad::ClassAd* ad = new classad::ClassAd;
	assert(ad);
	if (! ad->InsertAttr("type", "stringValue") ) {
		fprintf(stderr, "%d InsertAttr %s error: %s\n",
				__LINE__, "type", classad::CondorErrMsg.c_str() );
	}

#if 0
	unparser.Unparse(adbuffer, ad);
	printf("%s\n", adbuffer.c_str() );
	classad::ClassAdParser parser;
	classad::ClassAd *requestAd = NULL;
	requestAd = parser.ParseClassAd( adbuffer.c_str() );
	if (! requestAd ) {
		fprintf(stderr, "%d ParseClassAd(%s) error: %s\n",
				__LINE__, adbuffer.c_str(), classad::CondorErrMsg.c_str() );
	}
#endif

	jobId.updateClassAd(*ad);

	unparser.Unparse(adbuffer, ad);
	printf("%s\n", adbuffer.c_str() );

	sub();
	return 0;
}

