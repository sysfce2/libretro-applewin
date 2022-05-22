#include "frontends/sdl/monitor/exception.h"

namespace binarymonitor
{

  void throwBinaryException(const uint8_t type, const uint8_t error)
  {
    throw BinaryException({type, error});
  }

}
