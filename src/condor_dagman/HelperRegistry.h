// File: HelperRegistry.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperRegistry.h,v 1.1.2.2 2002-06-03 13:34:05 giaco Exp $

#ifndef HELPER_REGISTRY_H
#define HELPER_REGISTRY_H

#include <string>
#include <map>
#include <utility>

class HelperImpl;
class DynamicLibrary;

class HelperRegistry
{
public:
  typedef std::pair<HelperImpl*, DynamicLibrary*> value_type;
  typedef std::map<std::string, value_type> HelpersMap;

private:
  HelpersMap m_helpers;

  static HelperRegistry* s_instance;

  HelperRegistry();

  HelperRegistry(const HelperRegistry& other); // not implemented
  HelperRegistry& operator=(const HelperRegistry& other); // not implemented

public:

  static HelperRegistry* instance();

  ~HelperRegistry();

public:

  value_type add(std::string key, value_type value);
  value_type remove(std::string key);
  value_type lookup(std::string key);
  value_type lookup(HelperImpl* helper);
};

#endif // HELPER_REGISTRY_H

// Local Variables:
// mode: c++
// End:

