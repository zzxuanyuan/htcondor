// File: Helper.h
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: Helper.h,v 1.1.2.1 2002-04-29 08:27:12 giaco Exp $

#ifndef HELPER_H
#define HELPER_H

#include <string>

class HelperImpl;

/**
 * \class Helper Helper.h "Helper.h"
 * A Helper transforms the contents of a given input file and places them in a
 * new file. The name of the output file, if not specified, is obtained
 * postfixing the name of the input file with the string ".hlp".
 * \author Francesco Giacomini
 * \brief A Helper transforms the contents of a file
 */

class Helper
{
  HelperImpl* m_impl;

  Helper(const Helper& other);  // not implemented
  Helper& operator=(const Helper& other);  // not implemented

public:
  /**
   * \brief The Helper constructor
   * \param type the type of helper
   */
  Helper(std::string type);
  /**
   * \brief The Helper destructor
   */
  ~Helper();

  /**
   * \param input_file its contents are the subject of the transformation
   * \param output_file it will contain the modified contents of input_file. If
   output_file does not exist it is created. If output_file already exists it is
   overwritten.
   * \return true if and only if a modification occured
   * \brief Operate the transformation on the input file
   */
  bool resolve(std::string input_file, std::string output_file = "") const;
};

#endif // HELPER_H

// Local Variables:
// mode: c++
// End:

