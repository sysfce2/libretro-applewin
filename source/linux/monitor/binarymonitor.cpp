#include "StdAfx.h"
#include "linux/monitor/binarymonitor.h"
#include "linux/monitor/binarybuffer.h"
#include "linux/monitor/commands.h"

#include "Log.h"
#include "Interface.h"
#include "CPU.h"
#include "Memory.h"
#include "Debugger/Debug.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

#define MON_EVENT_ID 0xffffffff

enum t_binary_command {
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

enum t_binary_response {
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

enum t_mon_error {
  e_MON_ERR_OK = 0x00,
  e_MON_ERR_OBJECT_MISSING = 0x01,
  e_MON_ERR_INVALID_MEMSPACE = 0x02,
  e_MON_ERR_CMD_INVALID_LENGTH = 0x80,
  e_MON_ERR_INVALID_PARAMETER = 0x81,
  e_MON_ERR_CMD_INVALID_API_VERSION = 0x82,
  e_MON_ERR_CMD_INVALID_TYPE = 0x83,
  e_MON_ERR_CMD_FAILURE = 0x8f,
};

enum t_mon_resource_type {
  e_MON_RESOURCE_TYPE_STRING = 0x00,
  e_MON_RESOURCE_TYPE_INT = 0x01,
};

void BinaryClient::Command::reset()
{
  stx = 0;
}

BinaryClient::BinaryClient(const int socket)
  : mySocket(socket)
  , myRunning(true)
{
  myCommand.reset();
  LogOutput("New client: %d\n", mySocket);
}

BinaryClient::~BinaryClient()
{
  LogOutput("Del client: %d\n", mySocket);
  close(mySocket);
}

ssize_t BinaryClient::readData(char * dest, size_t len)
{
  const ssize_t data = recv(mySocket, dest, len, MSG_DONTWAIT);
  if (data == 0)
  {
    throw std::runtime_error("graceful termination");
  }
  else
  {
    throwIfError(data);
  }
  return data;
}

void BinaryClient::throwIfError(const ssize_t result)
{
  if (result < 0);
  {
    const int error = errno;
    if (error != EAGAIN && error != EWOULDBLOCK)
    {
      throw std::runtime_error(strerror(error));
    }
  }
}

bool BinaryClient::readCommand()
{
  const ssize_t data = readData(reinterpret_cast<char *>(&myCommand), sizeof(Command));
  return data > 0;
}

bool BinaryClient::readPayload()
{
  myPayloadIn.resize(myCommand.length);
  if (myCommand.length)
  {
    const ssize_t data = readData(reinterpret_cast<char *>(myPayloadIn.data()), myPayloadIn.size());
    return data > 0;
  }
  else
  {
    return true;
  }
}

void BinaryClient::sendReply(const BinaryBuffer & buffer, const uint8_t type, const uint32_t request, const uint8_t error)
{
  const std::vector<uint8_t> & data = buffer.getData();
  myResponse.stx = 0x02;
  myResponse.version = 0x02;
  myResponse.type = type;
  myResponse.error = error;
  myResponse.request = request;
  myResponse.length = data.size();
  ssize_t sent1 = send(mySocket, &myResponse, sizeof(Response), MSG_NOSIGNAL);
  throwIfError(sent1);
  if (sent1 == sizeof(Response) && !data.empty())
  {
    ssize_t sent2 = send(mySocket, data.data(), data.size(), MSG_NOSIGNAL);
    throwIfError(sent2);
    sent1 += sent2;
  }
  const bool ok = sent1 == (sizeof(Response) + data.size());
  LogOutput("RESPONSE: %d, LEN: %9d, REQ: %9d, CMD: 0x%02x, ERR: 0x%02x, OK: %d\n", mySocket, myResponse.length, myResponse.request, myResponse.type, myResponse.error, ok);
  LogOutput("PAYLOAD:");
  for (size_t i = 0; i < std::min(30UL, data.size()); ++i)
  {
    LogOutput(" %02x", data[i]);
  }
  LogOutput("\n");
}

void BinaryClient::sendBreakpoint(const uint32_t request, const uint8_t error, const size_t i)
{
  if (i >= MAX_BREAKPOINTS || !g_aBreakpoints[i].bSet)
  {
    sendError(e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_INVALID_PARAMETER);
    return;
  }

  Breakpoint_t & bp = g_aBreakpoints[i];

  BinaryBuffer buffer;
  buffer.writeInt32(i);
  buffer.writeInt8(0);
  buffer.writeInt16(bp.nAddress);
  buffer.writeInt16(bp.nAddress + bp.nLength - 1);
  buffer.writeInt8(1);
  buffer.writeInt8(bp.bEnabled);
  buffer.writeInt8(4);
  buffer.writeInt8(bp.bTemp);
  buffer.writeInt32(0);
  buffer.writeInt32(0);
  buffer.writeInt8(0);
  buffer.writeInt8(0);
  sendReply(buffer, e_MON_RESPONSE_CHECKPOINT_INFO, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::process()
{
  if (myCommand.stx != 0x02)
  {
    readCommand();
  }

  if (myCommand.stx == 0x02)
  {
    LogOutput("COMMAND:  %d, LEN: %9d, REQ: %9d, CMD: 0x%02x\n", mySocket, myCommand.length, myCommand.request, myCommand.type);
    if (readPayload())
    {
      processCommand();
      myCommand.reset();
    }
  }
}

void BinaryClient::cmdViceInfo()
{
  BinaryBuffer buffer;
  uint8_t * dest = buffer.enlargeBuffer(10);
  dest[0] = 4;
  dest[1] = 3;
  dest[2] = 6;
  dest[3] = 2;
  dest[4] = 0;

  dest[5] = 4;
  dest[6] = 0;
  dest[7] = 0;
  dest[8] = 0;
  dest[9] = 0;
  sendReply(buffer, e_MON_RESPONSE_VICE_INFO, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdResourceGet()
{
  if (!myPayloadIn.empty())
  {
    const uint8_t length = myPayloadIn[0];
    if (myPayloadIn.size() >= 1 + length)
    {
      const std::string name(myPayloadIn.data() + 1, myPayloadIn.data() + 1 + length);
      LogOutput("ResourceGet: %s\n", name.c_str());
      if (name == "MonitorServer")
      {
        sendResourceIntReply(myCommand.request, e_MON_ERR_OK, 0);
      } 
      else if (name == "MonitorServerAddress")
      {
        sendResourceStringReply(myCommand.request, e_MON_ERR_OK, "127.0.0.1");
      }
      else if (name == "VICIIPaletteFile")
      {
        sendResourceStringReply(myCommand.request, e_MON_ERR_OK, "pepto-ntsc");
      }
      return;
    }
  }
  sendError(e_MON_RESPONSE_RESOURCE_GET, e_MON_ERR_CMD_INVALID_LENGTH);
}

void BinaryClient::cmdRegistersAvailable()
{
  if (myPayloadIn.size() < 1)
  {
    sendError(e_MON_RESPONSE_REGISTERS_AVAILABLE, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const uint8_t memspace = myPayloadIn[0];
  if (memspace != 0)
  {
    sendError(e_MON_RESPONSE_REGISTERS_AVAILABLE, e_MON_ERR_INVALID_MEMSPACE);
    return;
  }

  BinaryBuffer buffer;
  buffer.writeInt16(1);

  {
    BinaryBufferSize<uint8_t> binarySize(buffer);
    buffer.writeInt8(0);       // ID
    buffer.writeInt8(16);      // bits
    const char * name = "PC";
    buffer.writeString(name);
  }

  sendReply(buffer, e_MON_RESPONSE_REGISTERS_AVAILABLE, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdRegistersGet()
{
  if (myPayloadIn.size() < 1)
  {
    sendError(e_MON_RESPONSE_REGISTER_INFO, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const uint8_t memspace = myPayloadIn[0];
  if (memspace != 0)
  {
    sendError(e_MON_RESPONSE_REGISTER_INFO, e_MON_ERR_INVALID_MEMSPACE);
    return;
  }

  sendRegisters(myCommand.request);
}

void BinaryClient::sendRegisters(const uint32_t request)
{
  BinaryBuffer buffer;
  buffer.writeInt16(1);

  {
    BinaryBufferSize<uint8_t> binarySize(buffer);
    buffer.writeInt8(0);
    buffer.writeInt16(regs.pc);
  }

  sendReply(buffer, e_MON_RESPONSE_REGISTER_INFO, request, e_MON_ERR_OK);
}

void BinaryClient::cmdDisplayGet()
{
  if (myPayloadIn.size() < 2)
  {
    sendError(e_MON_RESPONSE_DISPLAY_GET, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const uint8_t format = myPayloadIn[1];
  if (format != 0) // RGBA
  {
    sendError(e_MON_RESPONSE_DISPLAY_GET, e_MON_ERR_INVALID_PARAMETER);
    return;
  }

  const uint32_t infoLength = 13;

  Video & video = GetVideo();
  const uint16_t width = video.GetFrameBufferWidth();
  const uint16_t height = video.GetFrameBufferHeight();
  const uint16_t x = video.GetFrameBufferBorderWidth();
  const uint16_t y = video.GetFrameBufferBorderHeight();
  const uint16_t sw = video.GetFrameBufferBorderlessWidth();
  const uint16_t sh = video.GetFrameBufferBorderlessHeight();

  typedef uint8_t pixel_t;

  const uint8_t bpp = sizeof(pixel_t) * 8;

  BinaryBuffer buffer;

  {
    BinaryBufferSize<uint32_t> binarySize(buffer);
    buffer.writeInt16(width);
    buffer.writeInt16(height);
    buffer.writeInt16(x);
    buffer.writeInt16(y);
    buffer.writeInt16(sw);
    buffer.writeInt16(sh);
    buffer.writeInt8(bpp);
  }

  const uint32_t displayBuffer = width * height * sizeof(pixel_t);
  buffer.writeInt32(displayBuffer);

  uint8_t * dest = buffer.enlargeBuffer(displayBuffer);
  const bgra_t * data = (const bgra_t *)video.GetFrameBuffer();

  for (size_t i = 0; i < displayBuffer; ++i)
  {
    const bgra_t & source = data[i];
    const uint8_t grey = (source.b + source.g + source.r) / 3;
    dest[i] = grey;
  }

  sendReply(buffer, e_MON_RESPONSE_DISPLAY_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdPaletteGet()
{
  BinaryBuffer buffer;
  buffer.writeInt16(0x0100);
  for (size_t i = 0; i < 0x0100; ++i)
  {
    BinaryBufferSize<uint8_t> binarySize(buffer);
    buffer.writeInt8(i);
    buffer.writeInt8(i);
    buffer.writeInt8(i);
  }
  sendReply(buffer, e_MON_RESPONSE_PALETTE_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdBanksAvailable()
{
  BinaryBuffer buffer;
  buffer.writeInt16(1);

  {
    BinaryBufferSize<uint8_t> binarySize(buffer);
    buffer.writeInt16(0);
    const char * name = "ram";
    buffer.writeString(name);
  }

  sendReply(buffer, e_MON_RESPONSE_BANKS_AVAILABLE, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::sendStopped()
{
  BinaryBuffer buffer;
  buffer.writeInt16(regs.pc);
  sendReply(buffer, e_MON_RESPONSE_STOPPED, MON_EVENT_ID, e_MON_ERR_OK);
}

void BinaryClient::cmdCheckpointSet()
{
  if (myPayloadIn.size() < sizeof(CheckpointSet_t))
  {
    sendError(e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const CheckpointSet_t & checkpointSet = *(const CheckpointSet_t *)myPayloadIn.data();

  size_t i = 0;
  while ((i < MAX_BREAKPOINTS) && g_aBreakpoints[i].bSet)
  {
    ++i;
  }

  if (i >= MAX_BREAKPOINTS)
  {
    sendError(e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_INVALID_PARAMETER);
    return;
  }

  Breakpoint_t & bp = g_aBreakpoints[i];
  bp.nAddress = checkpointSet.startAddress;
  bp.nLength = checkpointSet.endAddress - checkpointSet.startAddress + 1;
  bp.eSource = BP_SRC_REG_PC;
  bp.bSet = true;
  bp.eOperator = BP_OP_EQUAL;
  bp.bEnabled = checkpointSet.enabled;
  bp.bTemp = checkpointSet.temporary;
  ++g_nBreakpoints;

  sendBreakpoint(myCommand.request, e_MON_ERR_OK, i);
}

void BinaryClient::cmdCheckpointToggle()
{
  if (myPayloadIn.size() < sizeof(CheckpointToggle_t))
  {
    sendError(e_MON_RESPONSE_CHECKPOINT_TOGGLE, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const CheckpointToggle_t & checkpointToggle = *(const CheckpointToggle_t *)myPayloadIn.data();

  if (checkpointToggle.id >= MAX_BREAKPOINTS || !g_aBreakpoints[checkpointToggle.id].bSet)
  {
    sendError(e_MON_RESPONSE_CHECKPOINT_TOGGLE, e_MON_ERR_INVALID_PARAMETER);
    return;
  }

  Breakpoint_t & bp = g_aBreakpoints[checkpointToggle.id];
  bp.bEnabled = checkpointToggle.enabled;

  BinaryBuffer buffer;
  sendReply(buffer, e_MON_RESPONSE_CHECKPOINT_TOGGLE, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdCheckpointList()
{
  size_t n = 0;
  for (size_t i = 0; i < MAX_BREAKPOINTS; ++i)
  {
    if (g_aBreakpoints[i].bSet)
    {
      ++n;
      sendBreakpoint(myCommand.request, e_MON_ERR_OK, i);
    }
  }

  BinaryBuffer buffer;
  buffer.writeInt32(n);
  sendReply(buffer, e_MON_RESPONSE_CHECKPOINT_LIST, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdMemoryGet()
{
  if (myPayloadIn.size() < sizeof(MemoryGet_t))
  {
    sendError(e_MON_RESPONSE_MEM_GET, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const MemoryGet_t & memoryGet = *(const MemoryGet_t *)myPayloadIn.data();

  if (memoryGet.memspace != 0 || memoryGet.bankID != 0)
  {
    sendError(e_MON_RESPONSE_MEM_GET, e_MON_ERR_INVALID_PARAMETER);
    return;
  }

  BinaryBuffer buffer;
  const uint16_t length = memoryGet.endAddress - memoryGet.startAddress + 1;

  {
    BinaryBufferSize<uint16_t> binarySize(buffer);
    uint8_t * dest = buffer.enlargeBuffer(length);
    memcpy(dest, mem + memoryGet.startAddress, length);
  }

  sendReply(buffer, e_MON_RESPONSE_MEM_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdAutostart()
{
  if (myPayloadIn.size() < sizeof(Autostart_t))
  {
    sendError(e_MON_RESPONSE_AUTOSTART, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const Autostart_t & autostart = *(const Autostart_t *)myPayloadIn.data();

  if (myPayloadIn.size() < sizeof(Autostart_t) + autostart.length)
  {
    sendError(e_MON_RESPONSE_AUTOSTART, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const char * begin = (const char *)(myPayloadIn.data() + sizeof(Autostart_t));
  const std::string filename(begin, begin + autostart.length);

  BinaryBuffer buffer;
  sendReply(buffer, e_MON_RESPONSE_AUTOSTART, myCommand.request, e_MON_ERR_OK);

  if (autostart.run)
  {
    myRunning = true;
    sendResume(MON_EVENT_ID);
  }
}


void BinaryClient::sendResourceStringReply(const uint32_t request, const uint8_t error, const char * value)
{
  BinaryBuffer buffer;
  buffer.writeInt8(e_MON_RESOURCE_TYPE_STRING);
  buffer.writeString(value);
  sendReply(buffer, e_MON_RESPONSE_RESOURCE_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::sendResourceIntReply(const uint32_t request, const uint8_t error, const uint32_t value)
{
  BinaryBuffer buffer;
  buffer.writeInt8(e_MON_RESOURCE_TYPE_INT);

  {
    BinaryBufferSize<uint8_t> binarySize(buffer);
    buffer.writeInt32(value);
  }

  sendReply(buffer, e_MON_RESPONSE_RESOURCE_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::sendError(const uint8_t type, const uint8_t error)
{
  BinaryBuffer buffer;
  sendReply(buffer, type, myCommand.request, error);
}

void BinaryClient::sendResume(const uint32_t request)
{
  BinaryBuffer buffer;
  buffer.writeInt16(regs.pc);
  sendReply(buffer, e_MON_RESPONSE_RESUMED, request, e_MON_ERR_OK);
}

void BinaryClient::cmdExit()
{
  myRunning = true;
  BinaryBuffer buffer;
  sendReply(buffer, e_MON_RESPONSE_EXIT, myCommand.request, e_MON_ERR_OK);
  sendResume(MON_EVENT_ID);
}

void BinaryClient::cmdQuit()
{
  BinaryBuffer buffer;
  sendReply(buffer, e_MON_RESPONSE_QUIT, myCommand.request, e_MON_ERR_OK);
  throw std::runtime_error("quit");
}

void BinaryClient::cmdPing()
{
  BinaryBuffer buffer;
  sendReply(buffer, e_MON_RESPONSE_PING, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::processCommand()
{
  if (myRunning)
  {
    sendRegisters(MON_EVENT_ID);
    sendStopped();
    myRunning = false;
  }

  switch (myCommand.type)
  {
    case e_MON_CMD_VICE_INFO:
      cmdViceInfo();
      break;
    case e_MON_CMD_RESOURCE_GET:
      cmdResourceGet();
      break;
    case e_MON_CMD_REGISTERS_AVAILABLE:
      cmdRegistersAvailable();
      break;
    case e_MON_CMD_BANKS_AVAILABLE:
      cmdBanksAvailable();
      break;
    case e_MON_CMD_DISPLAY_GET:
      cmdDisplayGet();
      break;
    case e_MON_CMD_PALETTE_GET:
      cmdPaletteGet();
      break;
    case e_MON_CMD_CHECKPOINT_SET:
      cmdCheckpointSet();
      break;
    case e_MON_CMD_CHECKPOINT_LIST:
      cmdCheckpointList();
      break;
    case e_MON_CMD_CHECKPOINT_TOGGLE:
      cmdCheckpointToggle();
      break;
    case e_MON_CMD_REGISTERS_GET:
      cmdRegistersGet();
      break;
    case e_MON_CMD_MEM_GET:
      cmdMemoryGet();
      break;
    case e_MON_CMD_PING:
      cmdPing();
      break;
    case e_MON_CMD_EXIT:
      cmdExit();
      break;
    case e_MON_CMD_AUTOSTART:
      cmdAutostart();
      break;
    case e_MON_CMD_QUIT:
      cmdQuit();
      break;
    default:
      sendError(myCommand.type, e_MON_ERR_CMD_INVALID_TYPE);
      break;
  }
  LogOutput("\n");
}

BinaryMonitor::BinaryMonitor()
{
  mySocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(6502);

  if (bind(mySocket, (sockaddr *)&server , sizeof(server)) < 0)
  {
    LogOutput("Monitor: bind failed: %s\n", strerror(errno));
  }
  else if (listen(mySocket, 5))
  {
    LogOutput("Monitor: listen failed: %s\n", strerror(errno));
  }
  else
  {
    LogOutput("Monitor: listening for incoming connections\n");
  }
}

BinaryMonitor::~BinaryMonitor()
{
  if (mySocket >= 0)
  {
    LogOutput("Monitor: closing socket\n");
    close(mySocket);
  }
}

void BinaryMonitor::process()
{
  sockaddr_in client;
  socklen_t len = sizeof(client);
  const int clientSocket = accept4(mySocket, (sockaddr *)&client, &len, 0);
  if (clientSocket >= 0)
  {
    myClients.push_back(std::make_shared<BinaryClient>(clientSocket));
  }
  else
  {
    int error = errno;
    if (error != EAGAIN && error != EWOULDBLOCK)
    {
      LogOutput("Accept: %d\n", error);
    }
  }

  std::vector<std::shared_ptr<BinaryClient>>::iterator iter = myClients.begin();
  while (iter != myClients.end())
  {
    try
    {
      (*iter)->process();
      ++iter;
    }
    catch (const std::exception & e)
    {
      LogOutput("Client: %s\n", e.what());
      iter = myClients.erase(iter);
    }
  }
}
