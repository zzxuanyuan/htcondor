// File: HelperFactory.C
// Author: Francesco Giacomini (francesco.giacomini@cnaf.infn.it)
// Copyright (C) 2002 Istituto Nazionale di Fisica Nucleare

// $Id: HelperFactory.C,v 1.1.2.1 2002-06-03 13:34:05 giaco Exp $

#include "HelperFactory.h"
#include <fstream>
#include <utility>
#include "HelperImpl.h"
#include "NullHelper.h"
#include "DynamicLibrary.h"
#include "HelperRegistry.h"
#include "classad_distribution.h"

HelperRegistry::value_type
load_helper(std::string helper_type, const ClassAd* configuration);

HelperFactory* HelperFactory::s_instance = 0;

HelperFactory*
HelperFactory::instance()
{
  if (s_instance == 0) {
    s_instance = new HelperFactory;
  }

  return s_instance;
}

HelperFactory::HelperFactory()
  : m_configuration(0)
{
  // where is the configuration file?

  const char* conf_file_env = getenv("HELPER_CONF_FILE");
  assert(conf_file_env != 0);

  // read the contents of the configuration file into a string

  ifstream conf_file(conf_file_env);
  assert(conf_file);

  string ad_str;

  char c;
  while (conf_file >> c) {
    ad_str.push_back(c);
  }

  // parse the string into a classad

  ClassAdParser parser;

  m_configuration = parser.ParseClassAd(ad_str, true);
}

HelperFactory::~HelperFactory()
{
  delete m_configuration;
}

HelperImpl*
HelperFactory::get_helper(std::string helper_type)
{
  HelperRegistry::value_type value =
    HelperRegistry::instance()->lookup(helper_type);

  if (value.first == 0) {

    if (helper_type == "null") {

      value = HelperRegistry::value_type(new NullHelper(), 0);

    } else if (m_configuration != 0) {

      // try load the helper from a shared lib
      value = load_helper(helper_type, m_configuration);

    }
  }

  if (value.first != 0) {
    HelperRegistry::instance()->add(helper_type, value);
  }

  return value.first;
}

void
HelperFactory::release_helper(HelperImpl* helper)
{
}

HelperRegistry::value_type
load_helper(std::string helper_type, const ClassAd* configuration)
{
  // search an entry for the required helper to get the following two
  // parameters; an entry for a helper has the form:
  //
  // <helper_type> = [
  //      library = <dynamic library to load>;
  //      configuration = [ <helper-specific parameters> ]
  // ]

  string helper_library;                   // mandatory
  const ClassAd* helper_configuration = 0; // optional

  for (ClassAd::iterator it = configuration->begin();
       it != configuration->end(); ++it) {
    string attribute = it->first;
    ExprTree* expression = it->second;

    if (attribute != helper_type) {
      continue;
    }

    assert(expression->GetKind() == ExprTree::CLASSAD_NODE);

    const ClassAd* helper_ad = dynamic_cast<const ClassAd*>(expression);

    // we have find the right classad; read the contents

    { // library
      ExprTree* expr = helper_ad->Lookup("library");
      assert(expr != 0 && expr->GetKind() == ExprTree::LITERAL_NODE);
      Value value;
      dynamic_cast<const Literal*>(expr)->GetValue(value);
      string tmp;
      if (value.IsStringValue(tmp)) {
        helper_library = tmp;
      }

      assert(library != "");    // mandatory!
    }

    { // configuration
      ExprTree* expr = helper_ad->Lookup("configuration");
      if (expr != 0 && expr->GetKind() == ExprTree::CLASSAD_NODE) {
        helper_configuration = dynamic_cast<const ClassAd*>(expr);
      }
    }
  }


  // try to open the dynamic library, look for the factory method and call it

  HelperImpl* helper = 0;
  DynamicLibrary* lib = 0;

  try {

    lib = new DynamicLibrary(helper_library);

    HelperImpl* (*creator)(const ClassAd*) =
      (HelperImpl* (*)(const ClassAd*))lib->symbol("create_helper");

    if (creator != 0) {
      helper = creator(helper_configuration);
    }

  } catch (DynamicLibraryNotFound& ex) {
  }

  return HelperRegistry::value_type(helper, lib);
}


#endif // HELPER_FACTORY_H

// Local Variables:
// mode: c++
// End:

