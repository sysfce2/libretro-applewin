class BinaryBuffer
{
public:
  void writeString(const char * value);
  void writeInt32(const uint32_t value);
  void writeInt16(const uint16_t value);
  void writeInt8(const uint8_t value);
  uint8_t * enlargeBuffer(const size_t size);

  std::vector<uint8_t> & getData();
  const std::vector<uint8_t> & getData() const;

private:
  std::vector<uint8_t> myBuffer;
};

template <typename T>
class BinaryBufferSize
{
public:
  BinaryBufferSize(BinaryBuffer & buffer) : myBuffer(buffer)
  {
    mySizePos = myBuffer.getData().size();
    myBuffer.enlargeBuffer(sizeof(T));
    myBegin = myBuffer.getData().size();
  }

  ~BinaryBufferSize()
  {
    const size_t end = myBuffer.getData().size();
    const size_t size = end - myBegin;
    T * dest = (T *)(myBuffer.getData().data() + mySizePos);
    *dest = size;
  }

private:
  BinaryBuffer & myBuffer;
  size_t mySizePos;
  size_t myBegin;
};