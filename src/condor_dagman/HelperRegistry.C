// File: HelperRegistry.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperRegistry.C,v 1.1.2.2 2002-06-03 13:34:05 giaco Exp $

#include <algorithm>
#include "HelperRegistry.h"
#include "HelperImpl.h"
#include "DynamicLibrary.h"

using namespace std;

struct DeleteValue             // see ESTL, item 7
{
  template<typename T>
  void operator()(const T& obj) const
  {
    HelperRegistry::value_type value = obj.second;

    delete value.first;         // HelperImpl*
    delete value.second;        // DynamicLibrary*
  }
};

HelperRegistry* HelperRegistry::s_instance = 0;

HelperRegistry*
HelperRegistry::instance()
{
  if (s_instance == 0) {
    s_instance = new HelperRegistry;
  }

  return s_instance;
}

HelperRegistry::HelperRegistry()
{
}

HelperRegistry::~HelperRegistry()
{
  // delete all the remaining HelperImpl's and release the
  // corresponding shared libs

  // see ESTL, item 7
  for_each(m_helpers.begin(), m_helpers.end(), DeleteValue());
}

HelperRegistry::value_type
HelperRegistry::add(string key, value_type value)
{
  m_helpers[key] = value;

  return value;
}

HelperRegistry::value_type
HelperRegistry::remove(string key)
{
  value_type result = value_type(0, 0);

  HelpersMap::iterator it = m_helpers.find(key);

  if (it != m_helpers.end()) {
    swap(result, it->second);
  }

  return result;
}

HelperRegistry::value_type
HelperRegistry::lookup(string key)
{
  value_type result = value_type(0, 0);

  HelpersMap::iterator it = m_helpers.find(key);

  if (it != m_helpers.end()) {
    result = it->second;
  }

  return result;
}

class HelperEqual
{
  HelperImpl* m_helper;

public:
  HelperEqual(HelperImpl* helper): m_helper(helper) {}
  bool operator()(const HelperRegistry::HelpersMap::value_type& v)
  {
    return v.second.first == m_helper;
  }
};

HelperRegistry::value_type
HelperRegistry::lookup(HelperImpl* helper)
{
  value_type result = value_type(0, 0);

  HelpersMap::iterator it =
    find_if(m_helpers.begin(), m_helpers.end(), HelperEqual(helper));

  if (it != m_helpers.end()) {
    result = it->second;
  }

  return result;
}
