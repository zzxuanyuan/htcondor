#ifndef DAGMAN_PARSER_H
#define DAGMAN_PARSER_H

#include <string>
#include "condor_system.h"   /* for <stdio.h> */

namespace dagman {

class Parser {
  public:
    enum Result { Result_OK, Result_EOL, Result_EOF };
    static const std::string ResultNames[];
    Parser (FILE * fp) : m_fp(fp) {}
    Result GetToken (std::string & token);
    Result GetLine (std::string & line);
    Result GetChar (char & c, bool quote = true);
    Result EatLine ();
  private:
    FILE * m_fp;
};

} // namespace dagman

#endif // ifndef DAGMAN_PARSER_H
