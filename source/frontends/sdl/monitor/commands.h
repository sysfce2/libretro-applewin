#include <cstdint>
#include <cstddef>

namespace binarymonitor
{

  #pragma pack(push)
  #pragma pack(1)

  struct CheckpointSet_t
  {
    uint16_t startAddress;
    uint16_t endAddress;
    bool stop;
    bool enabled;
    uint8_t operation;
    bool temporary;
  };

  struct CheckpointToggle_t
  {
    uint32_t id;
    bool enabled;
  };

  struct CheckpointGet_t
  {
    uint32_t id;
  };

  struct CheckpointDelete_t
  {
    uint32_t id;
  };

  struct MemoryGet_t
  {
    bool sideEffects;
    uint16_t startAddress;
    uint16_t endAddress;
    uint8_t memspace;
    uint16_t bankID;
  };

  struct Autostart_t
  {
    bool run;
    uint16_t fileIndex;
  };

  struct RegistersSet_t
  {
    uint8_t memspace;
    uint16_t n;
  };

  #pragma pack(pop)

}
