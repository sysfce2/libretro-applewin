#include "frontends/sdl/monitor/commands.h"

namespace binarymonitor
{
  const char * getCommandStr(const uint8_t cmd)
  {
    switch (cmd)
    {
      case e_MON_CMD_VICE_INFO:
        return "CMD_VICE_INFO";
      case e_MON_CMD_RESOURCE_GET:
        return "CMD_RESOURCE_GET";
      case e_MON_CMD_REGISTERS_AVAILABLE:
        return "CMD_REGISTERS_AVAILABLE";
      case e_MON_CMD_REGISTERS_GET:
        return "CMD_REGISTERS_GET";
      case e_MON_CMD_REGISTERS_SET:
        return "CMD_REGISTERS_SET";
      case e_MON_CMD_BANKS_AVAILABLE:
        return "CMD_BANKS_AVAILABLE";
      case e_MON_CMD_DISPLAY_GET:
        return "CMD_DISPLAY_GET";
      case e_MON_CMD_PALETTE_GET:
        return "CMD_PALETTE_GET";
      case e_MON_CMD_CHECKPOINT_SET:
        return "CMD_CHECKPOINT_SET";
      case e_MON_CMD_CHECKPOINT_GET:
        return "CMD_CHECKPOINT_GET";
      case e_MON_CMD_CHECKPOINT_LIST:
        return "CMD_CHECKPOINT_LIST";
      case e_MON_CMD_CHECKPOINT_DELETE:
        return "CMD_CHECKPOINT_DELETE";
      case e_MON_CMD_CHECKPOINT_TOGGLE:
        return "CMD_CHECKPOINT_TOGGLE";
      case e_MON_CMD_MEM_GET:
        return "CMD_MEM_GET";
      case e_MON_CMD_PING:
        return "CMD_PING";
      case e_MON_CMD_EXIT:
        return "CMD_EXIT";
      case e_MON_CMD_AUTOSTART:
        return "CMD_AUTOSTART";
      case e_MON_CMD_RESET:
        return "CMD_RESET";
      case e_MON_CMD_QUIT:
        return "CMD_QUIT";
      default:
        return "UNKNOWN";
    }
  }

  const char * getResponseStr(const uint8_t cmd)
  {
    switch (cmd)
    {
      case e_MON_RESPONSE_INVALID:
        return "RESPONSE_INVALID";
      case e_MON_RESPONSE_MEM_GET:
        return "RESPONSE_MEM_GET";
      case e_MON_RESPONSE_MEM_SET:
        return "RESPONSE_MEM_SET";
      case e_MON_RESPONSE_CHECKPOINT_INFO:
        return "RESPONSE_CHECKPOINT_INFO";
      case e_MON_RESPONSE_CHECKPOINT_DELETE:
        return "RESPONSE_CHECKPOINT_DELETE";
      case e_MON_RESPONSE_CHECKPOINT_LIST:
        return "RESPONSE_CHECKPOINT_LIST";
      case e_MON_RESPONSE_CHECKPOINT_TOGGLE:
        return "RESPONSE_CHECKPOINT_TOGGLE";
      case e_MON_RESPONSE_CONDITION_SET:
        return "RESPONSE_CONDITION_SET";
      case e_MON_RESPONSE_REGISTER_INFO:
        return "RESPONSE_REGISTER_INFO";
      case e_MON_RESPONSE_DUMP:
        return "RESPONSE_DUMP";
      case e_MON_RESPONSE_UNDUMP:
        return "RESPONSE_UNDUMP";
      case e_MON_RESPONSE_RESOURCE_GET:
        return "RESPONSE_RESOURCE_GET";
      case e_MON_RESPONSE_RESOURCE_SET:
        return "RESPONSE_RESOURCE_SET";
      case e_MON_RESPONSE_JAM:
        return "RESPONSE_JAM";
      case e_MON_RESPONSE_STOPPED:
        return "RESPONSE_STOPPED";
      case e_MON_RESPONSE_RESUMED:
        return "RESPONSE_RESUMED";
      case e_MON_RESPONSE_ADVANCE_INSTRUCTIONS:
        return "RESPONSE_ADVANCE_INSTRUCTIONS";
      case e_MON_RESPONSE_KEYBOARD_FEED:
        return "RESPONSE_KEYBOARD_FEED";
      case e_MON_RESPONSE_EXECUTE_UNTIL_RETURN:
        return "RESPONSE_EXECUTE_UNTIL_RETURN";
      case e_MON_RESPONSE_PING:
        return "RESPONSE_PING";
      case e_MON_RESPONSE_BANKS_AVAILABLE:
        return "RESPONSE_BANKS_AVAILABLE";
      case e_MON_RESPONSE_REGISTERS_AVAILABLE:
        return "RESPONSE_REGISTERS_AVAILABLE";
      case e_MON_RESPONSE_DISPLAY_GET:
        return "RESPONSE_DISPLAY_GET";
      case e_MON_RESPONSE_VICE_INFO:
        return "RESPONSE_VICE_INFO";
      case e_MON_RESPONSE_PALETTE_GET:
        return "RESPONSE_PALETTE_GET";
      case e_MON_RESPONSE_JOYPORT_SET:
        return "RESPONSE_JOYPORT_SET";
      case e_MON_RESPONSE_USERPORT_SET:
        return "RESPONSE_USERPORT_SET";
      case e_MON_RESPONSE_EXIT:
        return "RESPONSE_EXIT";
      case e_MON_RESPONSE_QUIT:
        return "RESPONSE_QUIT";
      case e_MON_RESPONSE_RESET:
        return "RESPONSE_RESET";
      case e_MON_RESPONSE_AUTOSTART:
        return "RESPONSE_AUTOSTART";
      default:
        return "UNKNOWN";
    }
  }

}
