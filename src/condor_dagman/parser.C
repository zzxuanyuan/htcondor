#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "parser.h"

namespace dagman {

//------------------------------------------------------------------------
const std::string Parser::ResultNames[] = {"OK", "EOL", "EOF"};

//------------------------------------------------------------------------
Parser::Result Parser::GetToken (std::string & token) {
    char c;
    Result r;
    token = "";
    bool in_token = false;
    for (r = GetChar(c) ; r == Result_OK ; r = GetChar(c)) {
        if (isspace(c)) {
            if (in_token) return Result_OK;
        } else {
            if (!in_token) in_token = true;
            token += c;
        }
    }

    if (r == Result_EOL) {
        if (! token.empty()) {
            int i = ungetc ('\n', m_fp);
            assert (i != EOF);
            return Result_OK;
        }
    }
    return r;
}

//------------------------------------------------------------------------
Parser::Result Parser::GetLine (std::string & line) {
    Result r;
    char c;
    line = "";
    for (r = GetChar(c) ; r == Result_OK ; r = GetChar(c)) line += c;
    if (r == Result_EOF && line.empty()) return Result_EOF;
    else return Result_OK;
}

//------------------------------------------------------------------------
Parser::Result Parser::GetChar (char & c, bool quote) {
    int i = fgetc(m_fp);
    switch (i) {
      case '\\':
        if (quote) {
            char c2;
            switch (GetChar(c2,false)) {
              case Result_EOL:
                c = ' ';
                return Result_OK;
              case Result_OK:
                ungetc (c2, m_fp);
              case Result_EOF:
                c = '\\';
                return Result_OK;
              default:
                assert (false);
            }
        } else {
            c = i;
            return Result_OK;
        }
        break;

      case '\r':
        i = fgetc(m_fp);
        if (i != '\n') ungetc (i, m_fp);
        return Result_EOL;

      case '\n': return Result_EOL;
      case EOF:  return Result_EOF;
      default:
        c = i;
        return Result_OK;
    }
    
}

//------------------------------------------------------------------------
Parser::Result Parser::EatLine () {
    Result r;
    char c;
    while (Result_OK == (r = GetChar(c)));
    return r;
}

//--------------------------------------------------------------------------

#if defined(TEST)
 
int main (int argc, char *argv[]) {
    cout << "This code tests the Parser class." << endl
         << "Enter a filename you wish to parse: ";
    std::string filename;
    cin >> filename;

    FILE *fp = fopen (filename.c_str(), "r");
    if (fp == NULL) {
        perror(argv[0]);
        exit(1);
    }

    Parser p(fp);
    bool quit = false;
    std::string token, line;
    char c = 'x';
    Parser::Result r = Parser::Result_OK;

    while (!quit) {
        cout << "Last  token: \"" << token << '\"' << endl;
        cout << "Last   line: \"" << line  << '\"' << endl;
        cout << "Last   char: \'" << c     << '\'' << endl;
        cout << "Last result: " << Parser::ResultNames[r] << endl << endl;

        cout << "Choose: (E)atLine Get(L)ine, Get(T)oken, "
             << "Get(C)har (Q)uit --> ";

        char choice;
        cin >> choice;

        switch (choice) {
          case 'E':
          case 'e':
            r = p.EatLine();
            break;
          case 'L':
          case 'l':
            r = p.GetLine(line);
            break;
          case 'T':
          case 't':
            r = p.GetToken(token);
            break;
          case 'C':
          case 'c':
            r = p.GetChar(c);
            break;
          case 'Q':
          case 'q':
            quit = true;
            break;
        }
    }
    return 0;
}

#endif

} // namespace dagman
