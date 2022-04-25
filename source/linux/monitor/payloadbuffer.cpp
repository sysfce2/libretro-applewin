#include "linux/monitor/payloadbuffer.h"

PayloadBuffer::PayloadBuffer(const uint8_t * begin, const uint8_t * end,
                             const uint8_t type)
  : myBegin(begin)
  , myEnd(end)
  , myType(type)
{

}

PayloadBuffer::PayloadBuffer(const std::vector<uint8_t> & payload, const uint8_t type)
  : myBegin(payload.data())
  , myEnd(payload.data() + payload.size())
  , myType(type)
{

}

const uint8_t * PayloadBuffer::ensureAvailable(const size_t size)
{
  if (myBegin + size > myEnd)
  {
    throw LengthException({myType});
  }
  const uint8_t * result = myBegin;
  myBegin += size;
  return result;
}

std::string PayloadBuffer::readString()
{
  const uint8_t size = read<uint8_t>();
  const uint8_t * begin = ensureAvailable(size);
  const std::string result(begin, begin + size);
  return result;
}

PayloadBuffer PayloadBuffer::readSubBuffer()
{
  const uint8_t size = read<uint8_t>();
  const uint8_t * begin = ensureAvailable(size);
  PayloadBuffer buffer(begin, begin + size, myType);
  return buffer;
}
