#ifndef USING_R

#include <string>

bool process(const std::string&);

int main(int argc, char* argv[])
{
  if (argc != 2) return 1;
  std::string file = argv[1];
  return process(file);
}

#endif