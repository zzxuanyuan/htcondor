// File: t_null_helper.C
// Author: Francesco Giacomini <francesco.giacomini@cnaf.infn.it>
// Copyright 2002 Istituto Nazionale di Fisica Nucleare (INFN)

// $Id: t_null_helper.C,v 1.1.2.1 2002-04-29 08:27:12 giaco Exp $

#include <string>
#include "Helper.h"

int
main(int argc, char* argv[])
{
  Helper helper("null");

  assert(! helper.resolve("f1"));

  return 0;
}
