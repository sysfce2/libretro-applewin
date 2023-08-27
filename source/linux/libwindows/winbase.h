#pragma once

#include "wincompat.h"
#include "winhandles.h"

#include <cstddef>

DWORD       WINAPI GetLastError(void);
void DeleteCriticalSection(CRITICAL_SECTION * criticalSection);
void InitializeCriticalSection(CRITICAL_SECTION * criticalSection);
void EnterCriticalSection(CRITICAL_SECTION * criticalSection);
void LeaveCriticalSection(CRITICAL_SECTION * criticalSection);
void OutputDebugString(const char * str);
BOOL WINAPI SetCurrentDirectory(LPCSTR path);

#define CBR_110	0xFF10
#define CBR_300	0xFF11
#define CBR_600	0xFF12
#define CBR_1200	0xFF13
#define CBR_2400	0xFF14
#define CBR_4800	0xFF15
#define CBR_9600	0xFF16
#define CBR_19200	0xFF18
#define CBR_115200      0xFF21

#define NOPARITY	0
#define ODDPARITY	1
#define EVENPARITY	2
#define MARKPARITY	3
#define SPACEPARITY	4

#define ONESTOPBIT	0
#define ONE5STOPBITS	1
#define TWOSTOPBITS	2

#define MS_CTS_ON           ((DWORD)0x0010)
#define MS_DSR_ON           ((DWORD)0x0020)
#define MS_RLSD_ON          ((DWORD)0x0080)

#define	DTR_CONTROL_DISABLE	0
#define	DTR_CONTROL_ENABLE	1

#define	RTS_CONTROL_DISABLE	0
#define	RTS_CONTROL_ENABLE	1

#define EV_RXCHAR    0x0001
#define EV_TXEMPTY   0x0004
#define EV_CTS       0x0008
#define EV_DSR       0x0010
#define EV_RLSD      0x0020

#define CE_RXOVER	0x0001

#define INFINITE      0xFFFFFFFF
#define WAIT_OBJECT_0           0
#define WAIT_FAILED             0xffffffff
#define STATUS_PENDING          0x00000103
#define STILL_ACTIVE            STATUS_PENDING

#define FILE_FLAG_OVERLAPPED            0x40000000

#define THREAD_BASE_PRIORITY_LOWRT  15
#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT

typedef DWORD (CALLBACK *LPTHREAD_START_ROUTINE)(LPVOID);
typedef size_t SIZE_T;

// not the original layout to ease compilation
typedef struct _OVERLAPPED {
        ULONG_PTR Internal;
        ULONG_PTR InternalHigh;
        DWORD Offset;
        DWORD OffsetHigh;
        HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

BOOL        WINAPI SetThreadPriority(HANDLE,INT);
DWORD       WINAPI WaitForSingleObject(const HANDLE,DWORD);
DWORD       WINAPI WaitForMultipleObjects(DWORD,const HANDLE*,BOOL,DWORD);
BOOL        WINAPI WaitCommEvent(HANDLE,LPDWORD,LPOVERLAPPED);
BOOL        WINAPI GetExitCodeThread(HANDLE,LPDWORD);
BOOL        WINAPI SetEvent(HANDLE);
VOID        WINAPI Sleep(DWORD);
HANDLE      WINAPI CreateEvent(LPSECURITY_ATTRIBUTES,BOOL,BOOL,LPCSTR);
HANDLE      WINAPI CreateThread(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER;

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER*);

HANDLE CreateSemaphore(
  LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
  LONG                  lInitialCount,
  LONG                  lMaximumCount,
  LPCSTR                lpName
);

typedef struct tagCOMSTAT
{
    DWORD fCtsHold : 1;
    DWORD fDsrHold : 1;
    DWORD fRlsdHold : 1;
    DWORD fXoffHold : 1;
    DWORD fXoffSent : 1;
    DWORD fEof : 1;
    DWORD fTxim : 1;
    DWORD fReserved : 25;
    DWORD cbInQue;
    DWORD cbOutQue;
} COMSTAT, *LPCOMSTAT;

typedef struct tagDCB
{
    DWORD DCBlength;
    DWORD BaudRate;
    unsigned fBinary               :1;
    unsigned fParity               :1;
    unsigned fOutxCtsFlow          :1;
    unsigned fOutxDsrFlow          :1;
    unsigned fDtrControl           :2;
    unsigned fDsrSensitivity       :1;
    unsigned fTXContinueOnXoff     :1;
    unsigned fOutX                 :1;
    unsigned fInX                  :1;
    unsigned fErrorChar            :1;
    unsigned fNull                 :1;
    unsigned fRtsControl           :2;
    unsigned fAbortOnError         :1;
    unsigned fDummy2               :17;
    WORD wReserved;
    WORD XonLim;
    WORD XoffLim;
    BYTE ByteSize;
    BYTE Parity;
    BYTE StopBits;
    char XonChar;
    char XoffChar;
    char ErrorChar;
    char EofChar;
    char EvtChar;
    WORD wReserved1;
} DCB, *LPDCB;

typedef struct tagCOMMTIMEOUTS {
	DWORD	ReadIntervalTimeout;
	DWORD	ReadTotalTimeoutMultiplier;
	DWORD	ReadTotalTimeoutConstant;
	DWORD	WriteTotalTimeoutMultiplier;
	DWORD	WriteTotalTimeoutConstant;
} COMMTIMEOUTS,*LPCOMMTIMEOUTS;

BOOL        WINAPI GetCommState(HANDLE,LPDCB);
BOOL        WINAPI SetCommState(HANDLE,LPDCB);
BOOL        WINAPI GetCommModemStatus(HANDLE,LPDWORD);
BOOL        WINAPI SetCommTimeouts(HANDLE,LPCOMMTIMEOUTS);
BOOL        WINAPI ClearCommError(HANDLE,LPDWORD,LPCOMSTAT);
BOOL        WINAPI SetCommMask(HANDLE,DWORD);
