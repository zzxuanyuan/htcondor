// File: Helper.cpp
// Author: Francesco Giacomini <Francesco.Giacomini@cnaf.infn.it>
// Copyright (c) 2002 EU DataGrid.
// For license conditions see http://www.eu-datagrid.org/license.html

// $Id: helper.C,v 1.1.2.2 2002-12-24 11:13:27 giaco Exp $

#include "helper.h"
#include <fstream>
#include <sstream>
#include <cstdlib>		// for system()
#include <string.h>		// for strlen()
//#include "condor_debug.h"
#include "condor_config.h"

class Helper::HelperImpl
{
public:
  std::string resolve(std::string const& input_file);
};

Helper::Helper()
  : m_impl(new HelperImpl)
{
  assert(m_impl != 0);
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
  assert(! input_file.empty());
  {
    std::string valid_chars("abcdefghijklmnopqrstuvwxyz"
			    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			    "0123456789_-.");
#ifdef WIN32
    valid_chars += "\:";
#else
    valid_chars += "/";
#endif
    std::string::size_type s = input_file.find_first_not_of(valid_chars.c_str());
				     
    bool input_file_name_is_valid = s == std::string::npos;
    assert(input_file_name_is_valid);
  }
  {
    std::ifstream input_file_exists(input_file.c_str());
    assert(input_file_exists);
  }
    
  std::string output_file;

  for (int i = 1; output_file.empty(); ++i) {
    std::ostringstream s;
    s << i;
    std::string tmp = input_file + '-' + s.str() + ".help";
    std::ifstream is(tmp.c_str());
    if (! is) {			// file does not exist: good!
      output_file = tmp;
    }
  }

  std::string helper_command("/bin/cp"); // default
  char const* p = param("DAGMAN_HELPER_COMMAND");
  if (p && strlen(p) != 0) {
    helper_command = p;
  }
  helper_command += " " + input_file + " " + output_file;

  std::cout << helper_command << '\n';

  int status = system(helper_command.c_str());
  assert(status == 0);

  return output_file;
}
