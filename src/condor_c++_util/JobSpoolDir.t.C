#include "JobSpoolDir.h"

int main()
{
	JobSpoolDir s;
	s.Initialize(123,4,5,true);
	//s.SetCmd("/example/executable");
	s.test();
	return 0;
}
