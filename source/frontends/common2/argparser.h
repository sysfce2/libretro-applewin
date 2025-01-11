#include <string>

#include "frontends/common2/programoptions.h"

namespace common2
{
  struct EmulatorOptions;
  bool getEmulatorOptions(int argc, char * argv [], OptionsType type, const std::string & edition, EmulatorOptions & options);
}
