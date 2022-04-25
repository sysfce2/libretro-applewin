#include "StdAfx.h"
#include "linux/monitor/binarybuffer.h"

std::vector<uint8_t> & BinaryBuffer::getData()
{
  return myBuffer;
}

const std::vector<uint8_t> & BinaryBuffer::getData() const
{
  return myBuffer;
}

uint8_t * BinaryBuffer::enlargeBuffer(const size_t size)
{
  const size_t current = myBuffer.size();
  myBuffer.resize(current + size);
  uint8_t * data = myBuffer.data() + current;
  return data;
}

void BinaryBuffer::writeString(const char * value)
{
  const size_t len = strlen(value);
  writeInt8(len);

  uint8_t * dest = enlargeBuffer(len);
  memcpy(dest, value, len);
}

void BinaryBuffer::writeInt32(const uint32_t value)
{
  uint8_t * dest = enlargeBuffer(sizeof(value));
  uint32_t * data = (uint32_t *)dest;
  *data = value;
}

void BinaryBuffer::writeInt16(const uint16_t value)
{
  uint8_t * dest = enlargeBuffer(sizeof(value));
  uint16_t * data = (uint16_t *)dest;
  *data = value;
}

void BinaryBuffer::writeInt8(const uint8_t value)
{
  uint8_t * dest = enlargeBuffer(sizeof(value));
  *dest = value;
}

void BinaryBuffer::writeMem(const void * ptr, const size_t size)
{
  uint8_t * dest = enlargeBuffer(size);
  memcpy(dest, ptr, size);
}
