// File: planner.h
// Author: Francesco Giacomini <Francesco.Giacomini@cnaf.infn.it>
// Copyright (c) 2002 EU DataGrid.
// For license conditions see http://www.eu-datagrid.org/license.html

// $Id: planner.h,v 1.1.2.1 2002-11-11 14:46:07 giaco Exp $

#ifndef PLANNER_H
#define PLANNER_H

#include <string>

class Planner
{
  class PlannerImpl;

  PlannerImpl* m_impl;

public:
  Planner();
  ~Planner();
  std::string resolve(std::string const& input_file) const;
};

#endif

// Local Variables:
// mode:c++
// End:
