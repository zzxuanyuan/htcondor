#include <cassert>
#include <string>
#include <fstream>
#include "helper.h"

int main(int argc, char* argv[])
{
  assert(argc == 2);
  std::string output_file(Helper().resolve(argv[1]));
  std::ofstream output_file_created(output_file.c_str());
  assert(output_file_created);
}
