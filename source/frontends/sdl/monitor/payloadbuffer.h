#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace binarymonitor
{

  class PayloadBuffer
  {
  public:
    struct LengthException
    {
      uint8_t type;
    };

    PayloadBuffer(const uint8_t * begin, const uint8_t * end, const uint8_t type);
    PayloadBuffer(const std::vector<uint8_t> & payload, const uint8_t type);

    template <typename T>
    const T & read()
    {
      const T * ptr = (const T *)ensureAvailable(sizeof(T));
      return *ptr;
    }

    // this advances the pointer by size bytes
    const uint8_t * ensureAvailable(const size_t size);

    // read a uint8_t-prefixed string
    std::string readString();

    // read a uint8_t-prefixed sub-buffer
    PayloadBuffer readSubBuffer();

  private:
    const uint8_t * myBegin;
    const uint8_t * myEnd;
    const uint8_t myType;  // used to throw the LENGTH exception
  };

}
