// File: CopyHelper.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: CopyHelper.h,v 1.1.2.2 2002-06-03 13:34:04 giaco Exp $

#ifndef COPY_HELPER_H
#define COPY_HELPER_H

#ifndef HELPER_IMPL_H
#include "HelperImpl.h"
#endif

class CopyHelper: public HelperImpl
{
  CopyHelper(const CopyHelper& other); // not implemented
  CopyHelper& operator=(const CopyHelper& other); // not implemented

public:
  CopyHelper();
  ~CopyHelper();

  virtual bool resolve(std::string input_file, std::string output_file) const;
};

#endif // COPY_HELPER_H

// Local Variables:
// mode: c++
// End:

