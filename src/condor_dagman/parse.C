#include "condor_common.h"

#include <list>
#include <string>

// DAGMan Includes
#include "job.h"
#include "util.h"
#include "debug.h"
#include "parse.h"
#include "parser.h"

namespace dagman {

static const char   COMMENT    = '#';

//-----------------------------------------------------------------------------
void exampleSyntax (const std::string & example) {
    debug_println (DEBUG_QUIET, "Example syntax is: %s", example.c_str());
}

//-----------------------------------------------------------------------------
class KeyWord {
  public:
    const static std::string JOB;
    const static std::string SCRIPT;
    const static std::string PARENT;
    const static std::string CHILD;
    const static std::string PRE;
    const static std::string POST;
    const static std::string DONE;
    
    const static unsigned int m_count = 6;
    const static std::string m_keywords[];
    
    inline static bool Equal (const std::string & k1,
                              const std::string & k2) {
        return !strcasecmp (k1.c_str(), k2.c_str());
    }
    
    static bool Is (const std::string & token) {
        for (unsigned int i = 0 ; i < m_count ; i++) {
            if (Equal(token, m_keywords[i])) return true;
        }
        return false;
    }
};

const std::string KeyWord::JOB    = "JOB";
const std::string KeyWord::SCRIPT = "SCRIPT";
const std::string KeyWord::PARENT = "PARENT";
const std::string KeyWord::CHILD  = "CHILD";
const std::string KeyWord::PRE    = "PRE";
const std::string KeyWord::POST   = "POST";
const std::string KeyWord::DONE   = "DONE";
const std::string KeyWord::m_keywords  [m_count] = {
    JOB, PARENT, CHILD, PRE, POST, DONE
};

//-----------------------------------------------------------------------------
bool parse (const std::string & filename, Dag *dag) {
    
    assert (dag != NULL);
    
    FILE *fp = fopen(filename.c_str(), "r");
    if (fp == NULL) {
        if (DEBUG_LEVEL(DEBUG_QUIET)) {
            debug_println (DEBUG_QUIET, "Could not open file %s for input",
                           filename.c_str());
            return false;
        }
    }

    //
    // This loop will read every line of the input file
    //
    Parser parser(fp);
    Parser::Result result = Parser::Result_OK;
    
    for (int line_num = 0 ; result != Parser::Result_EOF ; line_num++) {
        
        std::string token;
        result = parser.GetToken(token);
        
        // Ignore blank lines and comment lines
        if (result != Parser::Result_OK) continue;
        if (token[0] == COMMENT) {
            result = parser.EatLine();
            continue;
        }
        
        //
        // Handle a Job token
        //
        // Example Syntax is:  JOB j1 j1.condor [DONE]
        //
        if ( KeyWord::Equal(token,KeyWord::JOB) ) {
            const std::string example = "JOB j1 j1.condor";
            
            std::string jobName;
            result = parser.GetToken(jobName);
            
            if (result != Parser::Result_OK) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing job name",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
            
            // The JobName cannot be a keyword
            //
            if (KeyWord::Is(jobName)) {
                debug_println (DEBUG_QUIET,
                               "%s(%d): JobName cannot be a keyword.",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
            
            // Next token is the condor command file
            //
            std::string cmd;
            result = parser.GetToken(cmd);
            
            if (result != Parser::Result_OK) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing condor cmd file",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
            
            Job * job = new Job (jobName, cmd);
            if (job == NULL) debug_error (1, DEBUG_QUIET, "Out of Memory");
            
            // Check if the user has pre-definied a Job as being done
            //
            result = parser.GetToken(token);
            if (result == Parser::Result_OK) {
                //
                // Check for optional DONE keyword
                //
                if (KeyWord::Equal(token,KeyWord::DONE)) {
                    job->m_Status = Job::STATUS_DONE;
                    result = parser.GetToken(token);
                }
            }

            // There should not be any more tokens on the line
            //
            if (result == Parser::Result_OK) {
                debug_println (DEBUG_QUIET,
                               "%s(%d): Extra garbage after condor cmd file",
                               filename.c_str(), line_num);               
            }

            // Add the new Job to the Dag
            //
            if (!dag->Add (*job)) {
                if (DEBUG_LEVEL(DEBUG_QUIET)) {
                    cout << "ERROR adding JOB " << *job << " to Dag" << endl;
                }
                fclose(fp);
                return false;
            } else if (DEBUG_LEVEL(DEBUG_DEBUG_3)) {
                cout << __FUNCTION__ << ": Added JOB: "
                     << *job << endl;
            }
        }
        
        //
        // Handle a SCRIPT token
        //
        // Example Syntax is:  SCRIPT (PRE|POST) JobName ScriptName Args ...
        //
        
        else if ( KeyWord::Equal(token, KeyWord::SCRIPT) ) {
            const std::string example =
                "SCRIPT (PRE|POST) JobName Script Args ...";

            //
            // Second keyword is either PRE or POST
            //
            bool   post;

            result = parser.GetToken(token);

            if (result != Parser::Result_OK) goto MISSING_PREPOST;
            else if ( KeyWord::Equal(token,KeyWord::PRE)  ) post = false;
            else if ( KeyWord::Equal(token,KeyWord::POST) ) post = true;
            else {
              MISSING_PREPOST:
                debug_println (DEBUG_QUIET, "%s(%d): Expected PRE or POST",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
            
            Job * job = NULL;

            //
            // Third token is the JobName
            //
            std::string jobName;
            result = parser.GetToken(jobName);

            if (result != Parser::Result_OK) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing job name",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            } else if (KeyWord::Is(jobName)) {
                debug_println (DEBUG_QUIET,
                               "%s(%d): JobName cannot be a keyword.",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            } else {
                job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename.c_str(), line_num,
                                   jobName.c_str());
                    fclose(fp);
                    return false;
                }
            }

            //
            // Make sure this job doesn't already have a script
            //
            if (post ? job->m_scriptPost != NULL : job->m_scriptPre != NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Job previously assigned "
                               "a %s script.", filename.c_str(), line_num,
                               post ? "POST" : "PRE");
                fclose(fp);
                return false;
            }

            //
            // The rest of the line is the script and args
            //
            std::string rest;
            result = parser.GetLine(rest);

            if (result != Parser::Result_OK) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing script command",
                               filename.c_str(), line_num);
                fclose(fp);
                return false;
            }

            if (post) job->m_scriptPost = new Script (post, rest, job);
            else      job->m_scriptPre  = new Script (post, rest, job);

            debug_printf (DEBUG_DEBUG_3,
                          "%s: Added %s script to %s: %s\n",
                          __FUNCTION__, (post ? "POST" : "PRE "),
                          jobName.c_str(), rest.c_str());
        }

        //
        // Handle a Dependency token
        //
        // Example Syntax is:  PARENT p1 p2 p3 ... CHILD c1 c2 c3 ...
        //
        else if ( KeyWord::Equal(token,KeyWord::PARENT) ) {
            const std::string example = "PARENT p1 p2 p3 CHILD c1 c2 c3";
            
            std::list<Job *> parents;

            std::string jobName;

            while ((result = parser.GetToken(jobName)) == Parser::Result_OK &&
                   !KeyWord::Equal(jobName,KeyWord::CHILD)) {
                if (KeyWord::Is(jobName)) {
                    debug_println (DEBUG_QUIET,
                                   "%s(%d): JobName cannot be a keyword.",
                                   filename.c_str(), line_num);
                    exampleSyntax (example);
                    fclose(fp);
                    return false;
                }
                Job * job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename.c_str(), line_num,
                                   jobName.c_str());
                    fclose(fp);
                    return false;
                }
                parents.push_back (job);
            }

