#ifndef PARSE_H
#define PARSE_H

#include <string>

#include "dag.h"

namespace dagman {
bool parse (const std::string & filename, Dag * dag);
}

#endif
