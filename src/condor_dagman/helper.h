// File: helper.h
// Author: Francesco Giacomini <Francesco.Giacomini@cnaf.infn.it>
// Copyright (c) 2002 EU DataGrid.
// For license conditions see http://www.eu-datagrid.org/license.html

// $Id: helper.h,v 1.1.2.1 2002-12-23 13:14:22 giaco Exp $

#ifndef HELPER_H
#define HELPER_H

#include <string>

class Helper
{
  class HelperImpl;

  HelperImpl* m_impl;

public:
  Helper(std::string const& );
  ~Helper();
  std::string resolve(std::string const& input_file) const;
};

#endif

// Local Variables:
// mode:c++
// End:
