// File: HelperRegistry.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperRegistry.h,v 1.1.2.1 2002-04-29 08:27:13 giaco Exp $

#ifndef HELPER_REGISTRY_H
#define HELPER_REGISTRY_H

#include <string>
#include <map>

class HelperImpl;

class HelperRegistry
{

  std::map<std::string, HelperImpl*> m_helpers;

  static HelperRegistry* s_instance;

  HelperRegistry();

  HelperRegistry(const HelperRegistry& other); // not implemented
  HelperRegistry& operator=(const HelperRegistry& other); // not implemented

public:

  static HelperRegistry* instance();

  ~HelperRegistry();

public:

  HelperImpl* add(std::string key, HelperImpl* helper);
  HelperImpl* remove(std::string key);
  HelperImpl* lookup(std::string key);
};

#endif // HELPER_REGISTRY_H

// Local Variables:
// mode: c++
// End:

