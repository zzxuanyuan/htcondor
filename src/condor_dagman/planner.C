// File: Planner.cpp
// Author: Francesco Giacomini <Francesco.Giacomini@cnaf.infn.it>
// Copyright (c) 2002 EU DataGrid.
// For license conditions see http://www.eu-datagrid.org/license.html

// $Id: planner.C,v 1.1.2.2 2002-12-05 11:33:25 giaco Exp $

#include "planner.h"
#include <fstream>
#include <memory> // for std::auto_ptr
#include "classad_distribution.h"
#include "edg/workload/common/requestad/convert.h"

using namespace classad;

namespace {
  classad::ClassAd* f_resolve(classad::ClassAd const& input_ad);

ClassAd*
f_resolve(ClassAd const& input_ad)
{
  // here goes the REAL stuff, in particular matchmaking
  return new ClassAd(input_ad);
}

}

class Planner::PlannerImpl
{
  ClassAdParser m_parser;

public:
  std::string resolve(std::string const& input_file);
};

Planner::Planner()
  : m_impl(new PlannerImpl)
{
}

Planner::~Planner()
{
  delete m_impl;
}

std::string
Planner::resolve(std::string const& input_file) const
{
  return m_impl->resolve(input_file);
}

std::string
Planner::PlannerImpl::resolve(std::string const& input_file)
{
  assert(! input_file.empty());

  std::string output_file = input_file + ".pln";

  ifstream fin(input_file.c_str());
  assert(fin.is_open());

  ofstream fout(output_file.c_str());
  assert(fout.is_open());

  std::string ad_str;
  char c;
  while (fin >> c) {
    ad_str += c;
  }

  ClassAd input_ad;
  assert(m_parser.ParseClassAd(ad_str, input_ad));

  std::auto_ptr<ClassAd> resolved_ad(f_resolve(input_ad));
  assert(resolved_ad.get() != 0);

  edg::workload::common::requestad::to_submit_stream(fout, *resolved_ad);
  fout << '\n';

  return output_file;
}
