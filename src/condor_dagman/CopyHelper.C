// File: CopyHelper.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: CopyHelper.C,v 1.1.2.1 2002-04-29 08:26:59 giaco Exp $

#include <string>
#include <fstream>
#include "CopyHelper.h"
#include "classad_distribution.h"

using namespace std;

CopyHelper::CopyHelper()
{
}

CopyHelper::~CopyHelper()
{
}

bool
CopyHelper::resolve(string input_file, string output_file) const
{
  ifstream fin(input_file.c_str());
  if (! fin) {
    throw FileNotFound();
  }

  ofstream fout(output_file.c_str());
  if (! fout) {
    throw FileNotFound();
  }

  // read the input file

  string ad_str;

  char c;
  while (fin >> c) {
    ad_str.push_back(c);
  }

  ClassAdParser parser;

  ClassAd* ad = parser.ParseClassAd(ad_str, true);

  if (ad == 0) {
    throw NotAClassAd();
  }

  // write the output file

  PrettyPrint unparser;
  string unparsed;

  unparser.Unparse(unparsed, ad);

  fout << unparsed << endl;

  return true;
}

extern "C" {
  CopyHelper* create_helper(void)
  {
    return new CopyHelper();
  }
}

    
