// File: get_helper.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: get_helper.C,v 1.1.2.1 2002-04-29 08:27:11 giaco Exp $

#include <string>
#include <fstream>
#include "HelperImpl.h"
#include "DynamicLibrary.h"
#include "HelperRegistry.h"

using namespace std;

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

HelperImpl*
get_helper(std::string type)
{
  HelperImpl* result = HelperRegistry::instance()->lookup(type);

  if (result == 0) {

    if (type == "null") {

      result = new NullHelper();

    } else if (type == "copy") {

      // get the .so from a configuration file

      try {

        DynamicLibrary* lib = new DynamicLibrary("libcopyhelper.so");

        HelperImpl* (*creator)(void) =
          (HelperImpl* (*)(void))lib->symbol("create_helper");

        if (creator != 0) {
          result = creator();
        }

      } catch (DynamicLibraryNotFound& ex) {
      }

      // who deletes lib? not here, otherwise the HelperImpl is not available
    }

    HelperRegistry::instance()->add(type, result);
  }
  
  return result;
}