            // There must be one or more parent job names before
            // the CHILD token
            if (parents.size() < 1) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing Parent Job names",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            if (result != Parser::Result_OK) {
                debug_println (DEBUG_QUIET, "%s(%d): Expected CHILD token",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            std::list<Job *> children;

            while ((result = parser.GetToken(jobName)) == Parser::Result_OK) {
                Job * job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename.c_str(), line_num,
                                   jobName.c_str());
                    fclose(fp);
                    return false;
                }
                children.push_back (job);
            }

            if (children.size() < 1) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing Child Job names",
                               filename.c_str(), line_num);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            //
            // Now add all the dependencies
            //

            std::list<Job *>::iterator pjobit;
            for (pjobit = parents.begin() ; pjobit != parents.end() ;
                 pjobit++) {
                std::list<Job *>::iterator cjobit;
                for (cjobit = children.begin() ; cjobit != children.end() ;
                     cjobit++) {
                    if (!dag->AddDependency (*pjobit, *cjobit)) {
                        debug_println (DEBUG_QUIET,
                                       "Failed to add dependency to dag");
                        fclose(fp);
                        return false;
                    }
                    if (DEBUG_LEVEL(DEBUG_DEBUG_3)) {
                        cout << __FUNCTION__ << ": Added Dependency PARENT: "
                             << **pjobit << "  CHILD: " << **cjobit << endl;
                    }
                }
            }

        } else {
            //
            // Bad token in the input file
            //
            debug_println( DEBUG_QUIET,
                           "%s(%d): Expected JOB, SCRIPT, or PARENT token",
                           filename.c_str(), line_num );
            fclose(fp);
            return false;
        }
	
    }
    return true;
}

} // namespace dagman
