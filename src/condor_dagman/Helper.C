// File: Helper.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: Helper.C,v 1.1.2.2 2002-06-03 13:34:05 giaco Exp $

#include <string>
#include <Helper.h>
#include <HelperImpl.h>

HelperImpl* get_helper(std::string type);
void release_helper(HelperImpl* helper);

Helper::Helper(std::string type)
{
  m_impl = get_helper(type);
}

Helper::~Helper()
{
  release_helper(m_impl);
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

