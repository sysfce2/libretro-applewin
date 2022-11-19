#include <cstdint>
#include <cstddef>

namespace binarymonitor
{

  enum t_binary_command
  {
    e_MON_CMD_INVALID = 0x00,

    e_MON_CMD_MEM_GET = 0x01,
    e_MON_CMD_MEM_SET = 0x02,

    e_MON_CMD_CHECKPOINT_GET = 0x11,
    e_MON_CMD_CHECKPOINT_SET = 0x12,
    e_MON_CMD_CHECKPOINT_DELETE = 0x13,
    e_MON_CMD_CHECKPOINT_LIST = 0x14,
    e_MON_CMD_CHECKPOINT_TOGGLE = 0x15,

    e_MON_CMD_CONDITION_SET = 0x22,

    e_MON_CMD_REGISTERS_GET = 0x31,
    e_MON_CMD_REGISTERS_SET = 0x32,

    e_MON_CMD_DUMP = 0x41,
    e_MON_CMD_UNDUMP = 0x42,

    e_MON_CMD_RESOURCE_GET = 0x51,
    e_MON_CMD_RESOURCE_SET = 0x52,

    e_MON_CMD_ADVANCE_INSTRUCTIONS = 0x71,
    e_MON_CMD_KEYBOARD_FEED = 0x72,
    e_MON_CMD_EXECUTE_UNTIL_RETURN = 0x73,

    e_MON_CMD_PING = 0x81,
    e_MON_CMD_BANKS_AVAILABLE = 0x82,
    e_MON_CMD_REGISTERS_AVAILABLE = 0x83,
    e_MON_CMD_DISPLAY_GET = 0x84,
    e_MON_CMD_VICE_INFO = 0x85,

    e_MON_CMD_PALETTE_GET = 0x91,

    e_MON_CMD_JOYPORT_SET = 0xa2,

    e_MON_CMD_USERPORT_SET = 0xb2,

    e_MON_CMD_EXIT = 0xaa,
    e_MON_CMD_QUIT = 0xbb,
    e_MON_CMD_RESET = 0xcc,
    e_MON_CMD_AUTOSTART = 0xdd,
  };

  enum t_binary_response
  {
    e_MON_RESPONSE_INVALID = 0x00,
    e_MON_RESPONSE_MEM_GET = 0x01,
    e_MON_RESPONSE_MEM_SET = 0x02,

    e_MON_RESPONSE_CHECKPOINT_INFO = 0x11,

    e_MON_RESPONSE_CHECKPOINT_DELETE = 0x13,
    e_MON_RESPONSE_CHECKPOINT_LIST = 0x14,
    e_MON_RESPONSE_CHECKPOINT_TOGGLE = 0x15,

    e_MON_RESPONSE_CONDITION_SET = 0x22,

    e_MON_RESPONSE_REGISTER_INFO = 0x31,

    e_MON_RESPONSE_DUMP = 0x41,
    e_MON_RESPONSE_UNDUMP = 0x42,

    e_MON_RESPONSE_RESOURCE_GET = 0x51,
    e_MON_RESPONSE_RESOURCE_SET = 0x52,

    e_MON_RESPONSE_JAM = 0x61,
    e_MON_RESPONSE_STOPPED = 0x62,
    e_MON_RESPONSE_RESUMED = 0x63,

    e_MON_RESPONSE_ADVANCE_INSTRUCTIONS = 0x71,
    e_MON_RESPONSE_KEYBOARD_FEED = 0x72,
    e_MON_RESPONSE_EXECUTE_UNTIL_RETURN = 0x73,

    e_MON_RESPONSE_PING = 0x81,
    e_MON_RESPONSE_BANKS_AVAILABLE = 0x82,
    e_MON_RESPONSE_REGISTERS_AVAILABLE = 0x83,
    e_MON_RESPONSE_DISPLAY_GET = 0x84,
    e_MON_RESPONSE_VICE_INFO = 0x85,

    e_MON_RESPONSE_PALETTE_GET = 0x91,

    e_MON_RESPONSE_JOYPORT_SET = 0xa2,

    e_MON_RESPONSE_USERPORT_SET = 0xb2,

    e_MON_RESPONSE_EXIT = 0xaa,
    e_MON_RESPONSE_QUIT = 0xbb,
    e_MON_RESPONSE_RESET = 0xcc,
    e_MON_RESPONSE_AUTOSTART = 0xdd,
  };

  enum t_mon_error
  {
    e_MON_ERR_OK = 0x00,
    e_MON_ERR_OBJECT_MISSING = 0x01,
    e_MON_ERR_INVALID_MEMSPACE = 0x02,
    e_MON_ERR_CMD_INVALID_LENGTH = 0x80,
    e_MON_ERR_INVALID_PARAMETER = 0x81,
    e_MON_ERR_CMD_INVALID_API_VERSION = 0x82,
    e_MON_ERR_CMD_INVALID_TYPE = 0x83,
    e_MON_ERR_CMD_FAILURE = 0x8f,
  };

  enum t_mon_resource_type
  {
    e_MON_RESOURCE_TYPE_STRING = 0x00,
    e_MON_RESOURCE_TYPE_INT = 0x01,
  };

  const char * getCommandStr(const uint8_t cmd);
  const char * getResponseStr(const uint8_t cmd);

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
