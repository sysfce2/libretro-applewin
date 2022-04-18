#pragma pack(push)
#pragma pack(1)

struct CheckPointSet_t
{
  uint16_t startAddress;
  uint16_t endAddress;
  bool stop;
  bool enabled;
  uint8_t operation;
  bool temporary;
};

struct MemoryGet_t
{
  bool sideEffects;
  uint16_t startAddress;
  uint16_t endAddress;
  uint8_t memspace;
  uint16_t bankID;
};

#pragma pack(pop)
