#ifndef BASERESOURCE_H
#define BASERESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "basejob.h"

class BaseJob;

class BaseResource
{
 public:

	BaseResource( const char *resource_name );
	virtual ~BaseResource();

	virtual void Reconfig() {}

	virtual bool IsEmpty() = 0;
	char *ResourceName();

 protected:
	char *resourceName;
};

#endif // define BASERESOURCE_H
