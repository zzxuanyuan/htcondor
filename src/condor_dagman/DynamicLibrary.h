// File: DynamicLibrary.h
// Author: Reiner Hauser (reiner.hauser@cern.ch) original version
//         Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) Istituto Nazionale di Fisica Nucleare (INFN)

// $Id: DynamicLibrary.h,v 1.1.2.2 2002-06-03 13:34:05 giaco Exp $

#ifndef DYNAMIC_LIBRARY_H
#define DYNAMIC_LIBRARY_H

#include <string>

class DynamicLibraryNotFound
{
  std::string m_name;

public:
  DynamicLibraryNotFound(const std::string& name): m_name(name) {};
  ~DynamicLibraryNotFound() {}
  std::string name() const { return m_name; }
};

class DynamicLibrary
{
  void* m_handle;

  DynamicLibrary(const DynamicLibrary& other); // not implemented
  DynamicLibrary& operator=(const DynamicLibrary& other); // not implemented

public:
  explicit DynamicLibrary(const std::string& name);
  ~DynamicLibrary();

  void* symbol(const std::string& name);
};

#endif // DYNAMIC_LIBRARY_H

// Local Variables:
// mode: c++
// End:

