// File: Helper.cpp
// Author: Francesco Giacomini <Francesco.Giacomini@cnaf.infn.it>
// Copyright (c) 2002 EU DataGrid.
// For license conditions see http://www.eu-datagrid.org/license.html

// $Id: helper.C,v 1.1.2.1 2002-12-23 13:14:21 giaco Exp $

#include "helper.h"
#include <fstream>
#include "condor_debug.h"
#include "condor_config.h"

class Helper::HelperImpl
{
public:
  std::string resolve(std::string const& input_file);
};

Helper::Helper()
  : m_impl(new HelperImpl)
{
  if (! m_impl) {
    EXCEPT("Out of memory (%s:%d)", __FILE__, __LINE__);
  }
}

Helper::~Helper()
{
  delete m_impl;
}

std::string
Helper::resolve(std::string const& input_file) const
{
  return m_impl->resolve(input_file);
}

std::string
Helper::HelperImpl::resolve(std::string const& input_file)
{
  ASSERT(! input_file.empty());

  std::string output_file = std::string(StatInfo(input_file.c_str()).BaseName()) + ".help";
  std::string input_file_base(StatInfo(input_file.c_str()).BaseName());
  std::string output_file;
  while (output_file.empty()) {
    std::string tmp = 
  }
  
  // iterate until the file is new (basename.<n>.help)

  std::string helper_command(param("DAGMAN_HELPER_COMMAND"));
  helper_command += " " + input_file + " " + output_file;
  
  system(helper_command);
  // check errno

  return output_file;
}
