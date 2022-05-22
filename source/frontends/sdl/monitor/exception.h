#include <cstdint>

namespace binarymonitor
{

  struct BinaryException
  {
    uint8_t type;
    uint8_t error;
  };

  void throwBinaryException(const uint8_t type, const uint8_t error) __attribute__((noreturn));

}
