// File: HelperRegistry.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperRegistry.C,v 1.1.2.1 2002-04-29 08:27:11 giaco Exp $

#include <algorithm>
#include "HelperRegistry.h"
#include "HelperImpl.h"

using namespace std;

struct DeleteHelper             // see ESTL, item 7
{
  template<typename T>
  void operator()(const T& obj) const
  {
    delete obj.second;
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
  // see ESTL, item 7
  for_each(m_helpers.begin(), m_helpers.end(), DeleteHelper());
}

HelperImpl*
HelperRegistry::add(string key, HelperImpl* helper)
{
  m_helpers[key] = helper;

  return helper;
}

HelperImpl*
HelperRegistry::remove(string key)
{
  HelperImpl* result = m_helpers[key];

  m_helpers[key] = 0;

  return result;
}

HelperImpl*
HelperRegistry::lookup(string key)
{
  return m_helpers[key];
}

