#include "StdAfx.h"
#include "linux/monitor/binarymonitor.h"

#include "Log.h"
#include "Interface.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

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
  , myAlive(true)
{
  LogOutput("New client: %d\n", mySocket);
}

BinaryClient::~BinaryClient()
{
  LogOutput("Del client: %d\n", mySocket);
  close(mySocket);
}

ssize_t BinaryClient::readData(char * dest, size_t len)
{
  if (!myAlive)
  {
    return 0;
  }

  const ssize_t data = recv(mySocket, dest, len, 0);
  if (data == 0)
  {
    myAlive = false;
  }
  else if (data < 0);
  {
    const int error = errno;
    if (error != EAGAIN && error != EWOULDBLOCK)
    {
      myAlive = false;
    }
  }
  return data;
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

void BinaryClient::sendReply(const uint8_t type, const uint32_t request, const uint8_t error)
{
  if (myAlive)
  {
    myResponse.stx = myCommand.stx;
    myResponse.version = myCommand.version;
    myResponse.type = type;
    myResponse.error = error;
    myResponse.request = request;
    myResponse.length = myPayloadOut.size();
    ssize_t sent = send(mySocket, &myResponse, sizeof(Response), 0);
    if (!myPayloadOut.empty())
    {
      sent = send(mySocket, myPayloadOut.data(), myPayloadOut.size(), 0);
    }
    LogOutput("RESPONSE: %d, LEN: %9d, REQ: %9d, CMD: 0x%2x, ERR: 0x%02x\n", mySocket, myResponse.length, myResponse.request, myResponse.type, myResponse.error);
  }
}

bool BinaryClient::process()
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
  return myAlive;
}

void BinaryClient::cmdViceInfo()
{
  myPayloadOut.resize(10);
  myPayloadOut[0] = 4;
  myPayloadOut[1] = 3;
  myPayloadOut[2] = 6;
  myPayloadOut[3] = 1;
  myPayloadOut[4] = 0;

  myPayloadOut[5] = 4;
  myPayloadOut[6] = 0;
  myPayloadOut[7] = 0;
  myPayloadOut[8] = 0;
  myPayloadOut[9] = 0;
  sendReply(e_MON_RESPONSE_VICE_INFO, myCommand.request, e_MON_ERR_OK);
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
      else
      {
        sendError(e_MON_RESPONSE_RESOURCE_GET, e_MON_ERR_OBJECT_MISSING);
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

  writeInt16(1);
  const char * name = "PC";
  const uint8_t size = 1 + 1 + 1 + strlen(name);
  writeInt8(size);
  writeInt8(0);
  writeInt8(16);
  writeString(name);
  sendReply(e_MON_RESPONSE_REGISTERS_AVAILABLE, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdDisplayGet()
{
  if (myPayloadIn.size() < 2)
  {
    sendError(e_MON_RESPONSE_DISPLAY_GET, e_MON_ERR_CMD_INVALID_LENGTH);
    return;
  }

  const uint8_t format = myPayloadIn[1];
  if (format != 0)
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
  const uint8_t bpp = 8;

  writeInt16(width);
  writeInt16(height);
  writeInt16(x);
  writeInt16(y);
  writeInt16(sw);
  writeInt16(sh);
  writeInt8(bpp);

  const uint32_t displayBuffer = width * height;
  writeInt32(displayBuffer);

  const size_t size = myPayloadOut.size();
  myPayloadOut.resize(size + displayBuffer);
  
  uint8_t * dest = myPayloadOut.data() + size;

  const uint8_t * data = video.GetFrameBuffer();
  for (size_t i = 0; i < displayBuffer; ++i)
  {
    *dest = *(data + 1);
    dest += 1;
    data += 4;
  }
  sendReply(e_MON_RESPONSE_DISPLAY_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::cmdBanksAvailable()
{
  writeInt16(1);
  const char * name = "ram";
  const uint8_t size = 2 + 1 + strlen(name);
  writeInt8(size);
  writeInt16(0);
  writeString(name);
  sendReply(e_MON_RESPONSE_BANKS_AVAILABLE, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::writeString(const char * value)
{
  const size_t len = strlen(value);
  const size_t size = myPayloadOut.size();
  myPayloadOut.resize(size + 1 + len);
  uint8_t * dest = myPayloadOut.data() + size;

  dest[0] = len;
  void * data = dest + 1;
  memcpy(data, value, len);
}

void BinaryClient::writeInt32(const uint32_t value)
{
  const size_t size = myPayloadOut.size();
  myPayloadOut.resize(size + sizeof(value));
  uint8_t * dest = myPayloadOut.data() + size;

  uint32_t * data = (uint32_t *)dest;
  *data = value;
}

void BinaryClient::writeInt16(const uint16_t value)
{
  const size_t size = myPayloadOut.size();
  myPayloadOut.resize(size + sizeof(value));
  uint8_t * dest = myPayloadOut.data() + size;

  uint16_t * data = (uint16_t *)dest;
  *data = value;
}

void BinaryClient::writeInt8(const uint8_t value)
{
  const size_t size = myPayloadOut.size();
  myPayloadOut.resize(size + sizeof(value));
  uint8_t * dest = myPayloadOut.data() + size;
  *dest = value;
}

void BinaryClient::sendResourceStringReply(const uint32_t request, const uint8_t error, const char * value)
{
  writeInt8(e_MON_RESOURCE_TYPE_STRING);
  writeString(value);
  sendReply(e_MON_RESPONSE_RESOURCE_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::sendResourceIntReply(const uint32_t request, const uint8_t error, const uint32_t value)
{
  writeInt8(e_MON_RESOURCE_TYPE_INT);
  writeInt8(sizeof(value));
  writeInt32(value);
  sendReply(e_MON_RESPONSE_RESOURCE_GET, myCommand.request, e_MON_ERR_OK);
}

void BinaryClient::sendError(const uint8_t type, const uint8_t error)
{
  sendReply(type, myCommand.request, error);
}

void BinaryClient::processCommand()
{
  myPayloadOut.clear();
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
    default:
      sendError(myCommand.type, e_MON_ERR_CMD_INVALID_TYPE);
      break;
  }
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
  const int clientSocket = accept4(mySocket, (sockaddr *)&client, &len, SOCK_NONBLOCK);
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
    const bool ok = (*iter)->process();
    if (ok)
    {
      ++iter;
    }
    else
    {
      iter = myClients.erase(iter);
    }
  }
}
