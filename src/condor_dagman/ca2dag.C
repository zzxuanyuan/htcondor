// Author: Francesco Giacomini

/********************************************************************
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

// $Id: ca2dag.C,v 1.1.2.1 2002-04-10 15:00:00 giaco Exp $

#include <iostream>
#include <strstream>
#include <vector>

#include "classad_distribution.h"

using namespace std;

struct NotADAG {};
struct NotAJob {};

class Job
{
  string m_name;
  string m_description_file;
  string m_pre_script;
  vector<string> m_pre_script_arguments;
  string m_post_script;
  vector<string> m_post_script_arguments;
  vector<string> m_children;

public:
  Job(string name, const ClassAd* ad);
  // happy with default copy ctor, operator=() and dtor

  string name(void) const { return m_name; }
  string description_file(void) const { return m_description_file; }
  string pre_script(void) const { return m_pre_script; }
  const vector<string>& pre_script_arguments(void) const
  {
    return m_pre_script_arguments;
  }
  string post_script(void) const { return m_post_script; }
  const vector<string>& post_script_arguments(void) const
  {
    return m_post_script_arguments;
  }
  const vector<string>& children(void) const
  {
    return m_children;
  }

  void to_dag(ostream& os) const;

private:
  bool extract_description_file(string&, const ClassAd* ad);
  bool extract_pre_script(string&, const ClassAd* ad);
  bool extract_pre_script_arguments(vector<string>&, const ClassAd* ad);
  bool extract_post_script(string&, const ClassAd* ad);
  bool extract_post_script_arguments(vector<string>&, const ClassAd* ad);
  bool extract_children(vector<string>&, const ClassAd* ad);

};

class DAG
{
  typedef std::vector<Job> Jobs;

  Jobs m_jobs;

public:
  // happy with default ctors and dtor
  DAG::DAG(const ClassAd* ad);
  void add(const Job& job);
  void print(ostream& os) const;
};

ostream&
operator<<(ostream& os, const DAG& dag)
{
  dag.print(os);

  return os;
}

int
main(int argc, char* argv[])
{
  string ad_str;

  char c;
  while (cin >> c) {
    ad_str.push_back(c);
  }

  ClassAdParser parser;

  ClassAd* ad = parser.ParseClassAd(ad_str, true);

  DAG* dag = 0;

  try {
   dag = new DAG(ad);
  } catch (NotADAG&) {
    return 1;
  }

  ClassAdIterator it(*ad);

  string attribute;
  const ExprTree* expression;
  for (it.ToFirst(); ! it.IsAfterLast(); it.NextAttribute(attribute, expression)) {

    bool success = it.CurrentAttribute(attribute, expression);
    if (! success) {
      break;
    }

    // ignore all the attributes but the classads wich represent jobs
    if (expression->GetKind() != ExprTree::CLASSAD_NODE) {
      continue;
    }

    const ClassAd* job_ad = dynamic_cast<const ClassAd*>(expression);

    try {
      Job tmp_job(attribute, job_ad);	// "attribute" is the job name
      dag->add(tmp_job);
    } catch (NotAJob&) {
      // ignore non-jobs
    }
  }

  cout << *dag << endl;

  delete dag;

  return 0;
}

Job::Job(string name, const ClassAd* ad)
  : m_name(name)
{
  if (! extract_description_file(m_description_file, ad)) {
    throw NotAJob();
  }
  if (extract_pre_script(m_pre_script, ad)) {
    extract_pre_script_arguments(m_pre_script_arguments, ad);
  }
  if (extract_post_script(m_post_script, ad)) {
    extract_post_script_arguments(m_post_script_arguments, ad);
  }
  extract_children(m_children, ad);
}

bool
Job::extract_description_file(string& description_file, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("DescriptionFile");
  if (expr != 0 && expr->GetKind() == ExprTree::LITERAL_NODE) {
    Value value;
    dynamic_cast<const Literal*>(expr)->GetValue(value);
    if (value.IsStringValue(description_file)) {
      result = true;
    }
  }
  
  return result;
}

bool
Job::extract_pre_script(string& script, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("PreScript");
  if (expr != 0 && expr->GetKind() == ExprTree::LITERAL_NODE) {
    Value value;
    dynamic_cast<const Literal*>(expr)->GetValue(value);
    if (value.IsStringValue(script)) {
      result = true;
    }
  }

  return result;
}

bool
Job::extract_pre_script_arguments(vector<string>& arguments, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("PreScriptArgs");
  if (expr != 0 && expr->GetKind() == ExprTree::EXPR_LIST_NODE) {

    vector<ExprTree*> args;
    dynamic_cast<const ExprList*>(expr)->GetComponents(args);

    for (vector<ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {

      ExprTree* arg_expr = *it;
      if (arg_expr->GetKind() == ExprTree::LITERAL_NODE) {
	Value value;
	dynamic_cast<Literal*>(arg_expr)->GetValue(value);
	string arg;
	if (value.IsStringValue(arg)) {
	  arguments.push_back(arg);
	}
      }

    }

    if (args.size() == arguments.size()) {
      // we haven't encountered any error
      result = true;
    }
  }

  return result;
}

bool
Job::extract_post_script(string& script, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("PostScript");
  if (expr != 0 && expr->GetKind() == ExprTree::LITERAL_NODE) {
    Value value;
    dynamic_cast<const Literal*>(expr)->GetValue(value);
    if (value.IsStringValue(script)) {
      result = true;
    }
  }

  return result;
}

bool
Job::extract_post_script_arguments(vector<string>& arguments, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("PostScriptArgs");
  if (expr != 0 && expr->GetKind() == ExprTree::EXPR_LIST_NODE) {

    vector<ExprTree*> args;
    dynamic_cast<const ExprList*>(expr)->GetComponents(args);

    for (vector<ExprTree*>::iterator it = args.begin(); it != args.end(); ++it) {

      ExprTree* arg_expr = *it;
      if (arg_expr->GetKind() == ExprTree::LITERAL_NODE) {
	Value value;
	dynamic_cast<Literal*>(arg_expr)->GetValue(value);
	string arg;
	if (value.IsStringValue(arg)) {
	  arguments.push_back(arg);
	}
      }

    }

    if (args.size() == arguments.size()) {
      // we haven't encountered any error
      result = true;
    }

  }

  return result;
}

bool
Job::extract_children(vector<string>& children, const ClassAd* ad)
{
  bool result = false;

  ExprTree* expr = ad->Lookup("Children");
  if (expr != 0 && expr->GetKind() == ExprTree::EXPR_LIST_NODE) {

    vector<ExprTree*> children_expr;
    dynamic_cast<const ExprList*>(expr)->GetComponents(children_expr);

    for (vector<ExprTree*>::iterator it = children_expr.begin(); it != children_expr.end(); ++it) {

      ExprTree* child_expr = *it;
      if (child_expr->GetKind() == ExprTree::LITERAL_NODE) {
	Value value;
	dynamic_cast<Literal*>(child_expr)->GetValue(value);
	string child;
	if (value.IsStringValue(child)) {
	  children.push_back(child);
	}
      }

    }

    if (children_expr.size() == children.size()) {
      // we haven't encountered any error
      result = true;
    }

  }

  return result;
}

void
Job::to_dag(ostream& os) const
{
  os << "JOB " << m_name << " " << m_description_file << endl;

  if (m_pre_script != "") {
    os << "Script PRE " << m_name << " " << m_pre_script;
    for (vector<string>::const_iterator it = m_pre_script_arguments.begin();
         it != m_pre_script_arguments.end(); ++it) {
      os << " " << *it;
    }
    os << endl;
  }

  if (m_post_script != "") {
    os << "Script POST " << m_name << " " << m_post_script;
    for (vector<string>::const_iterator it = m_post_script_arguments.begin();
         it != m_post_script_arguments.end(); ++it) {
      os << " " << *it;
    }
    os << endl;
  }

  if (! m_children.empty()) {
    os << "PARENT " << m_name << " CHILD";
    for (vector<string>::const_iterator it = m_children.begin();
         it != m_children.end(); ++it) {
      os << " " << *it;
    }
    os << endl;
  }
}

DAG::DAG(const ClassAd* ad)
{
  // check if we are dealing with a DAG

  bool success = false;

  if (ad != 0) {
    ExprTree* expr = ad->Lookup("Type");
    if (expr != 0 && expr->GetKind() == ExprTree::LITERAL_NODE) {
      Value value;
      dynamic_cast<const Literal*>(expr)->GetValue(value);
      string type;
      if (value.IsStringValue(type) && type == "DAG") {
        success = true;
      }
    }
  }

  if (! success) {
    throw NotADAG();
  }
}

void
DAG::add(const Job& job)
{
  m_jobs.push_back(job);
}

void
DAG::print(ostream& os) const
{
  for (Jobs::const_iterator it = m_jobs.begin(); it != m_jobs.end(); ++it) {
    if (it != m_jobs.begin()) {
      os << endl;
    }
    os << "JOB " << it->name() << " " << it->description_file();
  }

  for (Jobs::const_iterator it = m_jobs.begin(); it != m_jobs.end(); ++it) {

    if (it->pre_script() != "") {
      os << endl;
      os << "SCRIPT PRE " << it->name() << " " << it->pre_script();
      for (vector<string>::const_iterator arg = it->pre_script_arguments().begin();
           arg != it->pre_script_arguments().end(); ++arg) {
        os << " " << *arg;
      }
    }

    if (it->post_script() != "") {
      os << endl;
      os << "SCRIPT POST " << it->name() << " " << it->post_script();
      for (vector<string>::const_iterator arg = it->post_script_arguments().begin();
           arg != it->post_script_arguments().end(); ++arg) {

        os << " " << *arg;
      }
    }

  }

  for (Jobs::const_iterator it = m_jobs.begin(); it != m_jobs.end(); ++it) {

    if (! it->children().empty()) {
      os << endl;
      os << "PARENT " << it->name() << " CHILD";
      for (vector<string>::const_iterator child = it->children().begin();
           child != it->children().end(); ++child) {
        os << " " << *child;
      }
    }
  }

}

