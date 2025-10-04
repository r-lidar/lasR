#ifdef EXECUTABLE

#include <string>

bool execute(const std::string& config_file, const std::string& async_communication_file = "");

int main(int argc, char* argv[])
{
  if (argc != 2) return 1;
  std::string file = argv[1];
  return execute(file);
}

#endif
