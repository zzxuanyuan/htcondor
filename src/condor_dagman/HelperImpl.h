// File: HelperImpl.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperImpl.h,v 1.1.2.2 2002-06-03 13:34:05 giaco Exp $

#ifndef HELPER_IMPL_H
#define HELPER_IMPL_H

class FileNotFound {};
class NotAClassAd {};

class HelperImpl
{
  HelperImpl(const HelperImpl& other);  // not implemented
  HelperImpl& operator=(const HelperImpl& other);  // not implemented

public:
  HelperImpl() {}
  virtual ~HelperImpl() {}

  virtual bool resolve(std::string input_file, std::string output_file) const = 0;
};

#endif // HELPER_H

// Local Variables:
// mode: c++
// End:

