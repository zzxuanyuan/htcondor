// File: t_dynamic_library.C
// Author: Francesco Giacomini <francesco.giacomini@cnaf.infn.it>
// Copyright 2002 Istituto Nazionale di Fisica Nucleare (INFN)

// $Id: t_dynamic_library.C,v 1.1.2.1 2002-04-29 08:27:12 giaco Exp $

#include <string>
#include <iostream>
#include "DynamicLibrary.C"

using namespace std;

int
main(int argc, char* argv[])
{
  try {

    DynamicLibrary lib("libhelper.so");

    void* symbol = lib.symbol("create_helper");

    assert(symbol != 0);
  } catch (DynamicLibraryNotFound& ex) {
    cerr << "Cannot find " << ex.name() << endl;
    return 1;
  }

  return 0;
}
