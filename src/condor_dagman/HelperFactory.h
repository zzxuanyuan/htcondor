// File: HelperFactory.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperFactory.h,v 1.1.2.1 2002-06-03 13:34:05 giaco Exp $

#ifndef HELPER_FACTORY_H
#define HELPER_FACTORY_H

#include <string>

class ClassAd;
class HelperImpl;

class HelperFactory
{
  // shared libs for helpers are specified in a configuration file

  ClassAd* m_configuration;

  HelperFactory();

  HelperFactory(const HelperFactory& other); // not implemented
  HelperFactory& operator=(const HelperFactory& other); // not implemented

public:
  ~HelperFactory();

public:
  HelperImpl* get_helper(std::string helper_type);
  void release_helper(HelperImpl* helper);

  // singleton
private:
  static HelperFactory* s_instance;

public:
  static HelperFactory* instance();
};


#endif // HELPER_FACTORY_H

// Local Variables:
// mode: c++
// End:

