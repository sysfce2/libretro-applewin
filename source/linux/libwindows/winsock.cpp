#include <winsock.h>

int WINAPI WSAStartup(WORD wVersionRequired,LPWSADATA wsaData)
{
  wsaData->wVersion = wVersionRequired;
  wsaData->wHighVersion = wVersionRequired;
  return 0;
}

int WINAPI WSACleanup(void)
{
  return 0;
}

int WINAPI WSAAsyncSelect(SOCKET,HWND,UINT,LONG)
{
  return 0;
}
