#include <string>

namespace common2
{
  struct EmulatorOptions;

  bool getEmulatorOptions(int argc, char * argv [], const std::string & edition, EmulatorOptions & options);
}
