// File: NullHelper.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: NullHelper.h,v 1.1.2.1 2002-06-03 13:34:05 giaco Exp $

#ifndef NULL_HELPER_H
#define NULL_HELPER_H

#include <string>

#ifndef HELPER_IMPL_H
#include "HelperImpl.h"
#endif

// this is a null helper, i.e. a helper that does nothing
// it is intended for testing and legacy code

class NullHelper: public HelperImpl
{
  NullHelper(const NullHelper& other); // not implemented
  NullHelper& operator=(const NullHelper& other); // not implemented

public:
  NullHelper() {}
  ~NullHelper() {}

  virtual bool resolve(std::string input_file, std::string output_file) const
  {
    return false;
  }
};

#endif // NULL_HELPER_H

// Local Variables:
// mode: c++
// End:
