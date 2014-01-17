#include "condor_common.h"
#include "condor_config.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "cached_server.h"



CachedServer::CachedServer():
  m_registered_handlers(false)
{
  
  if (m_registered_handlers) {
    return;
  }
    
    
}

CachedServer::~CachedServer() {
  
  
  
  
}


void
CachedServer::InitAndReconfig() {
  
  
}
