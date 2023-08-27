#pragma once

#include <wincompat.h>

typedef struct WSAData
{
    WORD                    wVersion;
    WORD                    wHighVersion;
} WSADATA, *LPWSADATA;

#define FD_READ                    0x00000001
#define FD_WRITE                   0x00000002
#define FD_OOB                     0x00000004
#define FD_ACCEPT                  0x00000008
#define FD_CONNECT                 0x00000010
#define FD_CLOSE                   0x00000020

#define SOCKET int
#define SOCKADDR_IN sockaddr_in
#define SOCKET_ERROR (-1)
#define closesocket close

#ifndef INVALID_SOCKET
#define INVALID_SOCKET             (SOCKET)(~0)
#endif

typedef struct sockaddr * LPSOCKADDR;

int WINAPI WSAStartup(WORD,LPWSADATA);
int WINAPI WSACleanup(void);
int WINAPI WSAAsyncSelect(SOCKET,HWND,UINT,LONG);
