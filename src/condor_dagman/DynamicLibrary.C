// File: DynamicLibrary.C
// Author: Reiner Hauser (reiner.hauser@cern.ch) original version
//         Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) Istituto Nazionale di Fisica Nucleare (INFN)

// $Id: DynamicLibrary.C,v 1.1.2.2 2002-06-03 13:34:04 giaco Exp $

#include <dlfcn.h>
#include <string>

#include "DynamicLibrary.h"

DynamicLibrary::DynamicLibrary(const std::string& name)
    : m_handle(0)
{
  m_handle = dlopen(name.c_str(), RTLD_NOW | RTLD_GLOBAL);

  if (m_handle == 0) {
    throw DynamicLibraryNotFound(name);
  }
}

DynamicLibrary::~DynamicLibrary()
{
  dlclose(m_handle);
}

void*
DynamicLibrary::symbol(const std::string& name)
{
  return dlsym(m_handle, name.c_str());
}
