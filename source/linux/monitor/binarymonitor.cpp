#include "StdAfx.h"
#include "linux/monitor/binarymonitor.h"

#include "Log.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <cstring>

void BinaryMonitor::Command::reset()
{
  stx = 0;
}

BinaryMonitor::BinaryMonitor()
{
  myServerSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
  sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(6502);
	
  if (bind(myServerSocket, (sockaddr *)&server , sizeof(server)) < 0)
  {
    LogOutput("Monitor: bind failed: %s\n", strerror(errno));
  }

  if (listen(myServerSocket, 3))
  {
    LogOutput("Monitor: listen failed: %s\n", strerror(errno));
  }
  myConnectionSocket = -1;
}

BinaryMonitor::~BinaryMonitor()
{
  if (myServerSocket >= 0)
  {
    close(myServerSocket);
  }
  
  if (myConnectionSocket >= 0)
  {
    close(myConnectionSocket);
  }
}

ssize_t BinaryMonitor::readData(char * dest, size_t len)
{
  if (myConnectionSocket >= 0)
  {
    const ssize_t data = recv(myConnectionSocket, dest, len, 0);
    if (data == 0)
    {
      LogOutput("Lost a client\n");
      close(myConnectionSocket);
      myConnectionSocket = -1;
    }
    else if (data < 0);
    {
      const int error = errno;
      if (error != EAGAIN && error != EWOULDBLOCK)
      {
        LogOutput("Lost a client\n");
        close(myConnectionSocket);
        myConnectionSocket = -1;
      }
    }
    return data;
  }
  return 0;
}


bool BinaryMonitor::readCommand()
{
  const ssize_t data = readData(reinterpret_cast<char *>(&myCommand), sizeof(Command));
  return data > 0;
}

bool BinaryMonitor::readPayload()
{
  myPayload.resize(myCommand.length);
  const ssize_t data = readData(reinterpret_cast<char *>(myPayload.data()), myPayload.size());
  return data > 0;
}

void BinaryMonitor::process()
{
  if (myConnectionSocket >= 0)
  {
    if (myCommand.stx != 0x02)
    {
      readCommand();
    }

    if (myCommand.stx == 0x02)
    {
      LogOutput("LEN: %d, REQ: %d, CMD: %x\n", myCommand.length, myCommand.request, myCommand.type);
      if (readPayload())
      {
        myCommand.reset();
      }
    }
  }
  else
  {
    sockaddr_in client;
    socklen_t len = sizeof(client);
    myConnectionSocket = accept(myServerSocket, (sockaddr *)&client, &len);
    if (myConnectionSocket >= 0)
    {
      LogOutput("Got a client\n");
    }
    myCommand.reset();
  }  
}
