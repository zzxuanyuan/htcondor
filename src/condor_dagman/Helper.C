// File: Helper.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: Helper.C,v 1.1.2.1 2002-04-29 08:27:11 giaco Exp $

#include <string>
#include <Helper.h>
#include <HelperImpl.h>

HelperImpl* get_helper(std::string type);

Helper::Helper(std::string type)
{
  m_impl = get_helper(type);

  // why save the HelperImpl* in m_impl?
  // what about calling get_helper() every time it is needed?
}

Helper::~Helper()
{
}

bool
Helper::resolve(std::string input_file, std::string output_file) const
{
  assert(input_file != "");

  if (output_file == "") {
    output_file = input_file + ".hlp";
  }

  return m_impl->resolve(input_file, output_file);
};

