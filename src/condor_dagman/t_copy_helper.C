// File: t_copy_helper.C
// Author: Francesco Giacomini <francesco.giacomini@cnaf.infn.it>
// Copyright 2002 Istituto Nazionale di Fisica Nucleare (INFN)

// $Id: t_copy_helper.C,v 1.1.2.1 2002-04-29 08:27:11 giaco Exp $

#include <string>
#include "Helper.h"

int
main(int argc, char* argv[])
{
  Helper helper("copy");

  bool result = helper.resolve("copy_in");
  assert(result);

  result = helper.resolve("copy_in", "copy_out");
  assert(result);

  return 0;
}
