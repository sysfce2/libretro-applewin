/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Debugger
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

// disable warning C4786: symbol greater than 255 character:
//#pragma warning(disable: 4786)

#include "StdAfx.h"

#include "Debug.h"
#include "Debugger_Win32.h"

#include "../Windows/AppleWin.h"
#include "../Core.h"
#include "../Interface.h"
#include "../CardManager.h"
#include "../CPU.h"
#include "../Disk.h"
#include "../Keyboard.h"
#include "../Memory.h"
#include "../NTSC.h"
#include "../SoundCore.h"	// SoundCore_SetFade()

//	#define DEBUG_COMMAND_HELP  1
//	#define DEBUG_ASM_HASH 1
#define ALLOW_INPUT_LOWERCASE 1

#define MAKE_VERSION(a,b,c,d) ((a<<24) | (b<<16) | (c<<8) | (d))

	// See /docs/Debugger_Changelog.txt for full details
	const int DEBUGGER_VERSION = MAKE_VERSION(2,9,2,8);


// Public _________________________________________________________________________________________

// All (Global)
	bool g_bDebuggerEatKey = false;

// Bookmarks __________________________________________________________________
	int        g_nBookmarks = 0;
	Bookmark_t g_aBookmarks[ MAX_BOOKMARKS ];

// Breakpoints ________________________________________________________________
	// Any Speed Breakpoints
	int  g_nDebugBreakOnInvalid  = 0; // Bit Flags of Invalid Opcode to break on: // iOpcodeType = AM_IMPLIED (BRK), AM_1, AM_2, AM_3
	int  g_iDebugBreakOnOpcode   = 0;
	bool g_bDebugBreakOnInterrupt = false;

	struct DebugBreakOnDMA
	{
		DebugBreakOnDMA() : isToOrFromMemory(0), memoryAddr(0), memoryAddrEnd(0), BPid(0) {}

		int isToOrFromMemory;
		WORD memoryAddr;
		WORD memoryAddrEnd;
		int BPid;
	};

	static const uint32_t NUM_BREAK_ON_DMA = 3;	// A 512-byte block misaligned touching 3 pages
	static DebugBreakOnDMA g_DebugBreakOnDMA[NUM_BREAK_ON_DMA];
	static DebugBreakOnDMA g_DebugBreakOnDMAIO;

	int                  g_bDebugBreakpointHit = 0;       // See: BreakpointHit_t
	static Breakpoint_t *g_pDebugBreakpointHit = nullptr; // NOTE: Only valid for BP_HIT_REG, see: CheckBreakpointsReg()

	int          g_nBreakpoints = 0;
	Breakpoint_t g_aBreakpoints[ MAX_BREAKPOINTS ];

	// NOTE: BreakpointSource_t and g_aBreakpointSource must match!
	const char *g_aBreakpointSource[ NUM_BREAKPOINT_SOURCES ] =
	{	// Used to be one char, since ArgsCook also uses // TODO/FIXME: Parser use Param[] ?
		// Used for both Input & Output!
		// Regs
		"A", // Reg A
		"X", // Reg X
		"Y", // Reg Y
		// Special
		"PC", // Program Counter -- could use "$"
		"S" , // Stack Pointer
		// Flags -- .8 Moved: Flag names from g_aFlagNames[] to "inlined" g_aBreakpointSource[]
		"P", // Processor Status
		"C", // ---- ---1 Carry
		"Z", // ---- --1- Zero
		"I", // ---- -1-- Interrupt
		"D", // ---- 1--- Decimal
		"B", // ---1 ---- Break
		"R", // --1- ---- Reserved
		"V", // -1-- ---- Overflow
		"N", // 1--- ---- Sign
		// Misc
		"OP", // Opcode/Instruction/Mnemonic
		"M", // Mem RW
		"M", // Mem READ_ONLY
		"M", // Mem WRITE_ONLY
		"V", // Video Scanner
		// TODO: M0 ram bank 0, M1 aux ram ?
	};

	// Note: BreakpointOperator_t, _PARAM_BREAKPOINT_, and g_aBreakpointSymbols must match!
	const char *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ] =
	{	// Output: Must be 2 chars!
		"<=", // LESS_EQUAL
		"< ", // LESS_THAN
		"= ", // EQUAL
		"!=", // NOT_EQUAL
//		"! ", // NOT_EQUAL_1
		"> ", // GREATER_THAN
		">=", // GREATER_EQUAL
		"? ", // READ   // Q. IO Read  use 'I'?  A. No, since I=Interrupt // Also can't use: 'r' reserver
		"@ ", // WRITE  // Q. IO Write use 'O'?  A. No, since O=Opcode    // This is free: 'w'
		"* ", // Read/Write
	};

	static WORD g_uBreakMemoryAddress = 0;

// Commands _______________________________________________________________________________________

	int g_iCommand; // last command (enum) // used for consecutive commands

	std::vector<int>       g_vPotentialCommands; // global, since TAB-completion also needs
	std::vector<Command_t> g_vSortedCommands;

//	static const char g_aFlagNames[_6502_NUM_FLAGS+1] = "CZIDBRVN";// Reversed since arrays are from left-to-right


// Cursor (Console Input) _____________________________________________________

//	char g_aInputCursor[] = "\|/-";
	enum InputCursor
	{
		CURSOR_INSERT,
		CURSOR_OVERSTRIKE,
		NUM_INPUT_CURSORS
	};
	
	const char g_aInputCursor[] = "_\x7F"; // insert over-write
	bool       g_bInputCursor = false;
	int        g_iInputCursor = CURSOR_OVERSTRIKE; // which cursor to use
	const int  g_nInputCursor = sizeof( g_aInputCursor );

	void DebuggerCursorUpdate();
	char DebuggerCursorGet();

// Cursor (Disasm) ____________________________________________________________

	WORD g_nDisasmTopAddress = 0;
	WORD g_nDisasmBotAddress = 0;
	WORD g_nDisasmCurAddress = 0;

	bool g_bDisasmCurBad    = false;
	int  g_nDisasmCurLine   = 0; // Aligned to Top or Center
	int  g_iDisasmCurState = CURSOR_NORMAL;

	int  g_nDisasmWinHeight = 0;

//	char g_aConfigDisasmAddressColon[] = " :";

	extern const int WINDOW_DATA_BYTES_PER_LINE = 8;

#if OLD_FONT
// Font
	char     g_sFontNameDefault[ MAX_FONT_NAME ] = "Courier New";
	char     g_sFontNameConsole[ MAX_FONT_NAME ] = "Courier New";
	char     g_sFontNameDisasm [ MAX_FONT_NAME ] = "Courier New";
	char     g_sFontNameInfo   [ MAX_FONT_NAME ] = "Courier New";
	char     g_sFontNameBranch [ MAX_FONT_NAME ] = "Webdings";
	HFONT     g_hFontWebDings  = (HFONT)0;
#endif
	int       g_iFontSpacing = FONT_SPACING_CLEAN;

	// TODO: This really needs to be phased out, and use the ConfigFont[] settings
#if USE_APPLE_FONT
	int       g_nFontHeight = CONSOLE_FONT_HEIGHT; // 13 -> 12 Lucida Console is readable
#else
	int       g_nFontHeight = 15; // 13 -> 12 Lucida Console is readable
#endif

	const int MIN_DISPLAY_CONSOLE_LINES =  5; // doesn't include ConsoleInput

	int g_nDisasmDisplayLines  = 0;


// Config _____________________________________________________________________

// Config - Disassembly
	bool  g_bConfigDisasmAddressView   = true;
	int   g_bConfigDisasmClick         = 4; // GH#462 alt=1, ctrl=2, shift=4 bitmask (default to Shift-Click)
	bool  g_bConfigDisasmAddressColon  = true;
	bool  g_bConfigDisasmOpcodesView   = true;
	bool  g_bConfigDisasmOpcodeSpaces  = true;
	int   g_iConfigDisasmTargets       = DISASM_TARGET_BOTH;
	int   g_iConfigDisasmBranchType    = DISASM_BRANCH_FANCY;
	int   g_bConfigDisasmImmediateChar = DISASM_IMMED_BOTH;
	int   g_iConfigDisasmScroll        = 3; // favor 3 byte opcodes
// Config - Info
	bool  g_bConfigInfoTargetPointer   = false;

	MemoryTextFile_t g_ConfigState;

	static bool g_bDebugFullSpeed      = false;
	static bool g_bLastGoCmdWasFullSpeed = false;
	static bool g_bGoCmd_ReinitFlag = false;

// Display ____________________________________________________________________

	void UpdateDisplay( Update_t bUpdate );


// Memory _____________________________________________________________________

	MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];

	// Made global so operator @# can be used with other commands.
	MemorySearchResults_t g_vMemorySearchResults;


// Profile
	const int NUM_PROFILE_LINES = NUM_OPCODES + NUM_OPMODES + 16;

	ProfileOpcode_t g_aProfileOpcodes[ NUM_OPCODES ];
	ProfileOpmode_t g_aProfileOpmodes[ NUM_OPMODES ];
	unsigned __int64 g_nProfileBeginCycles = 0; // g_nCumulativeCycles // PROFILE RESET

	const std::string g_FileNameProfile = "Profile.txt"; // changed from .csv to .txt since Excel doesn't give import options.
	int   g_nProfileLine = 0;
	char  g_aProfileLine[ NUM_PROFILE_LINES ][ CONSOLE_WIDTH ];

	void ProfileReset  ();
	bool ProfileSave   ();
	void ProfileFormat( bool bSeperateColumns, ProfileFormat_e eFormatMode );

	// TODO: Things would be much simpler if g_aProfileLine is just a container of std::string.
	struct ProfileLine_t
	{
		ProfileLine_t() : buf(NULL), bufsz(0) {}
		ProfileLine_t(char* _buf, size_t _bufsz) : buf(_buf), bufsz(_bufsz) {}
		void Assign(std::string const& str)
		{
			if (!buf || bufsz <= 0) return;
			strncpy_s(buf, bufsz, str.c_str(), _TRUNCATE);
		}
		ATTRIBUTE_FORMAT_PRINTF(2, 3) /* 1 is "this" */
		void Format(const char* fmt, ...)
		{
			if (!buf || bufsz <= 0) return;
			va_list va;
			va_start(va, fmt);
			Assign(StrFormatV(fmt, va));
			va_end(va);
		}
		char* buf;
		size_t bufsz;
	};
	ProfileLine_t ProfileLinePeek ( int iLine );
	ProfileLine_t ProfileLinePush ();
	void ProfileLineReset  ();

// Soft-switches __________________________________________________________________________________


// Source Level Debugging _________________________________________________________________________
	bool  g_bSourceLevelDebugging = false;
	bool  g_bSourceAddSymbols     = false;
	bool  g_bSourceAddMemory      = false;

	std::string g_aSourceFileName;

	MemoryTextFile_t g_AssemblerSourceBuffer;

	int    g_iSourceDisplayStart  = 0;
	int    g_nSourceAssembleBytes = 0;
	int    g_nSourceAssemblySymbols = 0;

	// TODO: Support multiple source filenames
	SourceAssembly_t g_aSourceDebug;



// Watches ________________________________________________________________________________________
	int       g_nWatches = 0;
	Watches_t g_aWatches[ MAX_WATCHES ]; // TODO: use vector<Watch_t> ??


// Window _________________________________________________________________________________________
	int           g_iWindowLast = WINDOW_CODE; // TODO: FIXME! should be offset into WindowConfig!!!
	// Who has active focus
	int           g_iWindowThis = WINDOW_CODE; // TODO: FIXME! should be offset into WindowConfig!!!
	WindowSplit_t g_aWindowConfig[ NUM_WINDOWS ];


// Zero Page Pointers _____________________________________________________________________________
	int                g_nZeroPagePointers = 0;
	ZeroPagePointers_t g_aZeroPagePointers[ MAX_ZEROPAGE_POINTERS ]; // TODO: use vector<> ?


// TODO: // CONFIG SAVE --> VERSION #
	enum DebugConfigVersion_e
	{
		VERSION_0,
		CURRENT_VERSION = VERSION_0
	};


// Misc. __________________________________________________________________________________________

	std::string g_sFileNameConfig     =
#ifdef MSDOS
		"AWDEBUGR.CFG";
#else
		"AppleWinDebugger.cfg";
#endif

	static char      g_sFileNameTrace      [] = "Trace.txt";

	static bool      g_bBenchmarking = false;

	static BOOL      g_bProfiling       = 0;
	static int       g_nDebugSteps      = 0;
	static uint32_t  g_nDebugStepCycles = 0;
	static int       g_nDebugStepStart  = 0;
	static int       g_nDebugStepUntil  = -1; // HACK: MAGIC #

	static int       g_nDebugSkipStart = 0;
	static int       g_nDebugSkipLen   = 0;

	static FILE     *g_hTraceFile       = NULL;
	static bool      g_bTraceHeader     = false; // semaphore, flag header to be printed
	static bool      g_bTraceFileWithVideoScanner = false;

	uint32_t     extbench      = 0;

	static bool      g_bIgnoreNextKey = false;

	const UINT LBR_UNDEFINED = -1;
	static UINT g_LBR = LBR_UNDEFINED;	// Last Branch Record

	static bool g_bScriptReadOk = false;

// Private ________________________________________________________________________________________


// Prototypes _______________________________________________________________
	static void DebugEnd ();

	static	int ParseInput ( LPTSTR pConsoleInput, bool bCook = true );
	static	Update_t ExecuteCommand ( int nArgs );

// Breakpoints
	Update_t _BP_InfoNone ();
	void _BWZ_ClearViaArgs ( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, int & nTotal );
	void _BWZ_EnableDisableViaArgs ( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, const bool bEnabled );
	void _BWZ_List ( const Breakpoint_t * aBreakWatchZero, const int iBWZ ); // bool bZeroBased = true );
	void _BWZ_ListAll ( const Breakpoint_t * aBreakWatchZero, const int nMax );
	void _BWZ_RemoveOne ( Breakpoint_t *aBreakWatchZero, const int iSlot, int & nTotal );
	void _BWZ_RemoveAll ( Breakpoint_t *aBreakWatchZero, const int nMax, int & nTotal );

//	bool CheckBreakpoint (WORD address, BOOL memory);
	bool _CmdBreakpointAddReg ( Breakpoint_t *pBP, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, WORD nAddress, int nLen, bool bIsTempBreakpoint );
	int  _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, bool bIsTempBreakpoint=false );

// Config - Save
	bool ConfigSave_BufferToDisk ( const char *pFileName, ConfigSave_t eConfigSave );
	void ConfigSave_PrepareHeader ( const Parameters_e eCategory, const Commands_e eCommandClear );

// Drawing
	static	void _CmdColorGet ( const int iScheme, const int iColor );

// Source Level Debugging
	static	bool BufferAssemblyListing ( const std::string & pFileName );
	static	bool ParseAssemblyListing ( bool bBytesToMemory, bool bAddSymbols );


// Window
	void _WindowJoin ();
	void _WindowSplit (Window_e eNewBottomWindow );
	void _WindowLast ();
	void _WindowSwitch ( int eNewWindow );
	int  WindowGetHeight ( int iWindow );
	void WindowUpdateDisasmSize ();
	void WindowUpdateConsoleDisplayedSize ();
	void WindowUpdateSizes                ();
	Update_t _CmdWindowViewFull   (int iNewWindow);
	Update_t _CmdWindowViewCommon (int iNewWindow);

// Utility
	void _CursorMoveDownAligned( int nDelta );
	void _CursorMoveUpAligned( int nDelta );

// DebugVideoMode _____________________________________________________________

// Fix for GH#345
// Wrap & protect the debugger's video mode in its own class:
// . This may seem like overkill but it stops the video mode being (erroneously) additionally used as a flag.
// . VideoMode is a bitmap of video flags and a VideoMode value of zero is a valid video mode (GR,PAGE1,non-mixed).
class DebugVideoMode	// NB. Implemented as a singleton
{
protected:
	DebugVideoMode()
	{
		Reset();
	}

public:
	~DebugVideoMode(){}

	static DebugVideoMode& Instance()
	{
		return m_Instance;
	}

	void Reset(void)
	{
		m_bIsVideoModeValid = false;
		m_uVideoMode = 0;
	}

	bool IsSet(void)
	{
		return m_bIsVideoModeValid;
	}

	bool Get(UINT* pVideoMode)
	{
		if (pVideoMode)
			*pVideoMode = m_bIsVideoModeValid ? m_uVideoMode : 0;
		return m_bIsVideoModeValid;
	}

	void Set(UINT videoMode)
	{
		m_bIsVideoModeValid = true;
		m_uVideoMode = videoMode;
	}

private:
	bool m_bIsVideoModeValid;
	uint32_t m_uVideoMode;

	static DebugVideoMode m_Instance;
};

DebugVideoMode DebugVideoMode::m_Instance;

bool DebugGetVideoMode(UINT* pVideoMode)
{
	return DebugVideoMode::Instance().Get(pVideoMode);
}


// File _______________________________________________________________________

size_t _GetFileSize( FILE *hFile )
{
	fseek( hFile, 0, SEEK_END );
	size_t nFileBytes = ftell( hFile );
	fseek( hFile, 0, SEEK_SET );

	return nFileBytes;
}



// Bookmarks __________________________________________________________________


//===========================================================================
bool _Bookmark_Add( const int iBookmark, const WORD nAddress )
{
	if (iBookmark < MAX_BOOKMARKS)
	{
	//	g_aBookmarks.push_back( nAddress );
//		g_aBookmarks.at( iBookmark ) = nAddress;
		g_aBookmarks[ iBookmark ].nAddress = nAddress;
		g_aBookmarks[ iBookmark ].bSet     = true;
		g_nBookmarks++;
		return true;
	}
	
	return false;
}


//===========================================================================
bool _Bookmark_Del( const WORD nAddress )
{
	bool bDeleted = false;

//	int nSize = g_aBookmarks.size();
	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].nAddress == nAddress)
		{
//			g_aBookmarks.at( iBookmark ) = NO_6502_TARGET;
			g_aBookmarks[ iBookmark ].bSet = false;
			g_nBookmarks--;
			bDeleted = true;
		}
	}
	return bDeleted;
}

// Returns:
//     0   if address does not have a bookmark set
//     N+1 if there is an existing bookmark that has this address
int Bookmark_Find( const WORD nAddress )
{
	// Ugh, linear search
//	int nSize = g_aBookmarks.size();
	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].nAddress == nAddress)
		{
			if (g_aBookmarks[ iBookmark ].bSet)
				return iBookmark + 1;
		}
	}
	return 0;
}


//===========================================================================
bool _Bookmark_Get( const int iBookmark, WORD & nAddress )
{
//	int nSize = g_aBookmarks.size();
	if (iBookmark >= MAX_BOOKMARKS)
		return false;

	if (g_aBookmarks[ iBookmark ].bSet)
	{
		nAddress = g_aBookmarks[ iBookmark ].nAddress;
		return true;
	}

	return false;
}


//===========================================================================
void _Bookmark_Reset()
{
//	g_aBookmarks.reserve( MAX_BOOKMARKS );
//	g_aBookmarks.insert( g_aBookma	int iBookmark = 0;
	int iBookmark = 0;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		g_aBookmarks[ iBookmark ].bSet = false;
	}

	g_nBookmarks = 0;
}


//===========================================================================
int _Bookmark_Size()
{
	g_nBookmarks = 0;

	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].bSet)
			g_nBookmarks++;
	}

	return g_nBookmarks;
}

//===========================================================================
Update_t CmdBookmark (int nArgs)
{
	return CmdBookmarkAdd( nArgs );
}

//===========================================================================
Update_t CmdBookmarkAdd (int nArgs )
{
	// BMA address
	// BMA # address	; where # is [0...9]
	if (! nArgs)
	{
		return CmdBookmarkList( 0 );
	}

	int iBookmark = 0;

	int iArg = 1;
	if (nArgs > 1)
	{
		iBookmark = g_aArgs[ iArg ].nValue;
		iArg++;
	}
	else
	{
		while ((iBookmark < MAX_BOOKMARKS) && g_aBookmarks[iBookmark].bSet)
			iBookmark++;
	}

	WORD nAddress = g_aArgs[ iArg ].nValue;

	if (iBookmark >= MAX_BOOKMARKS)
	{
		ConsoleDisplayPushFormat( "All bookmarks are currently in use.  (Max: %d)", MAX_BOOKMARKS );
		return ConsoleUpdate();
	}

	if (Bookmark_Find( nAddress ))
		_Bookmark_Del( nAddress );

	bool bAdded = _Bookmark_Add( iBookmark, nAddress );
	if (!bAdded)
		return Help_Arg_1( CMD_BOOKMARK_ADD );

	return UPDATE_DISASM | ConsoleUpdate();
}

//===========================================================================
Update_t CmdBookmarkClear (int nArgs)
{
	int iBookmark = 0;

	int iArg;
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (! strcmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			_Bookmark_Reset();
			break;
		}

		iBookmark = g_aArgs[ iArg ].nValue;
		if (g_aBookmarks[ iBookmark ].bSet)
			_Bookmark_Del(g_aBookmarks[ iBookmark ].nAddress);
	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdBookmarkGoto ( int nArgs )
{
	if (! nArgs)
		return Help_Arg_1( CMD_BOOKMARK_GOTO );

	int iBookmark = g_aArgs[ 1 ].nValue;

	WORD nAddress;
	if (_Bookmark_Get( iBookmark, nAddress ))
	{
		g_nDisasmCurAddress = nAddress;
		g_nDisasmCurLine = 0;
		DisasmCalcTopBotAddress();
	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdBookmarkList (int nArgs)
{
	if (! g_nBookmarks)
	{
		ConsoleBufferPushFormat( "  There are no current bookmarks.  (Max: %d)", MAX_BOOKMARKS );
	}
	else
	{
		_BWZ_ListAll( g_aBookmarks, MAX_BOOKMARKS );
	}
	return ConsoleUpdate();
}


//===========================================================================
Update_t CmdBookmarkLoad (int nArgs)
{
	if (nArgs == 1)
	{
//		strcpy( sMiniFileName, pFileName );
	//	strcat( sMiniFileName, ".aws" ); // HACK: MAGIC STRING

//		strcpy(sFileName, g_sCurrentDir); // 
//		strcat(sFileName, sMiniFileName);
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdBookmarkSave (int nArgs)
{
	g_ConfigState.Reset();

	ConfigSave_PrepareHeader( PARAM_CAT_BOOKMARKS, CMD_BOOKMARK_CLEAR );

	int iBookmark = 0;
	while (iBookmark < MAX_BOOKMARKS)
	{
		if (g_aBookmarks[ iBookmark ].bSet)
		{
			g_ConfigState.PushLineFormat( "%s %x %04X\n"
				, g_aCommands[ CMD_BOOKMARK_ADD ].m_sName
				, iBookmark
				, g_aBookmarks[ iBookmark ].nAddress
			);
		}
		iBookmark++;
	}

	if (nArgs)
	{
		if (! (g_aArgs[ 1 ].bType & TYPE_QUOTED_2))
			return Help_Arg_1( CMD_BOOKMARK_SAVE );

		if (ConfigSave_BufferToDisk( g_aArgs[ 1 ].sArg, CONFIG_SAVE_FILE_CREATE ))
		{
			ConsoleBufferPush(  "Saved."  );
			return ConsoleUpdate();
		}
	}

	return UPDATE_CONSOLE_DISPLAY;
}



// Benchmark ______________________________________________________________________________________

//===========================================================================
Update_t CmdBenchmark (int nArgs)
{
	if (g_bBenchmarking)
		CmdBenchmarkStart(0);
	else
		CmdBenchmarkStop(0);
	
	return UPDATE_ALL; // TODO/FIXME Verify
}

//===========================================================================
Update_t CmdBenchmarkStart (int nArgs)
{
	CpuSetupBenchmark();
	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();
	g_bBenchmarking = true;
	return UPDATE_ALL; // 1;
}

//===========================================================================
Update_t CmdBenchmarkStop (int nArgs)
{
	g_bBenchmarking = false;
	DebugEnd();
	
	GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_DISK_STATUS);
	GetFrame().VideoRedrawScreen();
	uint32_t currtime = GetTickCount();
	while ((extbench = GetTickCount()) != currtime)
		; // intentional busy-waiting
	KeybQueueKeypress(' ' ,ASCII);

	return UPDATE_ALL; // 0;
}

//===========================================================================
Update_t CmdProfile (int nArgs)
{
	if (! nArgs)
	{
		strncpy_s( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_RESET ].m_sName, _TRUNCATE );
		nArgs = 1;
	}

	if (nArgs == 1)
	{
		int iParam;
		int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

		if (! nFound)
			goto _Help;

		if (iParam == PARAM_RESET)
		{
			ProfileReset();
			g_bProfiling = 1;
			ConsoleBufferPush( " Resetting profile data." );
		}
		else
		{
			if ((iParam != PARAM_SAVE) && (iParam != PARAM_LIST))
				goto _Help;

			bool bExport = true;
			if (iParam == PARAM_LIST)
				bExport = false;

			// .csv (Comma Seperated Value)
//			ProfileFormat( bExport, bExport ? PROFILE_FORMAT_COMMA : PROFILE_FORMAT_SPACE );
			// .txt (Tab Seperated Value)
			ProfileFormat( bExport, bExport ? PROFILE_FORMAT_TAB : PROFILE_FORMAT_SPACE );

			// Dump to console
			if (iParam == PARAM_LIST)
			{
				const int nLine = g_nProfileLine;

				for ( int iLine = 0; iLine < nLine; iLine++ )
				{
					ProfileLine_t prfline = ProfileLinePeek( iLine );
					if (prfline.buf)
					{
						char sText[ CONSOLE_WIDTH ];
						TextConvertTabsToSpaces( sText, prfline.buf, CONSOLE_WIDTH, 4 );
						// ConsoleBufferPush( sText );
						ConsolePrint( sText );
					}
				}
			}
		
			if (iParam == PARAM_SAVE)
			{
				if (ProfileSave())
				{
					ConsoleBufferPushFormat( " Saved: %s", g_FileNameProfile.c_str() );
				}
				else
					ConsoleBufferPush( " ERROR: Couldn't save file. (In use?)" );
			}
		}
	}
	else
		goto _Help;

	return ConsoleUpdate(); // UPDATE_CONSOLE_DISPLAY;

_Help:
	return Help_Arg_1( CMD_PROFILE );
}


// Breakpoints ____________________________________________________________________________________

//===========================================================================
Update_t _BP_InfoNone ()
{
		ConsolePrintFormat( "There are no " CHC_ARG_SEP "(" CHC_CATEGORY "PC" CHC_ARG_SEP ")" CHC_DEFAULT " Breakpoints defined.");
		return ConsoleDisplayError( "" );
}

//===========================================================================

// iOpcodeType = AM_IMPLIED (BRK), AM_1, AM_2, AM_3
static bool IsDebugBreakOnInvalid (int iOpcodeType)
{
	return ((g_nDebugBreakOnInvalid >> iOpcodeType) & 1) ? true : false;
}

// iOpcodeType = AM_IMPLIED (BRK), AM_1, AM_2, AM_3
static void SetDebugBreakOnInvalid ( int iOpcodeType, int nValue )
{
	if (iOpcodeType <= AM_3)
	{
		g_nDebugBreakOnInvalid &= ~ (          1  << iOpcodeType);
		g_nDebugBreakOnInvalid |=   ((nValue & 1) << iOpcodeType);
	}
}

Update_t CmdBreakInvalid (int nArgs) // Breakpoint IFF Full-speed!
{
	if (nArgs > 2) // || (nArgs == 0))
		return HelpLastCommand();

	int iType = AM_IMPLIED; // default to BRK
	int nActive = 0;

	if (nArgs == 0)
	{
		nArgs = 1;
		g_aArgs[ 1 ].nValue = AM_IMPLIED;
		g_aArgs[ 1 ].sArg[0] = 0;
	}

	iType = g_aArgs[ 1 ].nValue;

	// Cases:
	// 0.  CMD            // display
	// 1a. CMD #          // display
	// 1b. CMD ON | OFF   // set
	// 1c. CMD ?          // error
	// 2a. CMD # ON | OFF // set
	// 2b. CMD ALL ON | OFF // set all
	// 2c. CMD # ?        // error
	bool bValidParam = true;

	int iParamArg = nArgs;	// last arg is the 'ON' / 'OFF' param
	int iParam;
	int nFound = FindParam( g_aArgs[ iParamArg ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

	if (nFound)
	{
		if (iParam == PARAM_ON)
			nActive = 1;
		else
		if (iParam == PARAM_OFF)
			nActive = 0;
		else
		{
			bValidParam = false;
			nFound = 0;
		}
	}
	else
		bValidParam = false;

	if (nArgs == 1)
	{
		if (! nFound)
		{
			if ((iType < AM_IMPLIED) || (iType > AM_3))
				goto _Help;

			if ( IsDebugBreakOnInvalid( iType ) ) 
				iParam = PARAM_ON;
			else
				iParam = PARAM_OFF;
		}
		else // case 1b
		{
			SetDebugBreakOnInvalid( iType, nActive );
		}
		
		if (iType == 0)
			ConsoleBufferPushFormat( "Enter debugger on BRK opcode: %s", g_aParameters[ iParam ].m_sName );
		else
			ConsoleBufferPushFormat( "Enter debugger on INVALID %1X opcode: %s", iType, g_aParameters[ iParam ].m_sName );
		return ConsoleUpdate();
 	}
	else
	if (nArgs == 2)
	{
		int iParam1;
		if (FindParam(g_aArgs[1].sArg, MATCH_EXACT, iParam1, PARAM_ALL, PARAM_ALL)) // case 2b
		{
			for (iType = 0; iType <= AM_3; iType++)
				SetDebugBreakOnInvalid(iType, nActive);
			ConsoleBufferPushFormat("Enter debugger on BRK opcode and INVALID opcodes: %s", g_aParameters[iParam].m_sName);
			return ConsoleUpdate();
		}
		else if (! bValidParam) // case 2c
		{
			goto _Help;
		}
		else // case 2a
		{
			if ((iType < 0) || (iType > AM_3))
				goto _Help;

			SetDebugBreakOnInvalid( iType, nActive );

			if (iType == 0)
				ConsoleBufferPushFormat( "Enter debugger on BRK opcode: %s", g_aParameters[ iParam ].m_sName );
			else
				ConsoleBufferPushFormat( "Enter debugger on INVALID %1X opcode: %s", iType, g_aParameters[ iParam ].m_sName );
			return ConsoleUpdate();
		}
 	}

	return UPDATE_CONSOLE_DISPLAY;

_Help:
	return HelpLastCommand();
}


//===========================================================================
Update_t CmdBreakOpcode (int nArgs) // Breakpoint IFF Full-speed!
{
	if (nArgs > 1)
		return HelpLastCommand();

	char sAction[ CONSOLE_WIDTH ] = "Current"; // default to display

	if (nArgs == 1)
	{
		int iOpcode = g_aArgs[ 1] .nValue;
		g_iDebugBreakOnOpcode = iOpcode & 0xFF;

		strcpy( sAction, "Setting" );

		if (iOpcode >= NUM_OPCODES)
		{
			ConsoleBufferPushFormat( "Warning: clamping opcode: %02X", g_iDebugBreakOnOpcode );
			return ConsoleUpdate();
		}
	}

	if (g_iDebugBreakOnOpcode == 0)
		// Show what the current break opcode is
		ConsoleBufferPushFormat( "%s Break on Opcode: None"
			, sAction
		);
	else
		// Show what the current break opcode is
		ConsoleBufferPushFormat( "%s Break on Opcode: %02X %s"
			, sAction
			, g_iDebugBreakOnOpcode
			, g_aOpcodes65C02[ g_iDebugBreakOnOpcode ].sMnemonic
		);

	return ConsoleUpdate();
}


//===========================================================================
Update_t CmdBreakOnInterrupt (int nArgs)
{
	if (nArgs > 1)
		return HelpLastCommand();

	int iParamArg = nArgs;
	int iParam;
	int nFound = FindParam(g_aArgs[iParamArg].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END);

	int nActive = -1;
	if (nFound)
	{
		if (iParam == PARAM_ON)
			nActive = 1;
		else if (iParam == PARAM_OFF)
			nActive = 0;
	}

	if (nArgs == 1 && nActive == -1)
		return HelpLastCommand();

	char sAction[CONSOLE_WIDTH] = "Current"; // default to display

	if (nArgs == 1)
	{
		g_bDebugBreakOnInterrupt = (iParam == PARAM_ON) ? true : false;
		strcpy(sAction, "Setting");
	}

	ConsoleBufferPushFormat("%s Break on Interrupt: %s"
		, sAction
		, g_bDebugBreakOnInterrupt ? "Enabled" : "Disabled"
	);

	return ConsoleUpdate();
}


// bool bBP = g_nBreakpoints && CheckBreakpoint(nOffset,nOffset == regs.pc);
//===========================================================================
bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ )
{
	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t *pBP = &g_aBreakpoints[ iBreakpoint ];
		
		if ((pBP->nLength)
//			 && (pBP->bEnabled) // not bSet
			 && (nOffset >= pBP->nAddress) && (nOffset < (pBP->nAddress + pBP->nLength))) // [nAddress,nAddress+nLength]
		{
			bBreakpointActive_ = pBP->bSet;
			bBreakpointEnable_ = pBP->bEnabled;
			return true;
		}

//		if (g_aBreakpoints[iBreakpoint].nLength && g_aBreakpoints[iBreakpoint].bEnabled &&
//			(g_aBreakpoints[iBreakpoint].nAddress <= targetaddr) &&
//			(g_aBreakpoints[iBreakpoint].nAddress + g_aBreakpoints[iBreakpoint].nLength > targetaddr))
	}

	bBreakpointActive_ = false;
	bBreakpointEnable_ = false;

	return false;
}

// returns the hit type if the breakpoint stops
static BreakpointHit_t hitBreakpoint(Breakpoint_t * pBP, BreakpointHit_t eHitType)
{
	pBP->bHit = true;
	++pBP->nHitCount;
	return pBP->bStop ? eHitType : BP_HIT_NONE;
}


// Returns true if we should continue checking breakpoint details, else false
//===========================================================================
bool _BreakpointValid ( Breakpoint_t *pBP ) //, BreakpointSource_t iSrc )
{
	bool bStatus = false;

	if (! pBP->bEnabled)
		return bStatus;

//	if (pBP->eSource != iSrc)
//		return bStatus;

	if (! pBP->nLength)
		return bStatus;

	return true;
}

// Stepping
void ClearTempBreakpoints ()
{
	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];

		if (! _BreakpointValid( pBP ))
			continue;

		if (pBP->bHit && pBP->bTemp)
			_BWZ_RemoveOne(g_aBreakpoints, iBreakpoint, g_nBreakpoints);

		pBP->bHit = false;
	}
}

static void DebugEnterStepping()
{
	ClearTempBreakpoints();
	g_nAppMode = MODE_STEPPING;
	GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_DISK_STATUS);
}

//===========================================================================
bool _CheckBreakpointValue ( Breakpoint_t *pBP, int nVal )
{
	bool bStatus = false;

	int iCmp = pBP->eOperator;
	switch (iCmp)
	{
		case BP_OP_LESS_EQUAL   :
			if (nVal <= pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_LESS_THAN    :
			if (nVal < pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_EQUAL        : // Range is like C++ STL: [,)  (inclusive,not-inclusive)
			 if ((nVal >= pBP->nAddress) && ((UINT)nVal < (pBP->nAddress + pBP->nLength)))
			 	bStatus = true;
			break;
		case BP_OP_NOT_EQUAL    : // Range is: (,] (not-inclusive, inclusive)
			 if ((nVal < pBP->nAddress) || ((UINT)nVal >= (pBP->nAddress + pBP->nLength)))
			 	bStatus = true;
			break;
		case BP_OP_GREATER_THAN :
			if (nVal > pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_GREATER_EQUAL:
			if (nVal >= pBP->nAddress)
				bStatus = true;
			break;
		default:
			break;
	}

	return bStatus;
}

//===========================================================================
bool _CheckBreakpointRange (Breakpoint_t* pBP, int nVal, int nSize)
{
	bool bStatus = false;

	int iCmp = pBP->eOperator;
	switch (iCmp)
	{
	case BP_OP_EQUAL: // Range is like C++ STL: [,)  (inclusive,not-inclusive)
		if ( ((nVal >= pBP->nAddress) && ((UINT)nVal < (pBP->nAddress + pBP->nLength))) ||
			 ((pBP->nAddress >= nVal) && (pBP->nAddress < ((UINT)nVal + nSize))) )
			bStatus = true;
		break;
	default:
		_ASSERT(0);
		break;
	}

	return bStatus;
}

//===========================================================================

static void DebuggerBreakOnDma (WORD nAddress, WORD nSize, bool isDmaToMemory, int iBreakpoint);

bool DebuggerCheckMemBreakpoints (WORD nAddress, WORD nSize, bool isDmaToMemory)
{
	// NB. Caller handles when (addr+size) wraps on 64K

	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t* pBP = &g_aBreakpoints[iBreakpoint];
		if (_BreakpointValid(pBP))
		{
			if (pBP->eSource == BP_SRC_MEM_RW || (pBP->eSource == BP_SRC_MEM_READ_ONLY && !isDmaToMemory) || (pBP->eSource == BP_SRC_MEM_WRITE_ONLY && isDmaToMemory))
			{
				if (_CheckBreakpointRange(pBP, nAddress, nSize))
				{
					DebuggerBreakOnDma(nAddress, nSize, isDmaToMemory, iBreakpoint);
					return true;
				}
			}
		}
	}

	return false;
}

//===========================================================================
int CheckBreakpointsIO ()
{
	const int NUM_TARGETS = 3;

	int aTarget[ NUM_TARGETS ] =
	{
		NO_6502_TARGET,
		NO_6502_TARGET,
		NO_6502_TARGET
	};
	int  nBytes;
	int  bBreakpointHit = 0;

	int  iTarget;
	int  nAddress;

	// bIncludeNextOpcodeAddress == false:
	// . JSR addr16: ignore addr16 as a target
	// . BRK/RTS/RTI: ignore return (or vector) addr16 as a target
	_6502_GetTargets( regs.pc, &aTarget[0], &aTarget[1], &aTarget[2], &nBytes, true, false );

	if (nBytes)
	{
		for (iTarget = 0; iTarget < NUM_TARGETS; iTarget++ )
		{
			nAddress = aTarget[ iTarget ];
			if (nAddress != NO_6502_TARGET)
			{
				for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
				{
					Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
					if (_BreakpointValid( pBP ))
					{
						if (pBP->eSource == BP_SRC_MEM_RW || pBP->eSource == BP_SRC_MEM_READ_ONLY || pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
						{
							if (_CheckBreakpointValue( pBP, nAddress ))
							{
								g_uBreakMemoryAddress = (WORD) nAddress;
								BYTE opcode = ReadByteFromMemory(regs.pc);

								if (pBP->eSource == BP_SRC_MEM_RW)
								{
									bBreakpointHit |= hitBreakpoint(pBP, BP_HIT_MEM);
								}
								else if (pBP->eSource == BP_SRC_MEM_READ_ONLY)
								{
									if (g_aOpcodes[opcode].nMemoryAccess & (MEM_RI|MEM_R))
									{
										bBreakpointHit |= hitBreakpoint(pBP, BP_HIT_MEMR);
									}
								}
								else if (pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
								{
									if (g_aOpcodes[opcode].nMemoryAccess & (MEM_WI|MEM_W))
									{
										bBreakpointHit |= hitBreakpoint(pBP, BP_HIT_MEMW);
									}
								}
								else
								{
									_ASSERT(0);
								}
							}
						}
					}
				}
			}
		}
	}
	return bBreakpointHit;
}

// Returns true if a register breakpoint is triggered
//===========================================================================
int CheckBreakpointsReg ()
{
	g_pDebugBreakpointHit = nullptr;

	int iAnyBreakpointHit = 0;

	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];

		if (! _BreakpointValid( pBP ))
			continue;

		bool bBreakpointHit = 0;

		switch (pBP->eSource)
		{
			case BP_SRC_REG_PC:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.pc );
				break;
			case BP_SRC_REG_A:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.a );
				break;
			case BP_SRC_REG_X:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.x );
				break;
			case BP_SRC_REG_Y:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.y );
				break;
			case BP_SRC_REG_P:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.ps );
				break;
			case BP_SRC_REG_S:
				bBreakpointHit = _CheckBreakpointValue( pBP, regs.sp );
				break;
			default:
				break;
		}

		if (bBreakpointHit)
		{
			iAnyBreakpointHit = hitBreakpoint(pBP, BP_HIT_REG);
			g_pDebugBreakpointHit = pBP; // Save breakpoint so we can display which register triggered the breakpoint.
		}
	}

	return iAnyBreakpointHit;
}

// Returns true if a video breakpoint is triggered
//===========================================================================
int CheckBreakpointsVideo ()
{
	int iBreakpointHit = 0;

	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t* pBP = &g_aBreakpoints[iBreakpoint];

		if (!_BreakpointValid(pBP))
			continue;

		if (pBP->eSource != BP_SRC_VIDEO_SCANNER)
			continue;

		uint16_t vert = NTSC_GetVideoVertForDebugger();	// update video scanner's vert/horz position - needed for when in fullspeed (GH#1164)
		if (_CheckBreakpointValue(pBP, vert))
		{
			iBreakpointHit = hitBreakpoint(pBP, BP_HIT_VIDEO_POS);
			pBP->bEnabled = false;	// Disable, otherwise it'll trigger many times on this scan-line
		}
	}

	return iBreakpointHit;
}

//===========================================================================
static int CheckBreakpointsDmaToOrFromIOMemory (void)
{
	int res = g_DebugBreakOnDMAIO.isToOrFromMemory;
	g_DebugBreakOnDMAIO.isToOrFromMemory = 0;
	return res;
}

void DebuggerBreakOnDmaToOrFromIoMemory (WORD nAddress, bool isDmaToMemory)
{
	g_DebugBreakOnDMAIO.isToOrFromMemory = isDmaToMemory ? BP_DMA_TO_IO_MEM : BP_DMA_FROM_IO_MEM;
	g_DebugBreakOnDMAIO.memoryAddr = nAddress;
}

static int CheckBreakpointsDmaToOrFromMemory (int idx)
{
	if (idx == -1)
	{
		int res = 0;
		for (int i = 0; i < NUM_BREAK_ON_DMA; i++)
			res |= g_DebugBreakOnDMA[i].isToOrFromMemory;
		return res;
	}

	_ASSERT(idx < NUM_BREAK_ON_DMA);
	if (idx >= NUM_BREAK_ON_DMA)
		return 0;

	int res = g_DebugBreakOnDMA[idx].isToOrFromMemory;
	g_DebugBreakOnDMA[idx].isToOrFromMemory = 0;
	return res;
}

static void DebuggerBreakOnDma (WORD nAddress, WORD nSize, bool isDmaToMemory, int iBreakpoint)
{
	for (int i = 0; i < NUM_BREAK_ON_DMA; i++)
	{
		if (g_DebugBreakOnDMA[i].isToOrFromMemory != 0)
			continue;

		g_DebugBreakOnDMA[i].isToOrFromMemory = isDmaToMemory ? BP_DMA_TO_MEM : BP_DMA_FROM_MEM;
		g_DebugBreakOnDMA[i].memoryAddr = nAddress;
		g_DebugBreakOnDMA[i].memoryAddrEnd = nAddress + nSize - 1;
		g_DebugBreakOnDMA[i].BPid = iBreakpoint;
		return;
	}

	_ASSERT(0);
}

//===========================================================================
Update_t CmdBreakpoint (int nArgs)
{
	return CmdBreakpointAddPC( nArgs );
}


// smart breakpoint
//===========================================================================
Update_t CmdBreakpointAddSmart (int nArgs)
{
	unsigned int nAddress = g_aArgs[1].nValue;

	if (! nArgs)
	{
		nArgs = 1;
		g_aArgs[ nArgs ].nValue = g_nDisasmCurAddress;		
	}

	if ((nAddress >= APPLE_IO_BEGIN) && (nAddress <= APPLE_IO_END))
	{
		return CmdBreakpointAddIO( nArgs );
	}
	else
	{
		CmdBreakpointAddReg( nArgs );
		CmdBreakpointAddMem( nArgs );	
		return UPDATE_BREAKPOINTS;
	}
}


//===========================================================================
Update_t CmdBreakpointAddReg (int nArgs)
{
	if (! nArgs)
	{
		return Help_Arg_1( CMD_BREAKPOINT_ADD_REG );
	}

	BreakpointSource_t   iSrc = BP_SRC_REG_PC;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;

	bool bHaveSrc = false;
	bool bHaveCmp = false;

	int iParamSrc;
	int iParamCmp;

	int  nFound;

	int  iArg   = 0;
	while (iArg++ < nArgs)
	{
		char *sArg = g_aArgs[iArg].sArg;

		bHaveSrc = false;
		bHaveCmp = false;

		nFound = FindParam( sArg, MATCH_EXACT, iParamSrc, _PARAM_REGS_BEGIN, _PARAM_REGS_END ); 
		if (nFound)
		{
			switch (iParamSrc)
			{
				case PARAM_REG_A : iSrc = BP_SRC_REG_A ; bHaveSrc = true; break;
				case PARAM_FLAGS : iSrc = BP_SRC_REG_P ; bHaveSrc = true; break;
				case PARAM_REG_X : iSrc = BP_SRC_REG_X ; bHaveSrc = true; break;
				case PARAM_REG_Y : iSrc = BP_SRC_REG_Y ; bHaveSrc = true; break;
				case PARAM_REG_PC: iSrc = BP_SRC_REG_PC; bHaveSrc = true; break;
				case PARAM_REG_SP: iSrc = BP_SRC_REG_S ; bHaveSrc = true; break;
				default:
					break;
			}
		}

		nFound = FindParam( sArg, MATCH_EXACT, iParamCmp, _PARAM_BREAKPOINT_BEGIN, _PARAM_BREAKPOINT_END );
		if (nFound)
		{
			switch (iParamCmp)
			{
				case PARAM_BP_LESS_EQUAL   : iCmp = BP_OP_LESS_EQUAL   ; bHaveCmp = true; break;
				case PARAM_BP_LESS_THAN    : iCmp = BP_OP_LESS_THAN    ; bHaveCmp = true; break;
				case PARAM_BP_EQUAL        : iCmp = BP_OP_EQUAL        ; bHaveCmp = true; break;
				case PARAM_BP_NOT_EQUAL    : iCmp = BP_OP_NOT_EQUAL    ; bHaveCmp = true; break;
				case PARAM_BP_NOT_EQUAL_1  : iCmp = BP_OP_NOT_EQUAL    ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_THAN : iCmp = BP_OP_GREATER_THAN ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_EQUAL: iCmp = BP_OP_GREATER_EQUAL; bHaveCmp = true; break;
				default:
					break;
			}
		}

		if ((! bHaveSrc) && (! bHaveCmp)) // Inverted/Convoluted logic: didn't find BOTH this pass, so we must have already found them.
		{
			int dArgs = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (!dArgs)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_REG );
			}
			iArg += dArgs;
		}
	}

	return UPDATE_ALL;
}


//===========================================================================
bool _CmdBreakpointAddReg ( Breakpoint_t *pBP, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, WORD nAddress, int nLen, bool bIsTempBreakpoint )
{
	bool bStatus = false;

	if (pBP)
	{
		_ASSERT(nLen <= _6502_MEM_LEN);
		if (nLen > (int) _6502_MEM_LEN) nLen = (int) _6502_MEM_LEN;

		if (iSrc == BP_SRC_VIDEO_SCANNER)
		{
			if (nAddress >= NTSC_GetVideoLines())
				nAddress = NTSC_GetVideoLines() - 1;

			if ((nAddress + (UINT)nLen) >= NTSC_GetVideoLines())
				nLen = NTSC_GetVideoLines() - nAddress;
		}

		pBP->eSource   = iSrc;
		pBP->eOperator = iCmp;
		pBP->nAddress  = nAddress;
		pBP->nLength   = nLen;
		pBP->bSet      = true;
		pBP->bEnabled  = true;
		pBP->bTemp     = bIsTempBreakpoint;
		pBP->bStop     = true;
		pBP->bHit      = false;
		pBP->nHitCount = 0;
		bStatus = true;
	}

	return bStatus;
}


// @return Number of args processed
//===========================================================================
int _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, bool bIsTempBreakpoint )
{
	int dArg = 0;

	int iBreakpoint = 0;
	Breakpoint_t *pBP = & g_aBreakpoints[ iBreakpoint ];

	while ((iBreakpoint < MAX_BREAKPOINTS) && g_aBreakpoints[iBreakpoint].bSet) //g_aBreakpoints[iBreakpoint].nLength)
	{
		iBreakpoint++;
		pBP++;
	}

	if (iBreakpoint >= MAX_BREAKPOINTS)
	{
		ConsoleDisplayError("All Breakpoints slots are currently in use.");
		return dArg;
	}

	if (iArg <= nArg)
	{
#if DEBUG_VAL_2
		int nLen = g_aArgs[iArg].nVal2;
#endif
		WORD nAddress  = 0;
		WORD nAddress2 = 0;
		WORD nEnd      = 0;
		int  nLen      = 0;

		dArg = 1;
		RangeType_t eRange = Range_Get( nAddress, nAddress2, iArg);
		if ((eRange == RANGE_HAS_END) ||
			(eRange == RANGE_HAS_LEN))
		{
			Range_CalcEndLen( eRange, nAddress, nAddress2, nEnd, nLen );
			dArg = 2;
		}

		if ( !nLen)
		{
			nLen = 1;
		}

		if (! _CmdBreakpointAddReg( pBP, iSrc, iCmp, nAddress, nLen, bIsTempBreakpoint ))
		{
			dArg = 0;
		}
		g_nBreakpoints++;
	}

	return dArg;
}


//===========================================================================
Update_t CmdBreakpointAddPC (int nArgs)
{
	BreakpointSource_t   iSrc = BP_SRC_REG_PC;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;

	if (!nArgs)
	{
		nArgs = 1;
//		g_aArgs[1].nValue = regs.pc;
		g_aArgs[1].nValue = g_nDisasmCurAddress;
	}

//	int iParamSrc;
	int iParamCmp;

	int  nFound = 0;

	int  iArg   = 0;
	while (iArg++ < nArgs)
	{
		char *sArg = g_aArgs[iArg].sArg;

		if (g_aArgs[iArg].bType & TYPE_OPERATOR)
		{
			nFound = FindParam( sArg, MATCH_EXACT, iParamCmp, _PARAM_BREAKPOINT_BEGIN, _PARAM_BREAKPOINT_END );
			if (nFound)
			{
				switch (iParamCmp)
				{
					case PARAM_BP_LESS_EQUAL   : iCmp = BP_OP_LESS_EQUAL   ; break;
					case PARAM_BP_LESS_THAN    : iCmp = BP_OP_LESS_THAN    ; break;
					case PARAM_BP_EQUAL        : iCmp = BP_OP_EQUAL        ; break;
					case PARAM_BP_NOT_EQUAL    : iCmp = BP_OP_NOT_EQUAL    ; break;
					case PARAM_BP_GREATER_THAN : iCmp = BP_OP_GREATER_THAN ; break;
					case PARAM_BP_GREATER_EQUAL: iCmp = BP_OP_GREATER_EQUAL; break;
					default:
						break;
				}
			}
		}
		else
		{
			int dArg = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (! dArg)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_PC );
			}
			iArg += dArg;
		}
	}
	
	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY; // 1;
}


//===========================================================================
Update_t CmdBreakpointAddIO (int nArgs)
{
	return CmdBreakpointAddMem( nArgs );
//	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdBreakpointAddMemA (int nArgs)
{
	return CmdBreakpointAddMem(nArgs);
}
//===========================================================================
Update_t CmdBreakpointAddMemR (int nArgs)
{
	return CmdBreakpointAddMem(nArgs, BP_SRC_MEM_READ_ONLY);
}
//===========================================================================
Update_t CmdBreakpointAddMemW (int nArgs)
{
	return CmdBreakpointAddMem(nArgs, BP_SRC_MEM_WRITE_ONLY);
}
//===========================================================================
Update_t CmdBreakpointAddMem (int nArgs, BreakpointSource_t bpSrc /*= BP_SRC_MEM_RW*/)
{
	BreakpointSource_t   iSrc = bpSrc;
	BreakpointOperator_t iCmp = BP_OP_EQUAL;

	int iArg = 0;
	
	while (iArg++ < nArgs)
	{
		if (g_aArgs[iArg].bType & TYPE_OPERATOR)
		{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_MEM );
		}
		else
		{
			int dArg = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (! dArg)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_MEM );
			}
			iArg += dArg;
		}
	}

	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdBreakpointAddVideo (int nArgs)
{
	BreakpointSource_t   iSrc = BP_SRC_VIDEO_SCANNER;
	BreakpointOperator_t iCmp = BP_OP_EQUAL;

	int iArg = 0;

	while (iArg++ < nArgs)
	{
		if (g_aArgs[iArg].bType & TYPE_OPERATOR)
		{
			return Help_Arg_1(CMD_BREAKPOINT_ADD_VIDEO);
		}
		else
		{
			int dArg = _CmdBreakpointAddCommonArg(iArg, nArgs, iSrc, iCmp);
			if (!dArg)
			{
				return Help_Arg_1(CMD_BREAKPOINT_ADD_VIDEO);
			}
			iArg += dArg;
		}
	}

	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdBreakpointClear (int nArgs)
{
	if (!g_nBreakpoints)
		return _BP_InfoNone();

	if (!nArgs)
	{
		_BWZ_RemoveAll( g_aBreakpoints, MAX_BREAKPOINTS, g_nBreakpoints );
	}
	else
	{
		_BWZ_ClearViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, g_nBreakpoints );
	}

	return UPDATE_DISASM | UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdBreakpointDisable (int nArgs)
{
	if (! g_nBreakpoints)
		return _BP_InfoNone();

	if (! nArgs)
		return Help_Arg_1( CMD_BREAKPOINT_DISABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, false );

	return UPDATE_BREAKPOINTS;
}

//===========================================================================
Update_t CmdBreakpointEdit (int nArgs)
{
	return (UPDATE_DISASM | UPDATE_BREAKPOINTS);
}


//===========================================================================
Update_t CmdBreakpointEnable (int nArgs) {

	if (! g_nBreakpoints)
		return _BP_InfoNone();

	if (! nArgs)
		return Help_Arg_1( CMD_BREAKPOINT_ENABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, true );

	return UPDATE_BREAKPOINTS;
}

// bpchange # <[E e T t S s]>
Update_t CmdBreakpointChange (int nArgs)
{
	if (! g_nBreakpoints)
		return _BP_InfoNone();

	if (nArgs < 2)
		return Help_Arg_1( CMD_BREAKPOINT_CHANGE );

	const int iSlot = g_aArgs[1].nValue;
	if (iSlot >= 0 && iSlot < MAX_BREAKPOINTS && g_aBreakpoints[iSlot].bSet)
	{
		Breakpoint_t & bp = g_aBreakpoints[iSlot];
		int iParam;
		int iParamArg;

		for (iParamArg = 2; iParamArg <= nArgs; ++iParamArg)
		{
			int bFound = FindParam( g_aArgs[ iParamArg ].sArg, MATCH_EXACT, iParam, _PARAM_BP_CHANGE_BEGIN, _PARAM_BP_CHANGE_END, true );
			if (! bFound)
				return Help_Arg_1( CMD_BREAKPOINT_CHANGE );

			switch (iParam)
			{
				case PARAM_BP_CHANGE_ENABLE  : bp.bEnabled = true ; break;
				case PARAM_BP_CHANGE_DISABLE : bp.bEnabled = false; break;
				case PARAM_BP_CHANGE_TEMP_ON : bp.bTemp    = true ; break;
				case PARAM_BP_CHANGE_TEMP_OFF: bp.bTemp    = false; break;
				case PARAM_BP_CHANGE_STOP_ON : bp.bStop    = true ; break;
				case PARAM_BP_CHANGE_STOP_OFF: bp.bStop    = false; break;
			}
		}
	}

	return UPDATE_BREAKPOINTS;
}

// called by BreakpointsClear, WatchesClear, ZeroPagePointersClear
//===========================================================================
void _BWZ_ClearViaArgs ( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, int & nTotal )
{
	int iSlot = 0;

	// Clear specified breakpoints
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nValue;

		if (! strcmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			_BWZ_RemoveAll( aBreakWatchZero, nMax, nTotal );
			break;
		}
		else
		if ((iSlot >= 0) && (iSlot < nMax))
		{
			_BWZ_RemoveOne( aBreakWatchZero, iSlot, nTotal );
		}

		nArgs--;
	}
}

// called by BreakpointsEnable, WatchesEnable, ZeroPagePointersEnable
// called by BreakpointsDisable, WatchesDisable, ZeroPagePointersDisable
//===========================================================================
void _BWZ_EnableDisableViaArgs ( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, const bool bEnabled )
{
	int iSlot = 0;

	// Enable each breakpoint in the list
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nValue;

		if (! strcmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			for ( ; iSlot < nMax; iSlot++ )
			{
				aBreakWatchZero[ iSlot ].bEnabled = bEnabled;
			}
		}
		else
		if ((iSlot >= 0) && (iSlot < nMax))
		{
			aBreakWatchZero[ iSlot ].bEnabled = bEnabled;
		}

		nArgs--;
	}
}

//===========================================================================
void _BWZ_List ( const Breakpoint_t * aBreakWatchZero, const int iBWZ ) //, bool bZeroBased )
{
	static const char sEnabledFlags[] = "-E";
	static const char sStopFlags[]    = "-S";
	static const char sTempFlags[]    = "-T";
	static const char sHitFlags[]     = " *";

	std::string sAddressBuf;
	std::string const& sSymbol = GetSymbol(aBreakWatchZero[iBWZ].nAddress, 2, sAddressBuf);

	const char *aMemAccess[4] =
	{
		 "R  "
		,"W  "
		,"R/W"
		,"   "
	};

	int iBPM;
	switch (aBreakWatchZero[iBWZ].eSource)
	{
		case BP_SRC_MEM_READ_ONLY : iBPM = 0; break;
		case BP_SRC_MEM_WRITE_ONLY: iBPM = 1; break;
		case BP_SRC_MEM_RW        : iBPM = 2; break;
		default                   : iBPM = 3; break;
	}

	// ID On Stop Temp HitCounter  Addr Mem Symbol
	ConsolePrintFormat( "  #%X %c  %c    %c  %c   %08X " CHC_ADDRESS " %04X " CHC_INFO "%s" CHC_SYMBOL " %s",
//		(bZeroBased ? iBWZ + 1 : iBWZ),
		iBWZ,
		sEnabledFlags[ aBreakWatchZero[ iBWZ ].bEnabled ? 1 : 0 ],
		sStopFlags   [ aBreakWatchZero[ iBWZ ].bStop    ? 1 : 0 ],
		sTempFlags   [ aBreakWatchZero[ iBWZ ].bTemp    ? 1 : 0 ],
		sHitFlags    [ aBreakWatchZero[ iBWZ ].bHit     ? 1 : 0 ],
		               aBreakWatchZero[ iBWZ ].nHitCount,
		               aBreakWatchZero[ iBWZ ].nAddress,
		aMemAccess[ iBPM ],
		sSymbol.c_str()
	);
}

void _BWZ_ListAll ( const Breakpoint_t * aBreakWatchZero, const int nMax )
{
	ConsolePrintFormat( "  ID On Stop Temp HitCounter  Addr Mem Symbol" );

	int iBWZ = 0;
	while (iBWZ < nMax) // 
	{
		if (aBreakWatchZero[ iBWZ ].bSet)
		{
			_BWZ_List( aBreakWatchZero, iBWZ );
		}
		iBWZ++;
	}
}

//===========================================================================
void _BWZ_RemoveOne ( Breakpoint_t *aBreakWatchZero, const int iSlot, int & nTotal )
{
	if (aBreakWatchZero[iSlot].bSet)
	{
		aBreakWatchZero[ iSlot ].bSet     = false;
		aBreakWatchZero[ iSlot ].bEnabled = false;
		aBreakWatchZero[ iSlot ].nLength  = 0;
		nTotal--;
	}
}

//===========================================================================
void _BWZ_RemoveAll ( Breakpoint_t *aBreakWatchZero, const int nMax, int & nTotal )
{
	for ( int iSlot = 0; iSlot < nMax; iSlot++ )
	{
		_BWZ_RemoveOne( aBreakWatchZero, iSlot, nTotal );
	}
}

//===========================================================================
Update_t CmdBreakpointList (int nArgs)
{
//	ConsoleBufferPush( );
//	std::vector<int> vBreakpoints;
//	int iBreakpoint = MAX_BREAKPOINTS;
//	while (iBreakpoint--)
//	{
//		if (g_aBreakpoints[iBreakpoint].enabled)
//		{
//			vBreakpoints.push_back( g_aBreakpoints[iBreakpoint].address );
//		}
//	}
//	std::sort( vBreakpoints.begin(), vBreakpoints.end() );
//	iBreakpoint = vBreakPoints.size();

	if (! g_nBreakpoints)
	{
		ConsoleBufferPushFormat( "  There are no current breakpoints.  (Max: %d)", MAX_BREAKPOINTS );
	}
	else
	{	
		_BWZ_ListAll( g_aBreakpoints, MAX_BREAKPOINTS );
	}
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdBreakpointLoad (int nArgs)
{
	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdBreakpointSave (int nArgs)
{
	g_ConfigState.Reset();

	ConfigSave_PrepareHeader( PARAM_CAT_BREAKPOINTS, CMD_BREAKPOINT_CLEAR );

	int iBreakpoint = 0;
	while (iBreakpoint < MAX_BREAKPOINTS)
	{
		if (g_aBreakpoints[ iBreakpoint ].bSet)
		{
			g_ConfigState.PushLineFormat( "%s %x %04X,%04X\n"
				, g_aCommands[ CMD_BREAKPOINT_ADD_REG ].m_sName
				, iBreakpoint
				, g_aBreakpoints[ iBreakpoint ].nAddress
				, g_aBreakpoints[ iBreakpoint ].nLength
			);
		}
		if (! g_aBreakpoints[ iBreakpoint ].bEnabled)
		{
			g_ConfigState.PushLineFormat( "%s %x\n"
				, g_aCommands[ CMD_BREAKPOINT_DISABLE ].m_sName
				, iBreakpoint
			);
		}
		
		iBreakpoint++;
	}

	if (nArgs)
	{
		if (! (g_aArgs[ 1 ].bType & TYPE_QUOTED_2))
			return Help_Arg_1( CMD_BREAKPOINT_SAVE );

		if (ConfigSave_BufferToDisk( g_aArgs[ 1 ].sArg, CONFIG_SAVE_FILE_CREATE ))
		{
			ConsoleBufferPush(  "Saved."  );
			return ConsoleUpdate();
		}
	}

	return UPDATE_CONSOLE_DISPLAY;
}

// Assembler ______________________________________________________________________________________

//===========================================================================
Update_t _CmdAssemble ( WORD nAddress, int iArg, int nArgs )
{
	// if AlphaNumeric
	ArgToken_e iTokenSrc = NO_TOKEN;
	ParserFindToken( g_pConsoleInput, g_aTokens, NUM_TOKENS, &iTokenSrc );

	if (iTokenSrc == NO_TOKEN) // is TOKEN_ALPHANUMERIC
	if (g_pConsoleInput[0] != CHAR_SPACE)
	{
		// Symbol
		char *pSymbolName = g_aArgs[ iArg ].sArg; // pArg->sArg;
		SymbolUpdate( SYMBOLS_ASSEMBLY, pSymbolName, nAddress, false, true ); // bool bRemoveSymbol, bool bUpdateSymbol )

		iArg++;
	}	

	bool bStatus = Assemble( iArg, nArgs, nAddress );
	if ( bStatus)
	{
		// move disassembler to current address
		g_nDisasmCurAddress = g_nAssemblerAddress;
		WindowUpdateDisasmSize(); // calc cur line
		DisasmCalcTopBotAddress();
		return UPDATE_ALL;
	}
		
	return UPDATE_CONSOLE_DISPLAY; // UPDATE_NOTHING;
}


//===========================================================================
Update_t CmdAssemble (int nArgs)
{
	if (! g_bAssemblerOpcodesHashed)
	{
		AssemblerStartup();
		g_bAssemblerOpcodesHashed = true;
	}

	// 0 : A
	// 1 : A address
	// 2+: A address mnemonic...

	if (nArgs > 0)
		g_nAssemblerAddress = g_aArgs[1].nValue;

	// move disassembler window to current assembler address
	g_nDisasmCurAddress = g_nAssemblerAddress;       // (2)
	WindowUpdateDisasmSize(); // calc cur line
	DisasmCalcTopBotAddress();

	if (! nArgs)
	{
//		return Help_Arg_1( CMD_ASSEMBLE );

		// Start assembler, continue with last assembled address
		AssemblerOn();
		return UPDATE_CONSOLE_DISPLAY;
	}

	if (nArgs == 1)
	{
		int iArg = 1;
		
		// undocumented ASM *
		if ((! strcmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR        ].m_sName )) ||
			(! strcmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName )) )
		{
			_CmdAssembleHashDump();
		}

		AssemblerOn();
		return UPDATE_CONSOLE_DISPLAY;
	
//		return Help_Arg_1( CMD_ASSEMBLE );
	}

	if (nArgs > 1)
	{
		return _CmdAssemble( g_nAssemblerAddress, 2, nArgs ); // disasm, memory, watches, zeropage
	}

//		return Help_Arg_1( CMD_ASSEMBLE );
	// g_nAssemblerAddress; // g_aArgs[1].nValue;
//	return ConsoleUpdate();

	return UPDATE_CONSOLE_DISPLAY;
}

// CPU ____________________________________________________________________________________________
// CPU Step, Trace ________________________________________________________________________________

//===========================================================================
static Update_t CmdGo (int nArgs, const bool bFullSpeed)
{
	// G StopAddress [SkipAddress,Length]
	// Example:
	//	G C600 FA00,FFFF 
	// TODO: G addr1,len   addr3,len
	// TODO: G addr1:addr2 addr3:addr4

	const int kCmdGo = !bFullSpeed ? CMD_GO_NORMAL_SPEED : CMD_GO_FULL_SPEED;

	g_nDebugSteps = -1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = nArgs ? g_aArgs[1].nValue : -1;
	g_nDebugSkipStart = -1;
	g_nDebugSkipLen   = -1;

	if (nArgs > 4)
		return Help_Arg_1( kCmdGo );

	//     G StopAddress [SkipAddress,Len]
	// Old   1            2           2   
	//     G addr addr [, len]
	// New   1    2     3 4
	if (nArgs > 1)
	{
		int iArg = 2;
		g_nDebugSkipStart = g_aArgs[ iArg ].nValue;

#if DEBUG_VAL_2
		WORD nAddress     = g_aArgs[ iArg ].nVal2;
#endif
		int nLen = 0;
		int nEnd = 0;

		if (nArgs > 2)
		{
			if (g_aArgs[ iArg + 1 ].eToken == TOKEN_COMMA)
			{
				if (nArgs > 3)
				{
					nLen = g_aArgs[ iArg + 2 ].nValue;
					nEnd = g_nDebugSkipStart + nLen;
					if (nEnd > (int) _6502_MEM_END)
						nEnd = _6502_MEM_END + 1;
				}
				else
				{
					return Help_Arg_1( kCmdGo );
				}
			}
			else
			if (g_aArgs[ iArg+ 1 ].eToken == TOKEN_COLON)
			{
				nEnd = g_aArgs[ iArg + 2 ].nValue + 1;
			}
			else
				return Help_Arg_1( kCmdGo );
		}
		else
			return Help_Arg_1( kCmdGo );
		
		nLen = nEnd - g_nDebugSkipStart;
		if (nLen < 0)
			nLen = -nLen;
		g_nDebugSkipLen = nLen;
		g_nDebugSkipLen &= _6502_MEM_END;			

#if _DEBUG
	ConsoleBufferPushFormat( "Start: %04X,%04X  End: %04X  Len: %04X",
		g_nDebugSkipStart, g_nDebugSkipLen, nEnd, nLen );
	ConsoleBufferToDisplay();
#endif
	}

//	WORD nAddressSymbol = 0;
//	bool bFoundSymbol = FindAddressFromSymbol( g_aArgs[1].sArg, & nAddressSymbol );
//	if (bFoundSymbol)
//		g_nDebugStepUntil = nAddressSymbol;

//  if (!g_nDebugStepUntil)
//    g_nDebugStepUntil = GetAddress(g_aArgs[1].sArg);

	g_bDebuggerEatKey = true;

	g_bDebugFullSpeed = bFullSpeed;
	g_bLastGoCmdWasFullSpeed = bFullSpeed;
	g_bGoCmd_ReinitFlag = true;

	DebugEnterStepping();

	SoundCore_SetFade(FADE_IN);

	return UPDATE_CONSOLE_DISPLAY;
}

Update_t CmdGoNormalSpeed (int nArgs)
{
	return CmdGo(nArgs, false);
}

Update_t CmdGoFullSpeed (int nArgs)
{
	return CmdGo(nArgs, true);
}

//===========================================================================
Update_t CmdStepOver (int nArgs)
{
	// assert( g_nDisasmCurAddress == regs.pc );

//	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	WORD nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;

	while (nDebugSteps-- > 0)
	{
		BYTE nOpcode = ReadByteFromMemory(regs.pc);
		WORD nExpectedAddr = (regs.pc + 3) & _6502_MEM_END; // Wrap around 64K edge case when PC = $FFFD..$FFFF: 20 xx xx
	//	int eMode = g_aOpcodes[ nOpcode ].addrmode;
	//	int nByte = g_aOpmodes[eMode]._nBytes;
	//	if ((eMode ==  AM_A) && 

		CmdTrace(0);
		if (nOpcode == OPCODE_JSR)
		{
			/*
				Repro #2 Test when SP <= 0x01 before JSR and _6502_GetStackReturnAddress() fetch return address
				300:BA 86 FF A2 01 9A 20 0D 03 A6 FF 9A 60 A9 FF 20 A8 FC 60
				BPX 306
				MD1 100
				MD2 1E0

			        ORG $300
			        TSX         ; 300
			        STX $FF     ; 301
			        LDX #1      ; 303
			        TXS         ; 305
			        JSR DelayFF ; 306
			        LDX $FF     ; 309
			        TXS         ; 30B
			        RTS         ; 30C
			DelayFF LDA #$FF    ; 30D
			        JSR $FCA8   ; 30F
			        RTS         ; 312
			*/
			CmdStepOut(0);

			int nMaxSteps = 0xFFFFF; // GH #1194
			g_nDebugSteps = nMaxSteps;

			while (g_nDebugSteps != 0)
			{
				DebugContinueStepping(true);
			}

			// If the PC isn't at the expected address after the JSR print a diagnostic so the user knows the stack may be buggered up
			if (regs.pc != nExpectedAddr)
			{
				WORD nActualAddr  = _6502_GetStackReturnAddress();
				bool bValidAddr   = (nActualAddr == nExpectedAddr);
				int  nStackOffset = _6502_FindStackReturnAddress( nExpectedAddr ); // Trace stack to seee if our expected address is on it

				/*
				            ORG $300
				Main        JSR HugeWait
				            RTS
				HugeWait    LDY #$FF
				Loop        JSR Delay
				            DEY
				            BNE Loop
				            RTS
				Delay       LDA #$FF
				            JSR $FCA8
				            RTS

				Repro #1
				1. MSVC: Revert line to repro: int nMaxSteps = 0xFFFF;
				2. MSVC: Set BP on line above: (regs.pc != nExpectedAddr)
				3. AppleWin:
				   F7
				   300:A0 FF 20 09 03 88 D0 FA 60 A9 FF 20 A8 FC 60
				   BPX 30B
				   F7
				   CALL 768
				   <Ctrl>-<Space>
				4. MSVC:Change regs.sp to one of 3 cases:
				   Case   Addr On Stack   Top of Stack   Diagnostic   nStackOffset   R SP              Continue in emulator
				   0      No              No             ERROR        -1             regs.sp = 0x1F3
				   1      Yes             Yes            INFO          O             regs.sp = 0x1F2   R PC FCB3
				   2      Yes             No             WARN         +1             regs.sp = 0x1F1   R S  F1
				*/
				/**/ if (nStackOffset <  0) ConsolePrintFormat( CHC_ERROR   "ERROR" CHC_ARG_SEP ":" CHC_ERROR   " Didn't step over JSR! " CHC_ARG_SEP "(" CHC_DEFAULT "RTS "           CHC_ARG_SEP "$" CHC_ADDRESS "%04X" CHC_DEFAULT " not found!"                                                                CHC_ARG_SEP ")", nExpectedAddr                      ); // Case 0
				else if (nStackOffset == 0) ConsolePrintFormat( CHC_INFO    "INFO"  CHC_ARG_SEP ":" CHC_INFO    " Didn't step over JSR! " CHC_ARG_SEP "(" CHC_DEFAULT "RTS "           CHC_ARG_SEP "$" CHC_ADDRESS "%04X" CHC_DEFAULT " on top of stack."                                                          CHC_ARG_SEP ")", nExpectedAddr                      ); // Case 1
				else /*                  */ ConsolePrintFormat( CHC_WARNING "WARN"  CHC_ARG_SEP ":" CHC_WARNING " Didn't step over JSR! " CHC_ARG_SEP "(" CHC_DEFAULT "Stack has RTS " CHC_ARG_SEP "$" CHC_ADDRESS "%04X" CHC_DEFAULT " but needs fixup: " CHC_ARG_SEP "$" CHC_NUM_HEX "%02X" CHC_DEFAULT " bytes" CHC_ARG_SEP ")", nExpectedAddr, nStackOffset & 0xFF ); // Case 2

				ConsolePrintFormat( CHC_DEFAULT "  Please report '" CHC_SYMBOL "nMaxSteps" CHC_ARG_SEP " = " CHC_DEFAULT "0x" CHC_NUM_HEX "%04X" CHC_DEFAULT "' to:", nMaxSteps );
				ConsolePrintFormat( CHC_PATH    "  https://github.com/AppleWin/AppleWin/issues/1194"               );
				ConsoleUpdate();
			}
		}
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdStepOut (int nArgs)
{
	// TODO: "RET" should probably pop the Call stack
	// Also see: CmdCursorJumpRetAddr
	WORD nAddress = _6502_GetStackReturnAddress();

	nArgs = _Arg_1( nAddress );
	g_aArgs[1].sArg[0] = 0;
	CmdGo( 1, true );

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdTrace (int nArgs)
{
	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;

	DebugEnterStepping();
	DebugContinueStepping(true);

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceFile (int nArgs)
{
	if (g_hTraceFile)
	{
		fclose( g_hTraceFile );
		g_hTraceFile = NULL;

		ConsoleBufferPush( "Trace stopped." );
	}
	else
	{
		std::string sFileName;

		if (nArgs)
			sFileName = g_aArgs[1].sArg;
		else
			sFileName = g_sFileNameTrace;

		g_bTraceFileWithVideoScanner = (nArgs >= 2);

		const std::string sFilePath = g_sCurrentDir + sFileName;

		g_hTraceFile = fopen( sFilePath.c_str(), "wt" );

		if (g_hTraceFile)
		{
			const char* pTextHdr = g_bTraceFileWithVideoScanner ? "Trace (with video info) started: %s"
																: "Trace started: %s";
			ConsoleBufferPushFormat( pTextHdr, sFilePath.c_str() );
			g_bTraceHeader = true;
		}
		else
		{
			ConsoleBufferPushFormat( "Trace ERROR: %s", sFilePath.c_str() );
		}
	}

	ConsoleBufferToDisplay();

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceLine (int nArgs)
{
	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	g_nDebugStepCycles  = 1;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;

	DebugEnterStepping();
	DebugContinueStepping(true);

	return UPDATE_ALL; // TODO: Verify // 0
}




// Unassemble
//===========================================================================
Update_t CmdUnassemble (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_UNASSEMBLE );
  
	WORD nAddress = g_aArgs[1].nValue;
	g_nDisasmTopAddress = nAddress;

	DisasmCalcCurFromTopAddress();
	DisasmCalcBotFromTopAddress();

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdKey (int nArgs)
{
	KeybQueueKeypress(
		nArgs ? g_aArgs[1].nValue ? g_aArgs[1].nValue : g_aArgs[1].sArg[0] : ' ', ASCII); // FIXME!!!
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdIn (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_IN );
  
	WORD nAddress = g_aArgs[1].nValue;

	IORead[ (nAddress>>4) & 0xF ](regs.pc, nAddress & 0xFF, 0, 0, 0); // g_aArgs[1].nValue 

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
}


//===========================================================================
Update_t CmdJSR (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_JSR );

	WORD nAddress = g_aArgs[1].nValue & _6502_MEM_END;

	// Push PC onto stack
	WriteByteToMemory(regs.sp, ((regs.pc >> 8) & 0xFF));
	regs.sp--;
	WriteByteToMemory(regs.sp, ((regs.pc >> 0) - 1) & 0xFF);
	regs.sp--;

	// Jump to new address
	regs.pc = nAddress;

	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdNOP (int nArgs)
{
	int iOpcode;
	int iOpmode;
	int nOpbytes;

	_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );

	while (nOpbytes--)
	{
		WriteByteToMemory(regs.pc + nOpbytes, 0xEA);
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdOut (int nArgs)
{
//  if ((!nArgs) ||
//      ((g_aArgs[1].sArg[0] != '0') && (!g_aArgs[1].nValue) && (!GetAddress(g_aArgs[1].sArg))))
//     return DisplayHelp(CmdInput);

	if (!nArgs)
		Help_Arg_1( CMD_OUT );

	WORD nAddress = g_aArgs[1].nValue;

	IOWrite[ (nAddress>>4) & 0xF ] (regs.pc, nAddress & 0xFF, 1, g_aArgs[2].nValue & 0xFF, 0);

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
}

//===========================================================================
Update_t CmdLBR(int nArgs)
{
	if (g_LBR == LBR_UNDEFINED)
		ConsolePrintFormat(" LBR not set yet. Hint: Run from the debugger via 'g' command.");
	else
		ConsolePrintFormat(" LBR = $%04X", g_LBR);
	return ConsoleUpdate();
}

// Color __________________________________________________________________________________________

void _ColorPrint ( int iColor, COLORREF nColor )
{
	int R = (nColor >>  0) & 0xFF;
	int G = (nColor >>  8) & 0xFF;
	int B = (nColor >> 16) & 0xFF;

	ConsoleBufferPushFormat( " Color %01X: %02X %02X %02X", iColor, R, G, B ); // TODO: print name of colors!
}

void _CmdColorGet ( const int iScheme, const int iColor )
{
	if (iColor < NUM_DEBUG_COLORS)
	{
//	COLORREF nColor = g_aColors[ iScheme ][ iColor ];
		DebugColors_e eColor = static_cast<DebugColors_e>( iColor );
		COLORREF nColor = DebuggerGetColor( eColor );
		_ColorPrint( iColor, nColor );
	}
	else
	{
		std::string sText = StrFormat( "Color: %d\nOut of range!", iColor );
		GetFrame().FrameMessageBox(sText.c_str(), "ERROR", MB_OK);
	}
}

//===========================================================================
Update_t CmdConfigColorMono (int nArgs)
{
	int iScheme = 0;
	
	if (g_iCommand == CMD_CONFIG_COLOR)
		iScheme = SCHEME_COLOR;
	if (g_iCommand == CMD_CONFIG_MONOCHROME)
		iScheme = SCHEME_MONO;
	if (g_iCommand == CMD_CONFIG_BW)
		iScheme = SCHEME_BW;

	if ((iScheme < 0) || (iScheme > NUM_COLOR_SCHEMES)) // sanity check
		iScheme = SCHEME_COLOR;

	if (! nArgs)
	{
		g_iColorScheme = iScheme;
		UpdateDisplay( UPDATE_BACKGROUND );
		return UPDATE_ALL;
	}

//	if ((nArgs != 1) && (nArgs != 4))
	if (nArgs > 4)
		return HelpLastCommand();

	int iColor = g_aArgs[ 1 ].nValue;
	if ((iColor < 0) || iColor >= NUM_DEBUG_COLORS)
		return HelpLastCommand();

	int iParam;
	int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

	if (nFound)
	{
		if (iParam == PARAM_RESET)
		{
			ConfigColorsReset();
			ConsoleBufferPush( " Resetting colors." );
		}
		else
		if (iParam == PARAM_SAVE)
		{
		}
		else
		if (iParam == PARAM_LOAD)
		{
		}
		else
			return HelpLastCommand();
	}
	else
	{
		if (nArgs == 1)
		{	// Dump Color
			_CmdColorGet( iScheme, iColor );
			return ConsoleUpdate();
		}
		else
		if (nArgs == 4)
		{	// Set Color
			int R = g_aArgs[2].nValue & 0xFF;
			int G = g_aArgs[3].nValue & 0xFF;
			int B = g_aArgs[4].nValue & 0xFF;
			COLORREF nColor = RGB(R,G,B);

			DebuggerSetColor( iScheme, iColor, nColor );
		}
		else
			return HelpLastCommand();
	}

	return UPDATE_ALL;
}

Update_t CmdConfigHColor (int nArgs)
{
	if ((nArgs != 1) && (nArgs != 4))
		return Help_Arg_1( g_iCommand );

	int iColor = g_aArgs[ 1 ].nValue;
	if ((iColor < 0) || iColor >= NUM_DEBUG_COLORS)
		return Help_Arg_1( g_iCommand );

	if (nArgs == 1)
	{	// Dump Color
//		_CmdColorGet( iScheme, iColor );
// TODO/FIXME: must export AW_Video.cpp: static LPBITMAPINFO  framebufferinfo;
//		COLORREF nColor = g_aColors[ iScheme ][ iColor ];
//		_ColorPrint( iColor, nColor );
		return ConsoleUpdate();
	}
	else
	{	// Set Color
//		DebuggerSetColor( iScheme, iColor );
		return UPDATE_ALL;
	}
}

// Config _________________________________________________________________________________________


//===========================================================================
Update_t CmdConfigLoad (int nArgs)
{
	// TODO: CmdConfigRun( gaFileNameConfig )
	
//	char sFileNameConfig[ MAX_PATH ];
	if (! nArgs)
	{

	}

//	gDebugConfigName
	// DEBUGLOAD file // load debugger setting
	return UPDATE_ALL;
}


//===========================================================================
bool ConfigSave_BufferToDisk ( const char *pFileName, ConfigSave_t eConfigSave )
{
	bool bStatus = false;

	char sModeCreate[] = "w+t";
	char sModeAppend[] = "a+t";
	char *pMode = NULL;
	if (eConfigSave == CONFIG_SAVE_FILE_CREATE)
		pMode = sModeCreate;
	else
	if (eConfigSave == CONFIG_SAVE_FILE_APPEND)
		pMode = sModeAppend;

	const std::string sFileName = g_sCurrentDir + pFileName; // TODO: g_sDebugDir

	FILE *hFile = fopen( pFileName, pMode );

	if (hFile)
	{
		const int nLine = g_ConfigState.GetNumLines();
		
		for ( int iLine = 0; iLine < nLine; iLine++ )
		{
			const char *pText = g_ConfigState.GetLine( iLine );
			if ( pText )
			{
				fputs( pText, hFile );
			}
		}
		
		fclose( hFile );
		bStatus = true;
	}

	return bStatus;
}


//===========================================================================
void ConfigSave_PrepareHeader ( const Parameters_e eCategory, const Commands_e eCommandClear )
{
	g_ConfigState.PushLineFormat( "%s %s = %s\n"
		, g_aTokens[ TOKEN_COMMENT_EOL  ].sToken
		, g_aParameters[ PARAM_CATEGORY ].m_sName
		, g_aParameters[ eCategory ].m_sName
		);

	g_ConfigState.PushLineFormat( "%s %s\n"
		, g_aCommands[ eCommandClear ].m_sName
		, g_aParameters[ PARAM_WILDSTAR ].m_sName
	);
}


// Save Debugger Settings
//===========================================================================
Update_t CmdConfigSave (int nArgs)
{
	const std::string sFilename = g_sProgramDir + g_sFileNameConfig;

/*
	HANDLE hFile = CreateFile( sfilename,
		GENERIC_WRITE,
		0,
		(LPSECURITY_ATTRIBUTES)NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		void *pSrc;
		int   nLen;
		uint32_t nPut;

	// FIXME: Should be saving in Text format, not binary!

		int nVersion = CURRENT_VERSION;
		pSrc = (void *) &nVersion;
		nLen = sizeof( nVersion );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		pSrc = (void *) & g_aColorPalette;
		nLen = sizeof( g_aColorPalette );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		pSrc = (void *) & g_aColorIndex;
		nLen = sizeof( g_aColorIndex );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		CloseHandle( hFile );
	}
*/
		// Bookmarks
		CmdBookmarkSave( 0 );

		// Breakpoints
		CmdBreakpointSave( 0 );
		
		// Watches
		CmdWatchSave( 0 );

		// Zeropage pointers
		CmdZeroPageSave( 0 );

		// Color Palete

		// Color Index
//		CmdColorSave( 0 );

		// UserSymbol

		// History

	return UPDATE_CONSOLE_DISPLAY;
}


// Config - Disasm ________________________________________________________________________________

//===========================================================================
Update_t CmdConfigDisasm (int nArgs)
{
	int iParam = 0;

	bool bDisplayCurrentSettings = false;

//	if (! strcmp( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ))
	if (! nArgs)
	{
		bDisplayCurrentSettings = true;
		nArgs = PARAM_CONFIG_NUM;
	}
	else
	{
		if (nArgs > 2)
			return Help_Arg_1( CMD_CONFIG_DISASM );
	}

	for (int iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (bDisplayCurrentSettings)
			iParam = _PARAM_CONFIG_BEGIN + iArg - 1;
		else
		if (FindParam( g_aArgs[iArg].sArg, MATCH_FUZZY, iParam ))
		{
		}

			switch (iParam)
			{
				case PARAM_CONFIG_BRANCH:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_iConfigDisasmBranchType = g_aArgs[ iArg ].nValue;
						if (g_iConfigDisasmBranchType < 0)
							g_iConfigDisasmBranchType = 0;
						if (g_iConfigDisasmBranchType >= NUM_DISASM_BRANCH_TYPES)
							g_iConfigDisasmBranchType = NUM_DISASM_BRANCH_TYPES - 1;

					}
					else // show current setting
					{
						ConsoleBufferPushFormat( "Branch Type: %d", g_iConfigDisasmBranchType );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_CLICK: // GH#462
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigDisasmClick = (g_aArgs[ iArg ].nValue) & 7; // MAGIC NUMBER
					}
//					else // Always show current setting -- TODO: Fix remaining disasm to show current setting when set
					{
						const char *aClickKey[8] =
						{
							 ""                 // 0
							,"Alt "             // 1
							,"Ctrl "            // 2
							,"Alt+Ctrl "        // 3
							,"Shift "           // 4
							,"Shift+Alt "       // 5
							,"Shift+Ctrl "      // 6
							,"Shift+Ctarl+Alt " // 7
						};
						ConsoleBufferPushFormat( "Click: %d = %sLeft click", g_bConfigDisasmClick, aClickKey[ g_bConfigDisasmClick & 7 ] );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_COLON:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{					
						iArg++;
						g_bConfigDisasmAddressColon = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else // show current setting
					{
						int iState = g_bConfigDisasmAddressColon ? PARAM_ON : PARAM_OFF;
						ConsoleBufferPushFormat( "Colon: %s", g_aParameters[ iState ].m_sName );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_OPCODE:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigDisasmOpcodesView = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigDisasmOpcodesView ? PARAM_ON : PARAM_OFF;
						ConsoleBufferPushFormat( "Opcodes: %s", g_aParameters[ iState ].m_sName );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_POINTER:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigInfoTargetPointer = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigInfoTargetPointer ? PARAM_ON : PARAM_OFF;
						ConsoleBufferPushFormat( "Info Target Pointer: %s", g_aParameters[ iState ].m_sName );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_SPACES:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigDisasmOpcodeSpaces = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigDisasmOpcodeSpaces ? PARAM_ON : PARAM_OFF;
						ConsoleBufferPushFormat( "Opcode spaces: %s", g_aParameters[ iState ].m_sName );
						ConsoleBufferToDisplay();
					}
					break;
					
				case PARAM_CONFIG_TARGET:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{					
						iArg++;
						g_iConfigDisasmTargets = g_aArgs[ iArg ].nValue;
						if (g_iConfigDisasmTargets < 0)
							g_iConfigDisasmTargets = 0;
						if (g_iConfigDisasmTargets >= NUM_DISASM_TARGET_TYPES)
							g_iConfigDisasmTargets = NUM_DISASM_TARGET_TYPES - 1;
					}
					else // show current setting
					{
						ConsoleBufferPushFormat( "Target: %d", g_iConfigDisasmTargets );
						ConsoleBufferToDisplay();
					}
					break;

				default:
					return Help_Arg_1( CMD_CONFIG_DISASM ); // CMD_CONFIG_DISASM_OPCODE );
			}
//		}
//		else
//			return Help_Arg_1( CMD_CONFIG_DISASM );
	}
	return UPDATE_CONSOLE_DISPLAY | UPDATE_DISASM;
}

// Cursor _________________________________________________________________________________________

//===========================================================================
Update_t CmdCursorFollowTarget ( int nArgs )
{
	WORD nAddress = 0;
	if (_6502_GetTargetAddress( g_nDisasmCurAddress, nAddress ))
	{
		g_nDisasmCurAddress = nAddress;

		if (CURSOR_ALIGN_CENTER == nArgs)
		{
			WindowUpdateDisasmSize();
		}
		else
		if (CURSOR_ALIGN_TOP == nArgs)
		{
			g_nDisasmCurLine = 0;
		}
		DisasmCalcTopBotAddress();
	}

	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdCursorLineDown (int nArgs)
{
	int iOpmode; 
	int nOpbytes;
	_6502_GetOpmodeOpbyte( g_nDisasmCurAddress, iOpmode, nOpbytes ); // g_nDisasmTopAddress

	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveDownAligned( WINDOW_DATA_BYTES_PER_LINE );
		DisasmCalcTopBotAddress();
	}
	else
	if (nArgs) // scroll down by 'n' bytes
	{
		nOpbytes = nArgs; // HACKL g_aArgs[1].val

		g_nDisasmTopAddress += nOpbytes;
		g_nDisasmCurAddress += nOpbytes;
		g_nDisasmBotAddress += nOpbytes;
		DisasmCalcTopBotAddress();
	}
	else
	{
#if DEBUG_SCROLL == 6
		// Works except on one case: G FB53, SPACE, DOWN
		WORD nTop = g_nDisasmTopAddress;
		WORD nCur = g_nDisasmCurAddress + nOpbytes;
		if (g_bDisasmCurBad)
		{
			g_nDisasmCurAddress = nCur;
			g_bDisasmCurBad = false;
			DisasmCalcTopFromCurAddress();
			return UPDATE_DISASM;
		}

		// Adjust Top until nNewCur is at > Cur
		do
		{
			g_nDisasmTopAddress++;
			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress < nCur);

		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
		g_bDisasmCurBad = false;
#endif
		g_nDisasmCurAddress += nOpbytes;

		_6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
		g_nDisasmTopAddress += nOpbytes;

		_6502_GetOpmodeOpbyte( g_nDisasmBotAddress, iOpmode, nOpbytes );
		g_nDisasmBotAddress += nOpbytes;

		if (g_bDisasmCurBad)
		{
//	MessageBox( NULL, "Bad Disassembly of opcodes", "Debugger", MB_OK );

//			g_nDisasmCurAddress = nCur;
//			g_bDisasmCurBad = false;
//			DisasmCalcTopFromCurAddress();
			DisasmCalcTopBotAddress();
//			return UPDATE_DISASM;
		}
		g_bDisasmCurBad = false;
	}

	// Can't use use + nBytes due to Disasm Singularity
//	DisasmCalcTopBotAddress();

	return UPDATE_DISASM;
}

// C++ Bug, can't have local structs used in STL containers
		struct LookAhead_t
		{
			int _nAddress;
			int _iOpcode;
			int _iOpmode;
			int _nOpbytes;
		};

//===========================================================================
Update_t CmdCursorLineUp (int nArgs)
{
	int nBytes = 1;
	
	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveUpAligned( WINDOW_DATA_BYTES_PER_LINE );
	}
	else
	if (nArgs)
	{
		nBytes = nArgs; // HACK: g_aArgs[1].nValue

		g_nDisasmTopAddress--;
		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
	}
	else
	{
//		if (! g_nDisasmCurLine)
//		{
//			g_nDisasmCurLine = 1;
//			DisasmCalcTopFromCurAddress( false );
			
//			g_nDisasmCurLine = 0;
//			DisasmCalcCurFromTopAddress();
//			DisasmCalcBotFromTopAddress();
//			return UPDATE_DISASM;
//		}

		// SmartLineUp()
		// Figure out if we should move up 1, 2, or 3 bytes since we have 2 possible cases:
		//
		// a) Scroll up by 2 bytes
		//    xx-2: A9 yy      LDA #xx
		//    xxxx: top
		//
		// b) Scroll up by 3 bytes
		//    xx-3: 20 A9 xx   JSR $00A9
		//    xxxx: top of window
		// 
#define DEBUG_SCROLL 3

#if DEBUG_SCROLL == 1
		WORD nCur = g_nDisasmCurAddress - nBytes;

		// Adjust Top until nNewCur is at > Cur
		do
		{
			g_nDisasmTopAddress--;
			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress > nCur);
#endif

#if DEBUG_SCROLL == 2
		WORD nCur = g_nDisasmCurAddress - nBytes;

		int iOpcode;
		int iOpmode;
		int nOpbytes;

		int aOpBytes[ 4 ]; // index is relative offset from cursor
		int nLeastDesiredTopAddress = NO_6502_TARGET;

		do
		{
			g_nDisasmTopAddress--;

//			_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );
			iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
			aOpBytes[ 1 ] = nOpbytes;

			// Disasm is kept in sync.  Maybe bad opcode, but if no other choices...
			if (nOpbytes == 1)
				nLeastDesiredTopAddress = g_nDisasmTopAddress;

			if (   (iOpmode == AM_1)
				|| (iOpmode == AM_2)
				|| (iOpmode == AM_3)
				|| ! _6502_IsOpcodeValid( iOpcode)
				|| (nOpbytes != 1) )
			{
 				g_nDisasmTopAddress--;
				DisasmCalcCurFromTopAddress();

				iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
				aOpBytes[ 2 ] = nOpbytes;

				if (   (iOpmode == AM_1)
					|| (iOpmode == AM_2)
					|| (iOpmode == AM_3)
					|| ! _6502_IsOpcodeValid( iOpcode)
					|| (nOpbytes != 2) )
				{
					g_nDisasmTopAddress--;
					DisasmCalcCurFromTopAddress();

					iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
					aOpBytes[ 3 ] = nOpbytes;

				if (   (iOpmode == AM_1)
					|| (iOpmode == AM_2)
					|| (iOpmode == AM_3)
					|| (nOpbytes != 3) )
					g_nDisasmTopAddress--;
					DisasmCalcCurFromTopAddress();
				}
			}

			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress > nCur);
#endif
#if DEBUG_SCROLL == 3
		// Isn't this the new DisasmCalcTopFromCurAddress() ??
		int iOpcode;
		int iOpmode;
		int nOpbytes;

		const int MAX_LOOK_AHEAD = g_nDisasmWinHeight;

		static std::vector<LookAhead_t> aTopCandidates;
		LookAhead_t tCandidate;

		aTopCandidates.clear();
		aTopCandidates.reserve( MAX_LOOK_AHEAD );

		WORD nTop = g_nDisasmTopAddress;
		WORD iTop = 0;
		WORD nCur = 0;

		do
		{
			nTop--;
			nCur = nTop;
			iTop = (g_nDisasmTopAddress - nTop);


			for (int iLine = 0; iLine < MAX_LOOK_AHEAD; iLine++ )
			{
				iOpcode = _6502_GetOpmodeOpbyte( nCur, iOpmode, nOpbytes );

				// If address on iLine = g_nDisasmCurLine + 1
				if (iLine == (g_nDisasmCurLine + 1))
				{
					if (nCur == (g_nDisasmCurAddress))
					{
						iOpcode = _6502_GetOpmodeOpbyte( nTop, iOpmode, nOpbytes );

						tCandidate._nAddress = nTop;
						tCandidate._iOpcode  = iOpcode;
						tCandidate._iOpmode  = iOpmode;
						tCandidate._nOpbytes = nOpbytes;

						aTopCandidates.push_back( tCandidate );
					}
				}
				nCur += nOpbytes;

				if (nCur > g_nDisasmCurAddress)
					break;
			}
		} while (iTop < MAX_LOOK_AHEAD);

		int nCandidates = aTopCandidates.size();
		if (nCandidates)
		{
			int iBest = NO_6502_TARGET;

			int iCandidate = 0;
			for ( ; iCandidate < nCandidates; iCandidate++ )
			{
				tCandidate = aTopCandidates.at( iCandidate );
				iOpcode = tCandidate._iOpcode;
				iOpmode = tCandidate._iOpmode;

				if (   (iOpmode != AM_1)
					&& (iOpmode != AM_2)
					&& (iOpmode != AM_3)
					&& _6502_IsOpcodeValid( iOpcode ) )
				{
					if (g_iConfigDisasmScroll == 1)
					{
						// Favor min opbytes
						if (iBest != NO_6502_TARGET)
							iBest = iCandidate;
					}
					else
					if (g_iConfigDisasmScroll == 3)
					{
						// Favor max opbytes
						iBest = iCandidate;
					}
				}
			}

			// All were "invalid", pick first choice
			if (iBest == NO_6502_TARGET)
				iBest = 0;

			tCandidate = aTopCandidates.at( iBest );
			g_nDisasmTopAddress = tCandidate._nAddress;

			DisasmCalcCurFromTopAddress();
			DisasmCalcBotFromTopAddress();
			g_bDisasmCurBad = false;
		}
		else
		{
			// Singularity
			g_bDisasmCurBad = true;

//			g_nDisasmTopAddress--;
			g_nDisasmCurAddress--;
//			g_nDisasmBotAddress--;
			DisasmCalcTopBotAddress();
		}
#endif

	}
	
	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdCursorJumpPC (int nArgs)
{
	// TODO: Allow user to decide if they want next g_aOpcodes at
	// 1) Centered (traditionaly), or
	// 2) Top of the screen

	// if (UserPrefs.bNextInstructionCentered)
	if (CURSOR_ALIGN_CENTER == nArgs)
	{
		g_nDisasmCurAddress = regs.pc;       // (2)
		WindowUpdateDisasmSize(); // calc cur line
	}
	else
	if (CURSOR_ALIGN_TOP == nArgs)
	{
		g_nDisasmCurAddress = regs.pc;       // (2)
		g_nDisasmCurLine = 0;
	}

	DisasmCalcTopBotAddress();

	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdCursorJumpRetAddr (int nArgs)
{
	WORD nAddress = _6502_GetStackReturnAddress();
	g_nDisasmCurAddress = nAddress;

	if (CURSOR_ALIGN_CENTER == nArgs)
	{
		WindowUpdateDisasmSize();
	}
	else
	if (CURSOR_ALIGN_TOP == nArgs)
	{
		g_nDisasmCurLine = 0;
	}
	DisasmCalcTopBotAddress();

	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdCursorRunUntil (int nArgs)
{
	nArgs = _Arg_1( g_nDisasmCurAddress );
	return CmdGo( nArgs, true );
}

// nDelta must be a power of 2
//===========================================================================
void _CursorMoveDownAligned ( int nDelta )
{
	if (g_iWindowThis == WINDOW_DATA)
	{
		if (g_aMemDump[0].eDevice == DEV_MEMORY)
		{
			g_aMemDump[0].nAddress += nDelta;
			g_aMemDump[0].nAddress &= _6502_MEM_END;
		}
	}
	else
	{
		int nNewAddress = g_nDisasmTopAddress; // BUGFIX: g_nDisasmCurAddress;

		if ((nNewAddress & (nDelta-1)) == 0)
			nNewAddress += nDelta;
		else
			nNewAddress += (nDelta - (nNewAddress & (nDelta-1))); // .22 Fixed: Shift-PageUp Shift-PageDown Ctrl-PageUp Ctrl-PageDown -> _CursorMoveUpAligned() & _CursorMoveDownAligned()

		g_nDisasmTopAddress = nNewAddress & _6502_MEM_END; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
	}
}

// nDelta must be a power of 2
//===========================================================================
void _CursorMoveUpAligned ( int nDelta )
{
	if (g_iWindowThis == WINDOW_DATA)
	{
		if (g_aMemDump[0].eDevice == DEV_MEMORY)
		{
			g_aMemDump[0].nAddress -= nDelta;
			g_aMemDump[0].nAddress &= _6502_MEM_END;
		}
	}
	else
	{
		int nNewAddress = g_nDisasmTopAddress; // BUGFIX: g_nDisasmCurAddress;

		if ((nNewAddress & (nDelta-1)) == 0)
			nNewAddress -= nDelta;
		else
			nNewAddress -= (nNewAddress & (nDelta-1)); // .22 Fixed: Shift-PageUp Shift-PageDown Ctrl-PageUp Ctrl-PageDown -> _CursorMoveUpAligned() & _CursorMoveDownAligned()

		g_nDisasmTopAddress = nNewAddress & _6502_MEM_END; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
	}
}


//===========================================================================
Update_t CmdCursorPageDown (int nArgs)
{
	int iLines = 0; // show at least 1 line from previous display
	int nLines = WindowGetHeight( g_iWindowThis );

	if (nLines < 2)
		nLines = 2;

	if (g_iWindowThis == WINDOW_DATA)
	{
		const int nStep = 128;
		_CursorMoveDownAligned( nStep );
	}
	else
	{
// 4
//		while (++iLines < nLines)
//			CmdCursorLineDown(nArgs);

// 5
		nLines -= (g_nDisasmCurLine + 1);
		if (nLines < 1)
			nLines = 1;
			
		while (iLines++ < nLines)
		{
			CmdCursorLineDown( 0 ); // nArgs
		}
// 6

	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdCursorPageDown256 (int nArgs)
{
	const int nStep = 256;
	_CursorMoveDownAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageDown4K (int nArgs)
{
	const int nStep = 4096;
	_CursorMoveDownAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp (int nArgs)
{
	int iLines = 0; // show at least 1 line from previous display
	int nLines = WindowGetHeight( g_iWindowThis );
	
	if (nLines < 2)
		nLines = 2;

	if (g_iWindowThis == WINDOW_DATA)
	{
		const int nStep = 128;
		_CursorMoveUpAligned( nStep );
	}
	else
	{
//		while (++iLines < nLines)
//			CmdCursorLineUp(nArgs);
		nLines -= (g_nDisasmCurLine + 1);
		if (nLines < 1)
			nLines = 1;
			
		while (iLines++ < nLines)
		{
			CmdCursorLineUp( 0 ); // smart line up
			// CmdCursorLineUp( -nLines );
		}
	}
		
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp256 (int nArgs)
{
	const int nStep = 256;
	_CursorMoveUpAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp4K (int nArgs)
{
	const int nStep = 4096;
	_CursorMoveUpAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorSetPC (int nArgs)
{
	regs.pc = g_nDisasmCurAddress; // set PC to current cursor address
	return UPDATE_DISASM;
}


// Flags __________________________________________________________________________________________


//===========================================================================
Update_t CmdFlagClear (int nArgs)
{
	int iFlag = (g_iCommand - CMD_FLAG_CLR_C);

	if (g_iCommand == CMD_FLAG_CLEAR)	// Undocumented: "cl f f ... f", eg: "se n v c" (TODO: Conflicts with monitor command #L -> 000CL)
	{
		int iArg = nArgs;
		while (iArg)
		{
			iFlag = 0;
			while (iFlag < _6502_NUM_FLAGS)
			{
//				if (g_aFlagNames[iFlag] == g_aArgs[iArg].sArg[0])
				if (g_aBreakpointSource[ BP_SRC_FLAG_N - iFlag ][0] == toupper(g_aArgs[iArg].sArg[0]))
				{
					regs.ps &= ~(1 << (7-iFlag));
					break;
				}
				iFlag++;
			}
			iArg--;			
		}
	}
	else
	{
		regs.ps &= ~(1 << iFlag);
	}

	return UPDATE_FLAGS; // 1;
}

//===========================================================================
Update_t CmdFlagSet (int nArgs)
{
	int iFlag = (g_iCommand - CMD_FLAG_SET_C);

	if (g_iCommand == CMD_FLAG_SET)	// Undocumented: "se f f ... f", eg: "se n v c"
	{
		int iArg = nArgs;
		while (iArg)
		{
			iFlag = 0;
			while (iFlag < _6502_NUM_FLAGS)
			{
//				if (g_aFlagNames[iFlag] == g_aArgs[iArg].sArg[0])
				if (g_aBreakpointSource[ BP_SRC_FLAG_N - iFlag ][0] == toupper(g_aArgs[iArg].sArg[0]))
				{
					regs.ps |= (1 << (7-iFlag));
					break;
				}
				iFlag++;
			}
			iArg--;			
		}
	}
	else
	{
		regs.ps |= (1 << iFlag);
	}
	return UPDATE_FLAGS; // 1;
}

//===========================================================================
Update_t CmdFlag (int nArgs)
{
//	if (g_aArgs[0].sArg[0] == g_aParameters[PARAM_FLAG_CLEAR].aName[0] ) // 'R'
	if (g_iCommand == CMD_FLAG_CLEAR)
		return CmdFlagClear( nArgs );
	else
	if (g_iCommand == CMD_FLAG_SET)
//	if (g_aArgs[0].sArg[0] == g_aParameters[PARAM_FLAG_SET].aName[0] ) // 'S'
		return CmdFlagSet( nArgs );

	return UPDATE_ALL; // 0;
}


// Disk ___________________________________________________________________________________________

// Usage:
//     DISK SLOT [#]                                 // Show [or set] the current slot of the Disk II I/F card (for all other cmds to act on)
//     DISK INFO                                     // Info for current drive
//     DISK # EJECT                                  // Unmount disk
//     DISK # PROTECT #                              // Write-protect disk on/off
//     DISK # "<filename>"                           // Mount filename as floppy disk
// TODO:
//     DISK # READ  <Track> <Sector> <NumSec> <Addr>     // Read Track/Sector(s)
//     DISK # READ  <Track> <Sector> Addr:Addr           // Read Track/Sector(s)
//     DISK # WRITE <Track> <Sector> Addr:Addr           // Write Track/Sector(s)
// Examples:
//     DISK INFO
Update_t CmdDisk (int nArgs)
{
	static UINT currentSlot = SLOT6;

	if (! nArgs)
		return HelpLastCommand();

	// check for info or slot command
	int iParam = 0;
	FindParam(g_aArgs[1].sArg, MATCH_EXACT, iParam, _PARAM_DISK_BEGIN, _PARAM_DISK_END);

	if (iParam == PARAM_DISK_SET_SLOT)
	{
		if (nArgs > 2)
			return HelpLastCommand();

		if (nArgs > 1)
		{
			UINT slot = g_aArgs[2].nValue;
			if (slot < SLOT1 || slot > SLOT7)
				return HelpLastCommand();

			currentSlot = slot;
		}

		ConsoleBufferPushFormat("Current Disk II slot = %d", currentSlot);
		return ConsoleUpdate();
	}

	if (GetCardMgr().QuerySlot(currentSlot) != CT_Disk2)
		return ConsoleDisplayErrorFormat("No Disk II card in slot-%d", currentSlot);

	Disk2InterfaceCard& diskCard = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(currentSlot));

	if (iParam == PARAM_DISK_INFO)
	{
		if (nArgs > 1)
			return HelpLastCommand();

		Disk_Status_e eDiskState;
		LPCTSTR       sDiskState = diskCard.GetCurrentState(eDiskState);
		BYTE          nShiftReg  = diskCard.GetCurrentShiftReg();

		static const char *aDiskStatusCHC[NUM_DISK_STATUS] =
		{
			 CHC_INFO     "%s" CHC_DEFAULT  "    " CHC_NUM_HEX "    " // DISK_STATUS_OFF
			,CHC_COMMAND  "%s" CHC_DEFAULT  " << " CHC_NUM_HEX "%02X" // DISK_STATUS_READ
			,CHC_ERROR    "%s" CHC_DEFAULT  " >> " CHC_NUM_HEX "%02X" // DISK_STATUS_WRITE
			,CHC_WARNING  "%s" CHC_DEFAULT  " >| " CHC_NUM_HEX "%02X" // DISK_STATUS_PROT
			,CHC_INFO     "%s" CHC_DEFAULT "     " CHC_NUM_HEX "    " // DISK_STATUS_EMPTY
			,CHC_INFO     "%s" CHC_DEFAULT "     " CHC_NUM_HEX "    " // DISK_STATUS_SPIN
		};

		std::string Format(
			/*CHC_DEFAULT*/ "FW"           CHC_NUM_DEC "%2d"  CHC_ARG_SEP ":"
			  CHC_DEFAULT   " D"           CHC_NUM_DEC "%d"
			  CHC_DEFAULT   " T$"          CHC_NUM_HEX "%s"   CHC_ARG_SEP ","
			  CHC_DEFAULT   " Phase $"     CHC_NUM_HEX "%s"   CHC_ARG_SEP ","
			  CHC_DEFAULT   " bitOffset $" CHC_ADDRESS "%04X" CHC_ARG_SEP ","
			  CHC_DEFAULT   " Cycles "     CHC_NUM_DEC "%.2f" CHC_ARG_SEP ", "
		);
		Format += aDiskStatusCHC[eDiskState];

		ConsolePrintFormat(
			Format.c_str(),
			diskCard.GetCurrentFirmware(),
			diskCard.GetCurrentDrive() + 1,
			diskCard.GetCurrentTrackString().c_str(),
			diskCard.GetCurrentPhaseString().c_str(),
			diskCard.GetCurrentBitOffset(),
			diskCard.GetCurrentExtraCycles(),
			sDiskState,
			nShiftReg
		);

		return ConsoleUpdate();
	}

	if (nArgs < 2)
		return HelpLastCommand();

	// first param should be drive
	int iDrive = g_aArgs[ 1 ].nValue;

	if ((iDrive < 1) || (iDrive > 2))
		return HelpLastCommand();
	
	iDrive--;

	// second param is command
	int nFound = FindParam( g_aArgs[ 2 ].sArg, MATCH_EXACT, iParam, _PARAM_DISK_BEGIN, _PARAM_DISK_END );

	if (! nFound)
		return HelpLastCommand();

	if (iParam == PARAM_DISK_EJECT)
	{
		if (nArgs > 2)
			return HelpLastCommand();

		diskCard.EjectDisk( iDrive );
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
	}
	else
	if (iParam == PARAM_DISK_PROTECT)
	{
		if (nArgs > 3)
			return HelpLastCommand();

		bool bProtect = true;

		if (nArgs == 3)
			bProtect = g_aArgs[ 3 ].nValue ? true : false;

		diskCard.SetProtect( iDrive, bProtect );
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
	}
	else
	{
		if (nArgs != 3)
			return HelpLastCommand();

		LPCTSTR pDiskName = g_aArgs[ 3 ].sArg;

		// DISK # "Diskname"
		diskCard.InsertDisk( iDrive, pDiskName, IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE );
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
	}

	return UPDATE_CONSOLE_DISPLAY;
}


// Memory _________________________________________________________________________________________


// TO DO:
// . Add support for dumping Disk][ device
//===========================================================================
bool MemoryDumpCheck (int nArgs, WORD * pAddress_ )
{
	if (! nArgs)
		return false;

	Arg_t *pArg = &g_aArgs[1];
	WORD nAddress = pArg->nValue;
	bool bUpdate = false;

	pArg->eDevice = DEV_MEMORY;						// Default

	if (strncmp(g_aArgs[1].sArg, "MB", 2) == 0)		// Mockingboard sub-unit (6522+AY8913): "MBs" or "MBsn"
	{
		UINT slot = (UINT)-1;
		UINT subUnit = 0;							// Default to 6522-A
		if (strlen(g_aArgs[1].sArg) >= 3)			// "MBs" where s = slot#
			slot = g_aArgs[1].sArg[2] - '0';
		if (strlen(g_aArgs[1].sArg) == 4)			// "MBsn" where s = slot#, n = SY6522 A or B eg. AY4A
			subUnit = g_aArgs[1].sArg[3] - 'A';
		if (slot <= 7 && subUnit <= 1)
		{
			nAddress = (slot << 4) | subUnit;		// slot=[0..7] | subUnit=[0..1]
			pArg->eDevice = DEV_MB_SUBUNIT;
			bUpdate = true;
		}
	}
	else if (strncmp(g_aArgs[1].sArg, "AY", 2) == 0)	// AY8913: "AYs" or "AYsn"
	{
		UINT slot = (UINT)-1;
		UINT subUnit = 0;							// Default to 6522-A
		if (strlen(g_aArgs[1].sArg) >= 3)			// "AYs" where s = slot#
			slot = g_aArgs[1].sArg[2] - '0';
		if (strlen(g_aArgs[1].sArg) == 4)			// "AYsn" where s = slot#, n = SY6522 A or B eg. AY4A
			subUnit = g_aArgs[1].sArg[3] - 'A';
		if (slot <= 7 && subUnit <= 1)
		{
			nAddress = (slot << 4) | subUnit;		// slot=[0..7] | subUnit=[0..1]
			pArg->eDevice = DEV_AY8913_PAIR;		// for Phasor
			bUpdate = true;
		}
	}
#ifdef SUPPORT_Z80_EMU
	else if (strcmp(g_aArgs[1].sArg, "*AF") == 0)
	{
		nAddress = ReadWordFromMemory(REG_AF);
		bUpdate = true;
	}
	else if (strcmp(g_aArgs[1].sArg, "*BC") == 0)
	{
		nAddress = ReadWordFromMemory(REG_BC);
		bUpdate = true;
	}
	else if (strcmp(g_aArgs[1].sArg, "*DE") == 0)
	{
		nAddress = ReadWordFromMemory(REG_DE);
		bUpdate = true;
	}
	else if (strcmp(g_aArgs[1].sArg, "*HL") == 0)
	{
		nAddress = ReadWordFromMemory(REG_HL);
		bUpdate = true;
	}
	else if (strcmp(g_aArgs[1].sArg, "*IX") == 0)
	{
		nAddress = ReadWordFromMemory(REG_IX);
		bUpdate = true;
	}
#endif

	if (bUpdate)
	{
		pArg->nValue = nAddress;
		strncpy_s( pArg->sArg, WordToHexStr(nAddress).c_str(), _TRUNCATE );
	}

	if (pAddress_)
	{
			*pAddress_ = nAddress;
	}

	return true;
}

//===========================================================================
Update_t CmdMemoryCompare (int nArgs )
{
	if (nArgs < 3)
		return Help_Arg_1( CMD_MEMORY_COMPARE );

	WORD nSrcAddr = g_aArgs[1].nValue;
	WORD nDstAddr = g_aArgs[3].nValue;

	WORD nSrcSymAddr;
	WORD nDstSymAddr;

	if (!nSrcAddr)
	{
		nSrcSymAddr = GetAddressFromSymbol( g_aArgs[1].sArg );
		if (nSrcAddr != nSrcSymAddr)
			nSrcAddr = nSrcSymAddr;
	}

	if (!nDstAddr)
	{
		nDstSymAddr = GetAddressFromSymbol( g_aArgs[3].sArg );
		if (nDstAddr != nDstSymAddr)
			nDstAddr = nDstSymAddr;
	}

//	if ((!nSrcAddr) || (!nDstAddr))
//		return Help_Arg_1( CMD_MEMORY_COMPARE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
static Update_t _CmdMemoryDump (int nArgs, int iWhich, int iView )
{
	WORD nAddress = 0;

	if ( ! MemoryDumpCheck(nArgs, & nAddress ) )
	{
		return Help_Arg_1( g_iCommand );
	}

	g_aMemDump[iWhich].nAddress = nAddress;
	g_aMemDump[iWhich].eDevice = g_aArgs[1].eDevice;
	g_aMemDump[iWhich].bActive = true;
	g_aMemDump[iWhich].eView = (MemoryView_e) iView;

	return UPDATE_MEM_DUMP; // TODO: This really needed? Don't think we do any actual ouput
}

//===========================================================================
bool _MemoryCheckMiniDump ( int iWhich )
{
	if ((iWhich < 0) || (iWhich > NUM_MEM_MINI_DUMPS))
	{
		ConsoleDisplayErrorFormat( "  Only %d memory mini dumps", NUM_MEM_MINI_DUMPS );
		return true;
	}
	return false;
}

//===========================================================================
Update_t CmdMemoryMiniDumpHex (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_HEX_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_HEX );
}

//===========================================================================
Update_t CmdMemoryMiniDumpAscii (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_ASCII_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_ASCII );
}

//===========================================================================
Update_t CmdMemoryMiniDumpApple (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_APPLE_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_LO );
}

//===========================================================================
//Update_t CmdMemoryMiniDumpLow (int nArgs)
//{
//	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_TXT_LO_1;
//	if (_MemoryCheckMiniDump( iWhich ))
//		return UPDATE_CONSOLE_DISPLAY;
//
//	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_LO );
//}

//===========================================================================
//Update_t CmdMemoryMiniDumpHigh (int nArgs)
//{
//	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_TXT_HI_1;
//	if (_MemoryCheckMiniDump( iWhich ))
//		return UPDATE_CONSOLE_DISPLAY;
//
//	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_HI );
//}

//===========================================================================
Update_t CmdMemoryEdit (int nArgs)
{

	return UPDATE_CONSOLE_DISPLAY;
}

// MEB addr 8_bit_value
//===========================================================================
Update_t CmdMemoryEnterByte (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != '0') && (!g_aArgs[2].nValue))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nValue;
	while (nArgs >= 2)
	{
		WORD nData = g_aArgs[nArgs].nValue;

		if (nData > 0xFF)
		{
			WriteByteToMemory(nAddress + nArgs - 2, (BYTE)(nData >> 0));
			WriteByteToMemory(nAddress + nArgs - 1, (BYTE)(nData >> 8));
		}
		else
		{
			WriteByteToMemory(nAddress + nArgs - 2, (BYTE)nData);
		}

		nArgs--;
	}

	return UPDATE_ALL;
}

// MEW addr 16-bit_vaue
//===========================================================================
Update_t CmdMemoryEnterWord (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != '0') && (!g_aArgs[2].nValue))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nValue;
	while (nArgs >= 2)
	{
		WORD nData = g_aArgs[nArgs].nValue;

		WriteByteToMemory(nAddress + nArgs - 2, (BYTE)(nData >> 0));
		WriteByteToMemory(nAddress + nArgs - 1, (BYTE)(nData >> 8));

		nArgs--;
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdMemoryFill (int nArgs)
{
	// F address end value
	// F address,len value
	// F address:end value
	if ((!nArgs) || (nArgs < 3) || (nArgs > 4))
		return Help_Arg_1( CMD_MEMORY_FILL );

	WORD nAddress2 = 0;
	WORD nAddressStart = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;
	BYTE nValue = 0;

	if ( nArgs == 3)
	{
		nAddressStart = g_aArgs[1].nValue;
		nAddressEnd   = g_aArgs[2].nValue;
		nAddressLen = MIN(_6502_MEM_END , nAddressEnd - nAddressStart + 1 );
	}
	else
	{
		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, 1 );

		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			return Help_Arg_1( CMD_MEMORY_MOVE );
	}
#if DEBUG_VAL_2
		nBytes   = MAX(1,g_aArgs[1].nVal2); // TODO: This actually work??
#endif

	if ((nAddressLen > 0) && (nAddressEnd <= _6502_MEM_END))
	{
		nValue = g_aArgs[nArgs].nValue & 0xFF;
		while ( nAddressLen-- ) // v2.7.0.22
		{
			// TODO: Optimize - split into pre_io, and post_io
			if ((nAddress2 < APPLE_IO_BEGIN) || (nAddress2 > APPLE_IO_END))
			{
				WriteByteToMemory(nAddressStart, nValue);
			}
			nAddressStart++;
		}
	}

	return UPDATE_ALL; // UPDATE_CONSOLE_DISPLAY;
}


static std::string g_sMemoryLoadSaveFileName;


// "PWD"
//===========================================================================
Update_t CmdConfigGetDebugDir (int nArgs)
{
	if ( nArgs != 0 )
		return Help_Arg_1( CMD_CONFIG_GET_DEBUG_DIR );

	// TODO: debugger dir has no ` CONSOLE_COLOR_ESCAPE_CHAR ?!?!
	ConsoleBufferPushFormat( "Path: %s", g_sCurrentDir.c_str() );

	return ConsoleUpdate();
}

// Usage:
//     CD "<dir>"
//     CD ".."
// Note: Subdirectory MUST be quoted with double quotes.
//===========================================================================
Update_t CmdConfigSetDebugDir (int nArgs)
{
	if ( nArgs > 1 )
		return Help_Arg_1( CMD_CONFIG_SET_DEBUG_DIR );

	if ( nArgs == 0 )
		return CmdConfigGetDebugDir(0);

	// http://msdn.microsoft.com/en-us/library/aa365530(VS.85).aspx

	// TODO: Support paths prefixed with "\\?\" (for long/unicode pathnames)
	if (strncmp("\\\\?\\", g_aArgs[1].sArg, 4) == 0)
		return Help_Arg_1( CMD_CONFIG_SET_DEBUG_DIR );

	std::string sPath;

	if (g_aArgs[1].sArg[1] == ':')			// Absolute
	{
		sPath = g_aArgs[1].sArg;
	}
	else if (g_aArgs[1].sArg[0] == PATH_SEPARATOR)	// Absolute
	{
		if (g_sCurrentDir[1] == ':')
		{
			sPath = g_sCurrentDir.substr(0, 2) + g_aArgs[1].sArg;	// Prefix with drive letter & colon
		}
		else
		{
			sPath = g_aArgs[1].sArg;
		}
	}
	else									// Relative
	{
		std::string SAME_DIR( "." ); SAME_DIR += PATH_SEPARATOR;
		std::string UP_DIR  ( ".."); UP_DIR   += PATH_SEPARATOR;
		std::string sNewPath( g_aArgs[1].sArg );

		// if new path doesn't have a trailing slash, append one
		if (*(sNewPath.rbegin()) != PATH_SEPARATOR)
			sNewPath += PATH_SEPARATOR;
				
		// Support ".." and various permutations
		//     cd "..\"
		//     cd "abc\..\def\"
		//
		// 1. find next slash in newpath
		// 2. subdir = newpath.substr()
		// 3. if subdir == "..\"
		//        reverse find slash in g_sCurrentDir
		//        g_sCurrentDir = g_sCurrentDir.substr()
		//    else
		//        g_sCurrentDir += subdir
		size_t iPrevSeparator = 0;
		size_t iPathSeparator = 0;

		while ((iPathSeparator = sNewPath.find( PATH_SEPARATOR, iPrevSeparator )) != std::string::npos)
		{
#if _DEBUG
			LogOutput( "Prev: %" SIZE_T_FMT "\n", iPrevSeparator	);
			LogOutput( "Next: %" SIZE_T_FMT "\n", iPathSeparator	);
			LogOutput( "%s\n", sNewPath.c_str()						);
			LogOutput( "%*s%s\n", int(iPathSeparator), "", "^"		);
#endif

			std::string sSubDir = sNewPath.substr( iPrevSeparator, iPathSeparator - iPrevSeparator + 1 );
			const size_t nSubDirLen = sSubDir.size();

			if ((nSubDirLen == 2) && (sSubDir == SAME_DIR)) // Same directory ".\" in the subpath?
			{
				// Intentional: Nothing to do
			}
			else
			if ((nSubDirLen == 3) && (sSubDir == UP_DIR))   // Up directory "..\" in the subpath?
			{
				size_t nCurrentLen    = g_sCurrentDir.size();
				size_t nLastSeparator = g_sCurrentDir.rfind( '\\', nCurrentLen - 2 );

				if (nLastSeparator != std::string::npos)
				{
#if _DEBUG
					LogOutput( "Last: %" SIZE_T_FMT "\n", nLastSeparator	);
					LogOutput( "%s\n", g_sCurrentDir.c_str()				);
					LogOutput( "%*s%s\n", int(nLastSeparator), "", "^"		);
#endif
					std::string sCurrentDir = g_sCurrentDir.substr( 0, nLastSeparator  + 1 ); // Path always has trailing slash so include it
					g_sCurrentDir = sCurrentDir;
				}
			}
			else
				g_sCurrentDir += sSubDir;

			iPathSeparator++; // start next search past path separator
			iPrevSeparator = iPathSeparator;
		}

		// TODO: debugger dir has no ` CONSOLE_COLOR_ESCAPE_CHAR ?!?!
		sPath = g_sCurrentDir;
	}

	if ( SetCurrentImageDir( sPath ) )
		nArgs = 0; // intentional fall into

	return CmdConfigGetDebugDir(0);		// Show the new PWD
}


//===========================================================================
#if 0	// Original - TODO: delete this old "original" code
Update_t CmdMemoryLoad (int nArgs)
{
	// BLOAD ["Filename"] , addr[, len] 
	// BLOAD ["Filename"] , addr[: end] 
	//       1            2 3    4 5
	if (nArgs > 5)
		return Help_Arg_1( CMD_MEMORY_LOAD );

	bool bHaveFileName = false;
	int iArgAddress = 3;

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
		bHaveFileName = true;

//	if (g_aArgs[2].bType & TOKEN_QUOTE_DOUBLE)
//		bHaveFileName = true;

	if (nArgs > 1)
	{
		if (g_aArgs[1].bType & TYPE_QUOTED_2)
			bHaveFileName = true;

		int iArgComma1  = 2;
		int iArgAddress = 3;
		int iArgComma2  = 4;
		int iArgLength  = 5;

		if (! bHaveFileName)
		{
			iArgComma1  = 1;
			iArgAddress = 2;
			iArgComma2  = 3;
			iArgLength  = 4;

			if (nArgs > 4)
				return Help_Arg_1( CMD_MEMORY_LOAD );
		}

		if (g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA)
			return Help_Arg_1( CMD_MEMORY_SAVE );

		std::string sLoadSaveFilePath = g_sCurrentDir; // TODO: g_sDebugDir

		WORD nAddressStart;
		WORD nAddress2   = 0;
		WORD nAddressEnd = 0;
		int  nAddressLen = 0;

		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );
		if (nArgs > 4)
		{
			if (eRange == RANGE_MISSING_ARG_2)
			{
				return Help_Arg_1( CMD_MEMORY_LOAD );
			}

//			if (eRange == RANGE_MISSING_ARG_2)
			if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			{
				return Help_Arg_1( CMD_MEMORY_SAVE );
			}
		}
		
		BYTE *pMemory = new BYTE [ _6502_MEM_END + 1 ]; // default 64K buffer
		BYTE *pSrc = pMemory;

		if (bHaveFileName)
		{
			g_sMemoryLoadSaveFileName = g_aArgs[ 1 ].sArg;
		}
		sLoadSaveFilePath += g_sMemoryLoadSaveFileName;
		
		FILE *hFile = fopen( sLoadSaveFilePath.c_str(), "rb" );
		if (hFile)
		{
			int nFileBytes = _GetFileSize( hFile );

			if (nFileBytes > _6502_MEM_END)
				nFileBytes = _6502_MEM_END + 1; // Bank-switched RAMR/ROM is only 16-bit

			// Caller didnt' specify how many bytes to read, default to them all
			if (nAddressLen == 0)
			{
				nAddressLen = nFileBytes;
			}

			size_t nRead = fread( pMemory, nAddressLen, 1, hFile );
			if (nRead == 1) // (size_t)nLen)
			{
				for ( int iByte = 0; iByte < nAddressLen; iByte++ )
				{
					WriteByteToMemory(nAddressStart++, *pSrc++);
				}
				ConsoleBufferPush(  "Loaded."  );
			}
			fclose( hFile );
		}
		else
		{
			ConsoleBufferPush(  "ERROR: Bad filename"  );

			CmdConfigGetDebugDir( 0 );

			ConsoleBufferPushFormat( "File: %s", g_sMemoryLoadSaveFileName.c_str() );
		}
		
		delete [] pMemory;
	}
	else
		return Help_Arg_1( CMD_MEMORY_LOAD );
	
	return ConsoleUpdate();
}
#else	// Extended cmd for loading physical memory
Update_t CmdMemoryLoad (int nArgs)
{
	// Active memory:
	// BLOAD ["Filename"] , addr[, len] 
	// BLOAD ["Filename"] , addr[: end] 
	//       1            2 3    4 5
	// Physical 64K memory bank:
	// BLOAD ["Filename"] , bank : addr [, len]
	// BLOAD ["Filename"] , bank : addr [: end]
	//       1            2 3    4 5     6 7
	if (nArgs > 7)
		return Help_Arg_1( CMD_MEMORY_LOAD );

	if (nArgs < 1)
		return Help_Arg_1( CMD_MEMORY_LOAD );

	bool bHaveFileName = false;

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
		bHaveFileName = true;

//	if (g_aArgs[2].bType & TOKEN_QUOTE_DOUBLE)
//		bHaveFileName = true;

	int iArgComma1  = 2;
	int iArgAddress = 3;
	int iArgComma2  = 4;
	int iArgLength  = 5;
	int iArgBank    = 3;
	int iArgColon   = 4;

	int nBank = 0;
	bool bBankSpecified = false;

	if (! bHaveFileName)
	{
		iArgComma1  = 1;
		iArgAddress = 2;
		iArgComma2  = 3;
		iArgLength  = 4;
		iArgBank    = 2;
		iArgColon   = 3;

		if (nArgs > 6)
			return Help_Arg_1( CMD_MEMORY_LOAD );
	}

	if (nArgs >= 5)
	{
		if (!(g_aArgs[iArgBank].bType & TYPE_ADDRESS && g_aArgs[iArgColon].eToken == TOKEN_COLON))
			return Help_Arg_1( CMD_MEMORY_LOAD );

		nBank = g_aArgs[iArgBank].nValue;
		bBankSpecified = true;

		iArgAddress += 2;
		iArgComma2  += 2;
		iArgLength  += 2;
	}
	else
	{
		bBankSpecified = false;
	}

	struct KnownFileType_t
	{
		const char *pExtension;
		int   nAddress;
		int   nLength;
	};

	const KnownFileType_t aFileTypes[] = 
	{
		 { ""     ,      0,      0 } // n/a
		,{ ".hgr" , 0x2000, 0x2000 }
		,{ ".hgr2", 0x4000, 0x2000 }
		// TODO: extension ".dhgr", ".dhgr2"
	};
	const int              nFileTypes = sizeof( aFileTypes ) / sizeof( KnownFileType_t );
	const KnownFileType_t *pFileType = NULL;

	const char *pFileName = g_aArgs[ 1 ].sArg;
	int   nLen = strlen( pFileName );
	const char *pEnd = pFileName + nLen - 1;
	while ( pEnd > pFileName )
	{
		if ( *pEnd == '.' )
		{
			for ( int i = 1; i < nFileTypes; i++ )
			{
				if ( strcmp( pEnd, aFileTypes[i].pExtension ) == 0 )
				{
					pFileType = &aFileTypes[i];
					break;
				}
			}
		}

		if ( pFileType )
			break;

		pEnd--;
	}

	if ( !pFileType )
		if (g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA)
			return Help_Arg_1( CMD_MEMORY_LOAD );

	WORD nAddressStart = 0;
	WORD nAddress2     = 0;
	WORD nAddressEnd   = 0;
	int  nAddressLen   = 0;

	if ( pFileType )
	{
		nAddressStart = pFileType->nAddress;
		nAddressLen   = pFileType->nLength;
		nAddressEnd   = pFileType->nLength + nAddressLen;
	}

	RangeType_t eRange = RANGE_MISSING_ARG_2;

	if (g_aArgs[ iArgComma1 ].eToken == TOKEN_COMMA)
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );

	if ( nArgs > iArgComma2 )
	{
		if (eRange == RANGE_MISSING_ARG_2)
		{
			return Help_Arg_1( CMD_MEMORY_LOAD );
		}

//		if (eRange == RANGE_MISSING_ARG_2)
		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
		{
			return Help_Arg_1( CMD_MEMORY_LOAD );
		}
	}

	std::unique_ptr<BYTE> pMemory(new BYTE[_6502_MEM_END + 1]); // default 64K buffer

	if (bHaveFileName)
	{
		g_sMemoryLoadSaveFileName = pFileName;
	}
	const std::string sLoadSaveFilePath = g_sCurrentDir + g_sMemoryLoadSaveFileName; // TODO: g_sDebugDir
	
	FILE *hFile = fopen( sLoadSaveFilePath.c_str(), "rb" );
	if (hFile)
	{
		size_t nFileBytes = _GetFileSize( hFile );

		if (nFileBytes > _6502_MEM_END)
			nFileBytes = _6502_MEM_END + 1; // Bank-switched RAM/ROM is only 16-bit

		// Caller didn't specify how many bytes to read, default to them all
		if (nAddressLen == 0)
		{
			nAddressLen = nFileBytes;
		}

		size_t nRead = fread(pMemory.get() + nAddressStart, nAddressLen, 1, hFile);
		fclose(hFile);

		if (nRead == 1)
		{
			ConsoleBufferPushFormat( "Loaded @ A$%04X,L$%04X", nAddressStart, nAddressLen );
		}
		else
		{
			ConsoleBufferPush( "Error loading data." );
			return ConsoleUpdate();
		}

		if (bBankSpecified)
		{
			BYTE* const pMemBankBase = MemGetBankPtr(nBank);
			if (!pMemBankBase)
			{
				ConsoleBufferPush("Error: Bank out of range.");
				return ConsoleUpdate();
			}

			memcpy(pMemBankBase + nAddressStart, pMemory.get() + nAddressStart, nAddressLen);

			MemUpdatePaging(TRUE);
		}
		else
		{
			for (WORD i=nAddressStart; i!=(nAddressStart+(WORD)nAddressLen); i++)
			{
				WriteByteToMemory(i, pMemory.get()[i]);
			}
		}
	}
	else
	{
		ConsoleBufferPush( "ERROR: Bad filename" );

		CmdConfigGetDebugDir( 0 );

		ConsoleBufferPushFormat( "File: %s", g_sMemoryLoadSaveFileName.c_str() );
	}
	
	return ConsoleUpdate();
}
#endif

// dst src : len
//===========================================================================
Update_t CmdMemoryMove (int nArgs)
{
	// M destaddr address end
	// M destaddr address,len
	// M destaddr address:end
	// 2000<4000.5FFFM
	if (nArgs < 3)
		return Help_Arg_1( CMD_MEMORY_MOVE );

	WORD nDst = g_aArgs[1].nValue;
//	WORD nSrc = g_aArgs[2].nValue;
//	WORD nLen = g_aArgs[3].nValue - nSrc;
	WORD nAddress2 = 0;
	WORD nAddressStart = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;

	RangeType_t eRange;
	eRange = Range_Get( nAddressStart, nAddress2, 2 );

//		if (eRange == RANGE_MISSING_ARG_2)
	if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
		return Help_Arg_1( CMD_MEMORY_MOVE );

	if ((nAddressLen > 0) && (nAddressEnd <= _6502_MEM_END))
	{
		while ( nAddressLen-- ) // v2.7.0.23
		{
			// TODO: Optimize - split into pre_io, and post_io
			if ((nDst < APPLE_IO_BEGIN) || (nDst > APPLE_IO_END))
			{
				BYTE value = ReadByteFromMemory(nAddressStart);
				WriteByteToMemory(nDst, value);
			}
			nDst++;
			nAddressStart++;
		}

		return UPDATE_ALL;
	}

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
#if 0	// Original - TODO: delete this old "original" code
Update_t CmdMemorySave (int nArgs)
{
	// BSAVE ["Filename"] , addr , len 
	// BSAVE ["Filename"] , addr : end
	//       1            2 3    4 5
	static WORD nAddressStart = 0;
	       WORD nAddress2     = 0;
	static WORD nAddressEnd   = 0;
	static int  nAddressLen   = 0;

	if (nArgs > 5)
		return Help_Arg_1( CMD_MEMORY_SAVE );

	if (! nArgs)
	{
		if (nAddressLen)
		{
			ConsoleBufferPushFormat( "Last saved: $%04X:$%04X, %04X",
				nAddressStart, nAddressEnd, nAddressLen );
		}
		else
		{
			ConsoleBufferPush( "Last saved: none" );
		}				
	}
	else
	{
		bool bHaveFileName = false;

		if (g_aArgs[1].bType & TYPE_QUOTED_2)
			bHaveFileName = true;

//		if (g_aArgs[1].bType & TOKEN_QUOTE_DOUBLE)
//			bHaveFileName = true;

		int iArgComma1  = 2;
		int iArgAddress = 3;
		int iArgComma2  = 4;
		int iArgLength  = 5;

		if (! bHaveFileName)
		{
			iArgComma1  = 1;
			iArgAddress = 2;
			iArgComma2  = 3;
			iArgLength  = 4;

			if (nArgs > 4)
				return Help_Arg_1( CMD_MEMORY_SAVE );
		}

//		if ((g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA) || 
//			(g_aArgs[ iArgComma2 ].eToken != TOKEN_COLON))
//			return Help_Arg_1( CMD_MEMORY_SAVE );

		char sLoadSaveFilePath[ MAX_PATH ];
		strcpy( sLoadSaveFilePath, g_sCurrentDir ); // g_sProgramDir

		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );

//		if (eRange == RANGE_MISSING_ARG_2)
		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			return Help_Arg_1( CMD_MEMORY_SAVE );

		if ((nAddressLen) && (nAddressEnd <= _6502_MEM_END))
		{
			if (! bHaveFileName)
			{
				g_sMemoryLoadSaveFileName = StrFormat( "%04X.%04X.bin", nAddressStart, nAddressLen ); // nAddressEnd );
			}
			else
			{
				g_sMemoryLoadSaveFileName = g_aArgs[ 1 ].sArg;
			}
			strcat( sLoadSaveFilePath, g_sMemoryLoadSaveFileName );

//				if (nArgs == 2)
			{
				BYTE *pMemory = new BYTE [ nAddressLen ];
				BYTE *pDst = pMemory;
				
				// memcpy -- copy out of active memory bank
				for ( int iByte = 0; iByte < nAddressLen; iByte++ )
				{
					*pDst++ = ReadByteFromMemory(nAddressStart + iByte);
				}

				FILE *hFile = fopen( sLoadSaveFilePath, "rb" );
				if (hFile)
				{
					ConsoleBufferPush(  "Warning: File already exists.  Overwriting."  );
					fclose( hFile );
				}

				hFile = fopen( sLoadSaveFilePath, "wb" );
				if (hFile)
				{
					size_t nWrote = fwrite( pMemory, nAddressLen, 1, hFile );
					if (nWrote == 1) // (size_t)nAddressLen)
					{
						ConsoleBufferPush(  "Saved."  );
					}
					else
					{
						ConsoleBufferPush(  "Error saving."  );
					}
					fclose( hFile );
				}
				
				delete [] pMemory;
			}
		}
	}
	
	return ConsoleUpdate();
}
#else	// Extended cmd for saving physical memory
Update_t CmdMemorySave (int nArgs)
{
	// Active memory:
	// BSAVE ["Filename"] , addr , len
	// BSAVE ["Filename"] , addr : end
	//       1            2 3    4 5
	// Physical 64K memory bank:
	// BSAVE ["Filename"] , bank : addr , len
	// BSAVE ["Filename"] , bank : addr : end
	//       1            2 3    4 5    6 7
	static WORD nAddressStart = 0;
	       WORD nAddress2     = 0;
	static WORD nAddressEnd   = 0;
	static int  nAddressLen   = 0;
	static int  nBank         = 0;
	static bool bBankSpecified = false;

	if (nArgs > 7)
		return Help_Arg_1( CMD_MEMORY_SAVE );

	if (! nArgs)
	{
		if (nAddressLen)
		{
			if (!bBankSpecified)
				ConsoleBufferPushFormat( "Last saved: $%04X:$%04X, %04X",
					nAddressStart, nAddressEnd, nAddressLen );
			else
				ConsoleBufferPushFormat( "Last saved: Bank=%02X $%04X:$%04X, %04X",
					nBank, nAddressStart, nAddressEnd, nAddressLen );
		}
		else
		{
			ConsoleBufferPush(  "Last saved: none"  );
		}				
	}
	else
	{
		bool bHaveFileName = false;

		if (g_aArgs[1].bType & TYPE_QUOTED_2)
			bHaveFileName = true;

//		if (g_aArgs[1].bType & TOKEN_QUOTE_DOUBLE)
//			bHaveFileName = true;

//		int iArgComma1  = 2;
		int iArgAddress = 3;
		int iArgComma2  = 4;
		int iArgLength  = 5;
		int iArgBank    = 3;
		int iArgColon   = 4;

		if (! bHaveFileName)
		{
//			iArgComma1  = 1;
			iArgAddress = 2;
			iArgComma2  = 3;
			iArgLength  = 4;
			iArgBank    = 2;
			iArgColon   = 3;

			if (nArgs > 6)
				return Help_Arg_1( CMD_MEMORY_SAVE );
		}

		if (nArgs > 5)
		{
			if (!(g_aArgs[iArgBank].bType & TYPE_ADDRESS && g_aArgs[iArgColon].eToken == TOKEN_COLON))
				return Help_Arg_1( CMD_MEMORY_SAVE );

			nBank = g_aArgs[iArgBank].nValue;
			bBankSpecified = true;

			iArgAddress += 2;
			iArgComma2  += 2;
			iArgLength  += 2;
		}
		else
		{
			bBankSpecified = false;
		}

//		if ((g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA) || 
//			(g_aArgs[ iArgComma2 ].eToken != TOKEN_COLON))
//			return Help_Arg_1( CMD_MEMORY_SAVE );

		std::string sLoadSaveFilePath = g_sCurrentDir; // g_sProgramDir

		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );

//		if (eRange == RANGE_MISSING_ARG_2)
		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			return Help_Arg_1( CMD_MEMORY_SAVE );

		if ((nAddressLen) && (nAddressEnd <= _6502_MEM_END))
		{
			if (! bHaveFileName)
			{
				g_sMemoryLoadSaveFileName = (bBankSpecified)
					? StrFormat( "%04X.%04X.bank%02X.bin", nAddressStart, nAddressLen, nBank )
					: StrFormat( "%04X.%04X.bin", nAddressStart, nAddressLen );
			}
			else
			{
				g_sMemoryLoadSaveFileName = g_aArgs[ 1 ].sArg;
			}
			sLoadSaveFilePath += g_sMemoryLoadSaveFileName;

			std::unique_ptr<BYTE> pMemory(new BYTE[_6502_MEM_END + 1]); // default 64K buffer

			if (bBankSpecified)
			{
				const BYTE* const pMemBankBase = MemGetBankPtr(nBank);
				if (!pMemBankBase)
				{
					ConsoleBufferPush("Error: Bank out of range.");
					return ConsoleUpdate();
				}

				memcpy(pMemory.get() + nAddressStart, pMemBankBase + nAddressStart, nAddressLen);
			}
			else
			{
				for (WORD i = nAddressStart; i != (nAddressStart + (WORD)nAddressLen); i++)
				{
					pMemory.get()[i] = ReadByteFromMemory(i);
				}
			}

			FILE *hFile = fopen( sLoadSaveFilePath.c_str(), "rb" );
			if (hFile)
			{
				ConsoleBufferPush(  "Warning: File already exists.  Overwriting."  );
				fclose( hFile );
				// TODO: BUG: Is this a bug/feature that we can over-write files and the user has no control over that?
			}

			hFile = fopen( sLoadSaveFilePath.c_str(), "wb" );
			if (hFile)
			{
				size_t nWrote = fwrite(pMemory.get() + nAddressStart, nAddressLen, 1, hFile);
				fclose(hFile);

				if (nWrote == 1)
					ConsoleBufferPush(  "Saved."  );
				else
					ConsoleBufferPush(  "Error saving."  );
			}
			else
			{
				ConsoleBufferPush(  "Error opening file."  );
			}
		}
		else
		{
			ConsoleBufferPush("Error: Length = 0.");
		}
	}
	
	return ConsoleUpdate();
}
#endif


char g_aTextScreen[ DEBUG_VIRTUAL_TEXT_HEIGHT * (DEBUG_VIRTUAL_TEXT_WIDTH + 4) ]; // (80 column + CR + LF) * 24 rows + NULL
int  g_nTextScreen = 0;

		/*
			$FBC1 BASCALC  IN: A=row, OUT: $28=low, $29=hi
			BASCALC     PHA          ; 000abcde  -> temp
			            LSR          ; 0000abcd  CARRY=e y /= 2
			            AND #3       ; 000000cd  y & 3
			            ORA #4       ; 000001cd  y | 4 
			            STA $29
			            PLA          ; 000abcde <- temp
			            AND #$18     ; 000ab000 
			            BCC BASCLC2  ; e=0?
			            ADC #7F      ; 100ab000  yes CARRY=e=1 -> #$+80
			BASCLC2     STA $28      ; e00ab000  no  CARRY=e=0
			            ASL          ; 00ab0000
			            ASL          ; 0ab00000
			            ORA $28      ; 0abab000
			            STA $28      ; eabab000
			
		300:A9 00 20 C1 FB A5 29 A6 28 4C 41 F9

		y  Hex  000a_bcde            01cd_eaba_b000
		 0  00  0000_0000  ->  $400  0100 0000_0000
		 1  01  0000_0001  ->  $480  0100 1000_0000
		 2  02  0000_0010  ->  $500  0101 0000_0000
		 3  03  0000_0011  ->  $580  0101 1000_0000
		 4  04  0000_0100  ->  $600  0110 0000_0000
		 5  05  0000_0101  ->  $680  0110 1000_0000
		 6  06  0000_0110  ->  $700  0111 0000_0000
		 7  07  0000_0111  ->  $780  0111 1000_0000

		 8  08  0000_1000  ->  $428  0100 0010_1000
		 9  09  0000_1001  ->  $4A8  0100 1010_1000
		10  0A  0000_1010  ->  $528  0101 0010_1000
		11  0B  0000_1011  ->  $5A8  0101 1010_1000
		12  0C  0000_1100  ->  $628  0110 0010_1000
		13  0D  0000_1101  ->  $6A8  0110 1010_1000
		14  0E  0000_1110  ->  $728  0111 0010_1000
		15  0F  0000_1111  ->  $7A8  0111 1010_1000

		16  10  0001_0000  ->  $450  0100 0101_0000
		17  11  0001_0001  ->  $4D0  0100 1101_0000
		18  12  0001_0010  ->  $550  0101 0101_0000
		19  13  0001_0011  ->  $5D0  0101 1101_0000
		20  14  0001_0100  ->  $650  0110 0101_0000
		21  15  0001_0101  ->  $6D0  0110_1101_0000
		22  16  0001_0110  ->  $750  0111_0101_0000
		23  17  0001_0111  ->  $7D0  0111 1101 0000
	*/

// Convert ctrl characters to displayable
// Note: FormatCharTxtCtrl() and RemapChar()
static char RemapChar (const char c)
{
	if ( c < 0x20 )
		return c + '@'; // Remap INVERSE control character to NORMAL
	else if ( c == 0x7F )
		return ' '; // Remap checkboard (DEL) to space

	return c;
}


size_t Util_GetDebuggerText ( char* &pText_ )
{
	char  *pBeg = &g_aTextScreen[0];
	char  *pEnd = &g_aTextScreen[0];

	g_nTextScreen = 0;
	memset( pBeg, 0, sizeof( g_aTextScreen ) );

	memset( g_aDebuggerVirtualTextScreen, 0, sizeof( g_aDebuggerVirtualTextScreen ) );
	DebugDisplay();

	for ( int y = 0; y < DEBUG_VIRTUAL_TEXT_HEIGHT; y++ )
	{
		for ( int x = 0; x < DEBUG_VIRTUAL_TEXT_WIDTH; x++ )
		{
			char c = g_aDebuggerVirtualTextScreen[y][x];
			if ( (c < 0x20) || (c >= 0x7F) )
				c = ' '; // convert null to spaces to keep everything non-proptional
			*pEnd++ = c;
		}
#ifdef _WIN32
		*pEnd++ = 0x0D; // CR // Windows inserts extra char
#endif
		*pEnd++ = 0x0A; // LF // OSX, Linux
	}

	*pEnd = 0;
	g_nTextScreen = pEnd - pBeg;
	
	pText_ = pBeg;
	return g_nTextScreen;
}

size_t Util_GetTextScreen ( char* &pText_ )
{
	WORD nAddressStart = 0;

	char  *pBeg = &g_aTextScreen[0];
	char  *pEnd = &g_aTextScreen[0];

	g_nTextScreen = 0;
	memset( pBeg, 0, sizeof( g_aTextScreen ) );

	const unsigned int uBank2 = (!GetVideo().VideoGetSW80STORE() && GetVideo().VideoGetSWPAGE2()) ? 1 : 0;
	LPBYTE g_pTextBank1  = MemGetAuxPtr (0x400 << uBank2);
	LPBYTE g_pTextBank0  = MemGetMainPtr(0x400 << uBank2);

	for ( int y = 0; y < 24; y++ )
	{
		// nAddressStart = 0x400 + (y%8)*0x80 + (y/8)*0x28;
		nAddressStart = ((y&7)<<7) | ((y&0x18)<<2) | (y&0x18); // no 0x400| since using MemGet*Ptr()

		for ( int x = 0; x < 40; x++ ) // always 40 columns
		{
			char c; // TODO: FormatCharTxtCtrl() ?

			if ( GetVideo().VideoGetSW80COL() )
			{ // AUX
				c = g_pTextBank1[ nAddressStart ] & 0x7F;
				c = RemapChar(c);
				*pEnd++ = c;
			} // MAIN -- NOTE: intentional indent & outside if () !

				c = g_pTextBank0[ nAddressStart ] & 0x7F;
				c = RemapChar(c);
				*pEnd++ = c;

			nAddressStart++;
		}

		// Newline // http://en.wikipedia.org/wiki/Newline
#ifdef _WIN32
		*pEnd++ = 0x0D; // CR // Windows inserts extra char
#endif
		*pEnd++ = 0x0A; // LF // OSX, Linux
	}
	*pEnd = 0;

	g_nTextScreen = pEnd - pBeg;
	
	pText_ = pBeg;
	return g_nTextScreen;
}

//===========================================================================
Update_t CmdNTSC (int nArgs)
{
	int iParam;
	int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

	struct KnownFileType_t
	{
		const char *pExtension;
	};

	enum KnownFileType_e
	{
		 TYPE_UNKNOWN
		,TYPE_BMP
		,TYPE_RAW
		,NUM_FILE_TYPES
	};

	const KnownFileType_t aFileTypes[ NUM_FILE_TYPES ] = 
	{
		 { ""      } // n/a
		,{ ".bmp"  }
		,{ ".data" }
//		,{ ".raw"  }
//		,{ ".ntsc" }
	};
	const int              nFileType = sizeof( aFileTypes ) / sizeof( KnownFileType_t );
	const KnownFileType_t *pFileType = NULL;
	/* */ KnownFileType_e  iFileType = TYPE_UNKNOWN;

#if _DEBUG
	assert( (nFileType == NUM_FILE_TYPES) );
#endif

	const char *pFileName = (nArgs > 1) ? g_aArgs[ 2 ].sArg : "";
	int   nLen = strlen( pFileName );
	const char *pEnd = pFileName + nLen - 1;
	while ( pEnd > pFileName )
	{
		if ( *pEnd == '.' )
		{
			for ( int i = TYPE_BMP; i < NUM_FILE_TYPES; i++ )
			{
				if ( strcmp( pEnd, aFileTypes[i].pExtension ) == 0 )
				{
					pFileType = &aFileTypes[i];
					iFileType = (KnownFileType_e) i;
					break;
				}
			}
		}

		if ( pFileType )
			break;

		pEnd--;
	}

	if ( nLen == 0 )
		pFileName = "AppleWinNTSC4096x4@32.data";

	static std::string sPaletteFilePath;
	sPaletteFilePath = g_sCurrentDir + pFileName;

	class ConsoleFilename
	{
		public:
			static void update( const char *pPrefixText )
			{
				size_t len1 = strlen( pPrefixText      );
				size_t len2 = sPaletteFilePath.size();
				size_t len  = len1 + len2;

				if (len >= CONSOLE_WIDTH)
				{
					ConsoleBufferPush( pPrefixText );	// TODO: Add a ": " separator

#if _DEBUG
					LogOutput( "Filename.length.1: %" SIZE_T_FMT "\n", len1 );
					LogOutput( "Filename.length.2: %" SIZE_T_FMT "\n", len2 );
					OutputDebugString( sPaletteFilePath.c_str() );
#endif
					// File path is too long
					// TODO: Need to split very long path names
					char text[CONSOLE_WIDTH * 2] = "";
					strncpy( text, sPaletteFilePath.c_str(), CONSOLE_WIDTH );
					ConsoleBufferPush( text );	// TODO: Switch ConsoleBufferPush() to ConsoleBufferPushFormat()
				}
				else
				{
					ConsoleBufferPushFormat( "%s: %s", pPrefixText, sPaletteFilePath.c_str() );
				}
			}
	};

	class Swizzle32
	{
		public:
			static void RGBAswapBGRA( size_t nSize, const uint8_t *pSrc, uint8_t *pDst ) // Note: pSrc and pDst _may_ alias; code handles this properly
			{
				const uint8_t* pEnd = pSrc + nSize;
				while ( pSrc < pEnd )
				{
					const uint8_t r = pSrc[2];
					const uint8_t g = pSrc[1];
					const uint8_t b = pSrc[0];
					const uint8_t a = 255; // Force A=1, 100% opacity; as pSrc[3] might not be 255

					*pDst++ = r;
					*pDst++ = g;
					*pDst++ = b;
					*pDst++ = a;
					 pSrc  += 4;
				}
			}

			static void ABGRswizzleBGRA( size_t nSize, const uint8_t *pSrc, uint8_t *pDst ) // Note: pSrc and pDst _may_ alias; code handles this properly
			{
				const uint8_t* pEnd = pSrc + nSize;
				while ( pSrc < pEnd )
				{
					const uint8_t r = pSrc[3];
					const uint8_t g = pSrc[2];
					const uint8_t b = pSrc[1];
					const uint8_t a = 255; // Force A=1, 100% opacity; as pSrc[3] might not be 255

					*pDst++ = b;
					*pDst++ = g;
					*pDst++ = r;
					*pDst++ = a;
					 pSrc  += 4;
				}
			}
#if 0
			static void ABGRswizzleRGBA( size_t nSize, const uint8_t *pSrc, uint8_t *pDst ) // Note: pSrc and pDst _may_ alias; code handles this properly
			{
				const uint8_t* pEnd = pSrc + nSize;
				while ( pSrc < pEnd )
				{
					const uint8_t r = pSrc[3];
					const uint8_t g = pSrc[2];
					const uint8_t b = pSrc[1];
					const uint8_t a = 255; // Force A=1, 100% opacity; as pSrc[3] might not be 255

					*pDst++ = r;
					*pDst++ = g;
					*pDst++ = b;
					*pDst++ = a;
					 pSrc  += 4;
				}
			}
#endif
	};

	class Transpose64x1
	{
		public:
			static void transposeTo64x256( const uint8_t *pSrc, uint8_t *pDst )
			{
				const uint32_t nBPP = 4; // bytes per pixel

				// Expand y from 1 to 256 rows
				const size_t nBytesPerScanLine = 16 * 4 * nBPP; // 16 colors * 4 phases
				for ( int y = 0; y < 256; y++ )
					memcpy( pDst + y*nBytesPerScanLine, pSrc, nBytesPerScanLine );
			}
	};

	// Transpose from 16x1 to 4096x16
	class Transpose16x1
	{
		public:

/*
		.Column
		.   Phases 0..3
		. X P0 P1 P2 P3
		. 0  0  0  0  0
		. 1  1  8  4  2
		. 2  2  1  8  4
		. 3  3  9  C  6
		. 4  4  2  1  8
		. 5  5  A  5  A
		. 6  6  3  9  C
		. 7  7  B  D  E
		. 8  8  4  2  1
		. 9  9  C  6  3
		. A  A  5  A  5
		. B  B  D  E  7
		. C  C  6  3  9
		. D  D  E  7  B
		. E  E  7  B  D
		. F  F  F  F  F
		.
		.    1  2  4  8  Delta
*/
			static void transposeTo64x1( const uint8_t *pSrc, uint8_t *pDst )
			{
				const uint32_t *pPhase0 = (uint32_t*) pSrc;
				/* */ uint32_t *pTmp    = (uint32_t*) pDst;

#if 1 // Loop
				// Expand x from 16 colors (single phase) to 64 colors (16 * 4 phases)
				for ( int iPhase = 0; iPhase < 4; iPhase++ )
				{
					int phase = iPhase;
					if (iPhase == 1) phase = 3;
					if (iPhase == 3) phase = 1;
					int mul = (1 << phase); // Mul: *1 *8 *4 *2

					for ( int iDstX = 0; iDstX < 16; iDstX++ )
					{
						int iSrcX = (iDstX * mul) % 15; // Delta: +1 +2 +4 +8

						if (iDstX == 15)
							iSrcX = 15;
#if 0 // _DEBUG
						LogOutput( "[ %X ] = [ %X ]\n", iDstX, iSrcX );
#endif
						pTmp[ iDstX + 16*iPhase ] = pPhase0[ iSrcX ];
					}
				}
#else // Manual Loop unrolled
				const uint32_t nBPP = 4; // bytes per pixel

				const size_t nBytesPerScanLine = 16 * 4 * nBPP; // 16 colors * 4 phases
				memcpy( pDst, pSrc, nBytesPerScanLine );

				int iPhase = 1;
				int iDstX  = iPhase * 16;
				pTmp[ iDstX + 0x0 ] = pPhase0[ 0x0 ];
				pTmp[ iDstX + 0x1 ] = pPhase0[ 0x8 ];
				pTmp[ iDstX + 0x2 ] = pPhase0[ 0x1 ];
				pTmp[ iDstX + 0x3 ] = pPhase0[ 0x9 ];
				pTmp[ iDstX + 0x4 ] = pPhase0[ 0x2 ];
				pTmp[ iDstX + 0x5 ] = pPhase0[ 0xA ];
				pTmp[ iDstX + 0x6 ] = pPhase0[ 0x3 ];
				pTmp[ iDstX + 0x7 ] = pPhase0[ 0xB ];
				pTmp[ iDstX + 0x8 ] = pPhase0[ 0x4 ];
				pTmp[ iDstX + 0x9 ] = pPhase0[ 0xC ];
				pTmp[ iDstX + 0xA ] = pPhase0[ 0x5 ];
				pTmp[ iDstX + 0xB ] = pPhase0[ 0xD ];
				pTmp[ iDstX + 0xC ] = pPhase0[ 0x6 ];
				pTmp[ iDstX + 0xD ] = pPhase0[ 0xE ];
				pTmp[ iDstX + 0xE ] = pPhase0[ 0x7 ];
				pTmp[ iDstX + 0xF ] = pPhase0[ 0xF ];

				iPhase = 2;
				iDstX  = iPhase * 16;
				pTmp[ iDstX + 0x0 ] = pPhase0[ 0x0 ];
				pTmp[ iDstX + 0x1 ] = pPhase0[ 0x4 ];
				pTmp[ iDstX + 0x2 ] = pPhase0[ 0x8 ];
				pTmp[ iDstX + 0x3 ] = pPhase0[ 0xC ];
				pTmp[ iDstX + 0x4 ] = pPhase0[ 0x1 ];
				pTmp[ iDstX + 0x5 ] = pPhase0[ 0x5 ];
				pTmp[ iDstX + 0x6 ] = pPhase0[ 0x9 ];
				pTmp[ iDstX + 0x7 ] = pPhase0[ 0xD ];
				pTmp[ iDstX + 0x8 ] = pPhase0[ 0x2 ];
				pTmp[ iDstX + 0x9 ] = pPhase0[ 0x6 ];
				pTmp[ iDstX + 0xA ] = pPhase0[ 0xA ];
				pTmp[ iDstX + 0xB ] = pPhase0[ 0xE ];
				pTmp[ iDstX + 0xC ] = pPhase0[ 0x3 ];
				pTmp[ iDstX + 0xD ] = pPhase0[ 0x7 ];
				pTmp[ iDstX + 0xE ] = pPhase0[ 0xB ];
				pTmp[ iDstX + 0xF ] = pPhase0[ 0xF ];

				iPhase = 3;
				iDstX  = iPhase * 16;
				pTmp[ iDstX + 0x0 ] = pPhase0[ 0x0 ];
				pTmp[ iDstX + 0x1 ] = pPhase0[ 0x2 ];
				pTmp[ iDstX + 0x2 ] = pPhase0[ 0x4 ];
				pTmp[ iDstX + 0x3 ] = pPhase0[ 0x6 ];
				pTmp[ iDstX + 0x4 ] = pPhase0[ 0x8 ];
				pTmp[ iDstX + 0x5 ] = pPhase0[ 0xA ];
				pTmp[ iDstX + 0x6 ] = pPhase0[ 0xC ];
				pTmp[ iDstX + 0x7 ] = pPhase0[ 0xE ];
				pTmp[ iDstX + 0x8 ] = pPhase0[ 0x1 ];
				pTmp[ iDstX + 0x9 ] = pPhase0[ 0x3 ];
				pTmp[ iDstX + 0xA ] = pPhase0[ 0x5 ];
				pTmp[ iDstX + 0xB ] = pPhase0[ 0x7 ];
				pTmp[ iDstX + 0xC ] = pPhase0[ 0x9 ];
				pTmp[ iDstX + 0xD ] = pPhase0[ 0xB ];
				pTmp[ iDstX + 0xE ] = pPhase0[ 0xD ];
				pTmp[ iDstX + 0xF ] = pPhase0[ 0xF ];
#endif
			}

		/*
		.   Source layout = 16x1 @ 32-bit
		.   |                                    phase 0                                    |
		.   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA| row 0
		.   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
		.    \ 0/ \ 1/ \ 2/ \ 3/ \ 4/ \ 5/ \ 6/ \ 7/ \ 8/ \ 9/ \ A/ \ B/ \ C/ \ D/ \ E/ \ F/
		.
		.   |<----------------------------------- 16 px ----------------------------------->|
		.     64 byte
		.
		.   Destination layout = 4096x4 @ 32-bit
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 0
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 1
		.   +----+----+----+----+----|
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 2
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 3
		.   +----+----+----+----+----+
		.    0    1    2         4095  column
		*/
/*
			static void transposeFrom16x1( const uint8_t *pSrc, uint8_t *pDst )
			{
				const uint8_t *pTmp = pSrc;
				const uint32_t nBPP = 4; // bytes per pixel

				for ( int x = 0; x < 16; x++ )
				{
					pTmp = pSrc + (x * nBPP); // dst is 16-px column
					for ( int y = 0; y < 256; y++ )
					{
							*pDst++ = pTmp[0];
							*pDst++ = pTmp[1];
							*pDst++ = pTmp[2];
							*pDst++ = pTmp[3];
					}
				}

				// we duplicate phase 0 a total of 4 times
				const size_t nBytesPerScanLine = 4096 * nBPP;
				for ( int iPhase = 1; iPhase < 4; iPhase++ )
					memcpy( pDst + iPhase*nBytesPerScanLine, pDst, nBytesPerScanLine );
			}
*/
	};

	class Transpose4096x4
	{
		/*
		.   Source layout = 4096x4 @ 32-bit
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 0
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 1
		.   +----+----+----+----+----|
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 2
		.   +----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|... |BGRA| phase 3
		.   +----+----+----+----+----+
		.    0    1    2         4095  column
		.
		.   Destination layout = 64x256 @ 32-bit
		.   | phase 0 | phase 1 | phase 2 | phase 3 |
		.   +----+----+----+----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA| row 0
		.   +----+----+----+----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA| row 1
		.   +----+----+----+----+----+----+----+----+
		.   |... |... |... |... |... |... |... |... |
		.   +----+----+----+----+----+----+----+----+
		.   |BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA|BGRA| row 255
		.   +----+----+----+----+----+----+----+----+
		.    \ 16 px / \ 16 px / \ 16 px / \ 16 px  / = 64 pixels
		.     64 byte   64 byte   64 byte   64 byte
		.
		.Column
		.   Phases 0..3
		. X P0 P1 P2 P3
		. 0  0  0  0  0
		. 1  1  8  4  2
		. 2  2  1  8  4
		. 3  3  9  C  6
		. 4  4  2  1  8
		. 5  5  A  5  A
		. 6  6  3  9  C
		. 7  7  B  D  E
		. 8  8  4  2  1
		. 9  9  C  6  3
		. A  A  5  A  5
		. B  B  D  E  7
		. C  C  6  3  9
		. D  D  E  7  B
		. E  E  7  B  D
		. F  F  F  F  F
		.
		.    1  2  4  8  Delta
		*/

		public:
			static void transposeTo64x256( const uint8_t *pSrc, uint8_t *pDst )
			{
				/* */ uint8_t *pTmp = pDst;
				const uint32_t nBPP = 4; // bytes per pixel

				for ( int iPhase = 0; iPhase < 4; iPhase++ )
				{
					pDst = pTmp + (iPhase * 16 * nBPP); // dst is 16-px column

					for ( int x = 0; x < 4096/16; x++ ) // 4096px/16 px = 256 columns
					{
						for ( int i = 0; i < 16*nBPP; i++ ) // 16 px, 32-bit
							*pDst++ = *pSrc++;

						pDst -= (16*nBPP);
						pDst += (64*nBPP); // move to next scan line
					}
				}
			}

			static void transposeFrom64x256( const uint8_t *pSrc, uint8_t *pDst )
			{
				const uint8_t *pTmp = pSrc;
				const uint32_t nBPP = 4; // bytes per pixel

				for ( int iPhase = 0; iPhase < 4; iPhase++ )
				{
					pSrc = pTmp + (iPhase * 16 * nBPP); // src is 16-px column
					for ( int y = 0; y < 256; y++ )
					{
						for ( int i = 0; i < 16*nBPP; i++ ) // 16 px, 32-bit
							*pDst++ = *pSrc++;

						pSrc -= (16*nBPP);
						pSrc += (64*nBPP); // move to next scan line
					}
				}
			}
	};

	bool bColorTV = (GetVideo().GetVideoType() == VT_COLOR_TV);

	uint32_t* pChromaTable = NTSC_VideoGetChromaTable( false, bColorTV );

	//uint8_t* pTmp = (uint8_t*) pChromaTable;
	//*pTmp++ = 0xFF; // b
	//*pTmp++ = 0x00; // g
	//*pTmp++ = 0x00; // r
	//*pTmp++ = 0xFF; // a

	if (nFound)
	{
		if (iParam == PARAM_RESET)
		{
			NTSC_VideoInitChroma();
			ConsoleBufferPush( " Resetting NTSC palette." );
		}
		else
		if (iParam == PARAM_SAVE)
		{
			FILE *pFile = fopen( sPaletteFilePath.c_str(), "w+b" );
			if ( pFile )
			{
				size_t nWrote = 0;
				uint8_t *pSwizzled = new uint8_t[ g_nChromaSize ];

				if ( iFileType == TYPE_BMP )
				{
					// need to save 32-bit bpp as 24-bit bpp
					// VideoSaveScreenShot()
					Transpose4096x4::transposeTo64x256( (uint8_t*) pChromaTable, pSwizzled );

					// Write BMP header
					WinBmpHeader_t bmp, *pBmp = &bmp;
					GetVideo().Video_SetBitmapHeader( pBmp, 64, 256, 32 );
					fwrite( pBmp, sizeof( WinBmpHeader_t ), 1, pFile );
				}
				else
				{
					// RAW has no header
					Swizzle32::RGBAswapBGRA( g_nChromaSize, (uint8_t*) pChromaTable, pSwizzled );
				}

				nWrote = fwrite( pSwizzled, g_nChromaSize, 1, pFile );
				fclose( pFile );
				delete [] pSwizzled;

				if (nWrote == 1)
				{
					ConsoleFilename::update( "Saved" );
				}
				else
					ConsoleBufferPush(  "Error saving."  );
			}
			else
			{
					ConsoleFilename::update( "File" );
					ConsoleBufferPush( "Error couldn't open file for writing."  );
			}
		}
		else
		if (iParam == PARAM_LOAD)
		{
			std::string sStatusText;

			FILE *pFile = fopen( sPaletteFilePath.c_str(), "rb" );
			if ( pFile )
			{
				sStatusText = "Loaded";

				// Get File Size
				size_t  nFileSize  = _GetFileSize( pFile );
				uint8_t *pSwizzled = new uint8_t[ g_nChromaSize ];
				bool     bSwizzle  = true;

				WinBmpHeader4_t bmp = { 0 };

				if ( iFileType == TYPE_BMP )
				{
					fread( &bmp, sizeof( WinBmpHeader4_t ), 1, pFile );
					fseek( pFile, bmp.nOffsetData, SEEK_SET );

					if (bmp.nBitsPerPixel != 32)
					{
						sStatusText = "Bitmap not 32-bit RGBA";
						goto _error;
					}

					if (bmp.nOffsetData > nFileSize)
					{
						sStatusText = "Bad BITMAP: Data > file size !?";
						goto _error;
					}

					if ( !
					(  ((bmp.nWidthPixels  == 64 ) && (bmp.nHeightPixels == 256))
					|| ((bmp.nWidthPixels  == 64 ) && (bmp.nHeightPixels == 1))
					|| ((bmp.nWidthPixels  == 16 ) && (bmp.nHeightPixels == 1))
					))
					{
						sStatusText = "Bitmap not 64x256, 64x1, or 16x1";
						goto _error;
					}

					if (bmp.nStructSize == 0x28)
					{
						if ( bmp.nCompression == 0) // BI_RGB mode
							bSwizzle = false;
					}
					else // 0x7C version4 bitmap
					{
						if ( bmp.nCompression == 3 ) // BI_BITFIELDS
						{
							if ((bmp.nRedMask   == 0xFF000000 ) // Gimp writes in ABGR order
							&&  (bmp.nGreenMask == 0x00FF0000 )
							&&  (bmp.nBlueMask  == 0x0000FF00 ))
								bSwizzle = true;
							if ((bmp.nRedMask   == 0x00FF0000 ) // Gimp 32 Bits, X8 R8 G8 B8
							&&  (bmp.nGreenMask == 0x0000FF00 )
							&&  (bmp.nBlueMask  == 0x000000FF ))
								bSwizzle = false;
						}
					}
				}
				else
					if ( nFileSize != g_nChromaSize )
					{
						sStatusText = StrFormat( "Raw size != %d", 64 * 256 * 4 );
						goto _error;
					}


				fread( pSwizzled, g_nChromaSize, 1, pFile );

				if ( iFileType == TYPE_BMP )
				{

					if (bmp.nHeightPixels == 1)
					{
						uint8_t *pTemp64x256 = new uint8_t[ 64 * 256 * 4 ];
						memset( pTemp64x256, 0, g_nChromaSize );

						//Transpose16x1::transposeFrom16x1( pSwizzled, (uint8_t*) pChromaTable );

						if (bmp.nWidthPixels == 16)
						{
							Transpose16x1::transposeTo64x1( pSwizzled, pTemp64x256 );
							Transpose64x1::transposeTo64x256( pTemp64x256, pTemp64x256 );
						}
						else
						if (bmp.nWidthPixels == 64)
							Transpose64x1::transposeTo64x256( pSwizzled, pTemp64x256 );

						Transpose4096x4::transposeFrom64x256( pTemp64x256, (uint8_t*) pChromaTable );

						delete [] pTemp64x256;
					}
					else
						Transpose4096x4::transposeFrom64x256( pSwizzled, (uint8_t*) pChromaTable );

					if ( bSwizzle )
						Swizzle32::ABGRswizzleBGRA( g_nChromaSize, (uint8_t*) pChromaTable, (uint8_t*) pChromaTable );
				}
				else
					Swizzle32::RGBAswapBGRA( g_nChromaSize, pSwizzled, (uint8_t*) pChromaTable );

_error:
				fclose( pFile );
				delete [] pSwizzled;
			}
			else
			{
				sStatusText = "File: ";
				ConsoleBufferPush( "Error couldn't open file for reading." );
			}

			ConsoleFilename::update( sStatusText.c_str() );
		}
		else
			return HelpLastCommand();
	}
//	else

	return ConsoleUpdate();
}


//===========================================================================
int CmdTextSave (int nArgs)
{
	// Save the TEXT1 40-colomn to text file (Default: AppleWin_Text40.txt"
	// TSAVE ["Filename"]
	// TSAVE ["Filename"]
	//       1           
	if (nArgs > 1)
		return Help_Arg_1( CMD_TEXT_SAVE );

	bool bHaveFileName = false;

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
		bHaveFileName = true;

	char  *pText;
	size_t nSize = Util_GetTextScreen( pText );

	std::string sLoadSaveFilePath = g_sCurrentDir; // g_sProgramDir

	if ( bHaveFileName )
		g_sMemoryLoadSaveFileName = g_aArgs[ 1 ].sArg;
	else
	{
		if ( GetVideo().VideoGetSW80COL() )
			g_sMemoryLoadSaveFileName = "AppleWin_Text80.txt";
		else
			g_sMemoryLoadSaveFileName = "AppleWin_Text40.txt";
	}

	sLoadSaveFilePath += g_sMemoryLoadSaveFileName;

	FILE *hFile = fopen( sLoadSaveFilePath.c_str(), "rb" );
	if (hFile)
	{
		ConsoleBufferPush( "Warning: File already exists.  Overwriting." );
		fclose( hFile );
	}

	hFile = fopen( sLoadSaveFilePath.c_str(), "wb" );
	if (hFile)
	{
		size_t nWrote = fwrite( pText, nSize, 1, hFile );
		if (nWrote == 1)
		{
			ConsoleBufferPushFormat( "Saved: %s", g_sMemoryLoadSaveFileName.c_str() );
		}
		else
		{
			ConsoleBufferPush( "Error saving." );
		}
		fclose( hFile );
	}
	else
	{
		ConsoleBufferPush( "Error opening file." );
	}

	return ConsoleUpdate();
}

//===========================================================================
int _SearchMemoryFind (
	MemorySearchValues_t vMemorySearchValues,
	WORD nAddressStart,
	WORD nAddressEnd )
{
	int nFound = 0;

	g_vMemorySearchResults.clear();
	g_vMemorySearchResults.push_back( NO_6502_TARGET );

	uint32_t nAddress;	// NB. can't be uint16_t, since need to count up to 0x10000 if nAddressEnd is 0xFFFF
	for ( nAddress = nAddressStart; nAddress <= nAddressEnd; nAddress++ )
	{
		bool bMatchAll = true;

		uint32_t nAddress2 = nAddress;

		int nMemBlocks = vMemorySearchValues.size();
		for ( int iBlock = 0; iBlock < nMemBlocks; iBlock++, nAddress2++ )
		{
			MemorySearch_t ms = vMemorySearchValues.at( iBlock );
			ms.m_bFound = false;
		
			if ((ms.m_iType == MEM_SEARCH_BYTE_EXACT    ) || 
				(ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT) ||
				(ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT ))
			{
				BYTE nTarget = ReadByteFromMemory(nAddress2);
	
				if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
					nTarget &= 0x0F;

				if (ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT)
					nTarget &= 0xF0;
				
				if (ms.m_nValue == nTarget)
				{ // ms.m_nAddress = nAddress2;
					ms.m_bFound = true;
					continue;
				}
				else
				{
					bMatchAll = false;
					break;
				}
			}
			else
			if (ms.m_iType == MEM_SEARCH_BYTE_1_WILD)
			{
				// match by definition
			}
			else
			{
				// start 2ndary search
				// if next block matches, then this block matches (since we are wild)
				if ((iBlock + 1) == nMemBlocks) // there is no next block, hence we match
					continue;

				for (uint32_t nAddress3 = nAddress2; nAddress3 < nAddressEnd; nAddress3++ )
				{
					if ((ms.m_iType == MEM_SEARCH_BYTE_EXACT    ) || 
						(ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT) ||
						(ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT ))
					{
						BYTE nTarget = ReadByteFromMemory(nAddress3);
			
						if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
							nTarget &= 0x0F;

						if (ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT)
							nTarget &= 0xF0;
						
						if (ms.m_nValue == nTarget)
						{
							nAddress2 = nAddress3;
							continue;
						}
						else
						{
							bMatchAll = false;
							break;
						}
					}

				}
			}
		}

		if (bMatchAll)
		{
			nFound++;

			// Save the search result
			g_vMemorySearchResults.push_back( nAddress );
		}
	}

	return nFound;
}


//===========================================================================
Update_t _SearchMemoryDisplay (int nArgs)
{
	int const nFound = g_vMemorySearchResults.size() - 1;

	if (nFound > 0)
	{
		std::string sMatches;

		int iFound = 1;
		while (iFound <= nFound)
		{
			WORD const nAddress = g_vMemorySearchResults.at( iFound );

			// 2.6.2.17 Search Results: The n'th result now using correct color (was command, now number decimal)
			// BUGFIX: 2.6.2.32 n'th Search results were being displayed in dec, yet parser takes hex numbers. i.e. SH D000:FFFF A9 00
			// Intentional default instead of CHC_ARG_SEP for better readability
			// 2.6.2.16 Fixed: Search Results: The hex specify for target address results now colorized properly
			// 2.6.2.15 Fixed: Search Results: Added space between results for better readability

			// FIXME: Color is DEC whereas the format is "%X". What's the real intention?
			std::string sResult = StrFormat( CHC_NUM_DEC "%02X" CHC_DEFAULT ":" CHC_ARG_SEP "$" CHC_ADDRESS "%04X ",
											 iFound, nAddress );

			// Fit on same line?
			if ((sMatches.length() + sResult.length()) > (size_t(g_nConsoleDisplayWidth) - 1)) // CONSOLE_WIDTH
			{
				//ConsoleDisplayPush( sMatches.c_str() );
				ConsolePrint( sMatches.c_str() );
				sMatches = sResult;
			}
			else
			{
				sMatches += sResult;
			}

			iFound++;
		}

		ConsolePrint( sMatches.c_str() );
	}

	//ConsoleDisplayPushFormat( "Total: %d  (#$%04X)", nFound, nFound );

	ConsolePrintFormat( CHC_USAGE "Total" CHC_DEFAULT ": " CHC_NUM_DEC "%d  " CHC_ARG_SEP "(" CHC_ARG_SEP "#$" CHC_NUM_HEX "%04X" CHC_ARG_SEP ")",
						nFound /*dec*/, nFound /*hex*/ );

	// g_vMemorySearchResults is cleared in DebugEnd()

//	return UPDATE_CONSOLE_DISPLAY;
	return ConsoleUpdate();
}


//===========================================================================
Update_t _CmdMemorySearch (int nArgs, bool bTextIsAscii = true )
{
	WORD nAddressStart = 0;
	WORD nAddress2   = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;

	RangeType_t eRange;
	eRange = Range_Get( nAddressStart, nAddress2 );

//	if (eRange == RANGE_MISSING_ARG_2)
	if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen))
		return ConsoleDisplayError( "Error: Missing address separator (comma or colon)" );

	int iArgFirstByte = 4;
	int iArg;

	MemorySearchValues_t vMemorySearchValues;
	MemorySearch_e       tLastType = MEM_SEARCH_BYTE_N_WILD;
	
	// Get search "string"
	Arg_t *pArg = & g_aArgs[ iArgFirstByte ];
	
	for (iArg = iArgFirstByte; iArg <= nArgs; iArg++, pArg++ )
	{
		WORD nTarget = pArg->nValue;

		MemorySearch_t ms;
		ms.m_nValue = nTarget & 0xFF;
		ms.m_iType = MEM_SEARCH_BYTE_EXACT;
		ms.m_bFound = false;

		if (nTarget > 0xFF) // searching for 16-bit address
		{
			vMemorySearchValues.push_back( ms );
			ms.m_nValue = (nTarget >> 8);

			tLastType = ms.m_iType;
		}
		else
		{
			char *pByte = pArg->sArg;

			if (pArg->bType & TYPE_QUOTED_1)
			{
				// Convert string to hex byte(s)
				int iChar = 0;
				int nChars = pArg->nArgLen;

				if (nChars)
				{
					ms.m_iType = MEM_SEARCH_BYTE_EXACT;
					ms.m_bFound = false;

					while (iChar < nChars)
					{
						ms.m_nValue = pArg->sArg[ iChar ];

						// Ascii (Low-Bit)
						// Apple (High-Bit)
//						if (! bTextIsAscii) // NOTE: Single quote chars is opposite hi-bit !!!
//							ms.m_nValue &= 0x7F;
//						else
							ms.m_nValue |= 0x80;

						// last char is handle in common case below
						iChar++; 
						if (iChar < nChars)
							vMemorySearchValues.push_back( ms );
					}
				}
			}
			else
			if (pArg->bType & TYPE_QUOTED_2)
			{
				// Convert string to hex byte(s)
				int iChar = 0;
				int nChars = pArg->nArgLen;

				if (nChars)
				{
					ms.m_iType = MEM_SEARCH_BYTE_EXACT;
					ms.m_bFound = false;

					while (iChar < nChars)
					{
						ms.m_nValue = pArg->sArg[ iChar ];

						// Ascii (Low-Bit)
						// Apple (High-Bit)
//						if (bTextIsAscii)
							ms.m_nValue &= 0x7F;
//						else
//							ms.m_nValue |= 0x80;

						iChar++; // last char is handle in common case below
						if (iChar < nChars) 
							vMemorySearchValues.push_back( ms );
					}
				}
			}
			else
			{
				// must be numeric .. make sure not too big
				if (pArg->nArgLen > 2)
				{
					vMemorySearchValues.clear();
					return HelpLastCommand();
				}

				if (pArg->nArgLen == 1)
				{
					if (pByte[0] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						ms.m_iType = MEM_SEARCH_BYTE_1_WILD;
					}
				}
				else
				{
					if (pByte[0] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						ms.m_iType = MEM_SEARCH_NIB_LOW_EXACT;
						ms.m_nValue = pArg->nValue & 0x0F;
					}

					if (pByte[1] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
						{
							ms.m_iType = MEM_SEARCH_BYTE_N_WILD;
						}
						else
						{
							ms.m_iType = MEM_SEARCH_NIB_HIGH_EXACT;
							ms.m_nValue = (pArg->nValue << 4) & 0xF0;
						}
					}
				}
			}
		}

		// skip over multiple byte_wild, since they are redundent
		// xx ?? ?? xx
		//       ^
		//       redundant
		if ((tLastType == MEM_SEARCH_BYTE_N_WILD) && (ms.m_iType == MEM_SEARCH_BYTE_N_WILD))
			continue;

		vMemorySearchValues.push_back( ms );
		tLastType = ms.m_iType;
	}

	_SearchMemoryFind( vMemorySearchValues, nAddressStart, nAddressEnd );
	vMemorySearchValues.clear();

	return _SearchMemoryDisplay();
}


//===========================================================================
Update_t CmdMemorySearch (int nArgs)
{
	// S address,length # [,#]
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );

	return UPDATE_CONSOLE_DISPLAY;
}


// Search for ASCII text (no Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchAscii (int nArgs)
{
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );
}

// Search for Apple text (Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchApple (int nArgs)
{
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, false );
}

//===========================================================================
Update_t CmdMemorySearchHex (int nArgs)
{
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );
}


// Registers ______________________________________________________________________________________


//===========================================================================
Update_t CmdRegisterSet (int nArgs)
{
	if (nArgs < 2) // || ((g_aArgs[2].sArg[0] != '0') && !g_aArgs[2].nValue))
	{
		return Help_Arg_1( CMD_REGISTER_SET );
	}
	else
	{
		char *pName = g_aArgs[1].sArg;
		int iParam;
		if (FindParam( pName, MATCH_EXACT, iParam, _PARAM_REGS_BEGIN, _PARAM_REGS_END ))
		{
			int iArg = 2;
			if (g_aArgs[ iArg ].eToken == TOKEN_EQUAL)
				iArg++;

			if (iArg > nArgs)
				return Help_Arg_1( CMD_REGISTER_SET );
				
			BYTE b = (BYTE)(g_aArgs[ iArg ].nValue & 0xFF);
			WORD w = (WORD)(g_aArgs[ iArg ].nValue & 0xFFFF);

			switch (iParam)
			{
				case PARAM_REG_A : regs.a  = b;    break;
				case PARAM_REG_PC: regs.pc = w; g_nDisasmCurAddress = regs.pc; DisasmCalcTopBotAddress(); break;
				case PARAM_REG_SP: regs.sp = b | 0x100; break;
				case PARAM_REG_X : regs.x  = b; break;
				case PARAM_REG_Y : regs.y  = b; break;
				case PARAM_FLAGS : regs.ps = b; break;
				default:        return Help_Arg_1( CMD_REGISTER_SET );
			}
		}
	}
	
//	g_nDisasmCurAddress = regs.pc;
//	DisasmCalcTopBotAddress();

	return UPDATE_ALL; // 1
}


// Output _________________________________________________________________________________________


//===========================================================================
Update_t CmdOutputCalc (int nArgs)
{
	const int nBits = 8;

	if (! nArgs)
		return Help_Arg_1( CMD_OUTPUT_CALC );

	WORD nAddress = g_aArgs[1].nValue;

	bool bHi = false;
	bool bLo = false;
	char c = FormatChar4Font( (BYTE) nAddress, &bHi, &bLo );
	bool bParen = bHi || bLo;

	int nBit = 0;
	int iBit = 0;
	for ( iBit = 0; iBit < nBits; iBit++ )
	{
		bool bSet = (nAddress >> iBit) & 1;
		if (bSet)
			nBit |= (1 << (iBit * 4)); // 4 bits per hex digit
	}

	// TODO: Colorize output
	//    CHC_NUM_HEX
	//    CHC_NUM_BIN -- doesn't exist, use CHC_INFO
	//    CHC_NUM_DEC
	//    CHC_ARG_
	//    CHC_STRING
	std::string sText = StrFormat( "$%04X  0z%08X  %5d  '%c' ", nAddress, nBit, nAddress, c );

	if (bParen)
		sText += '(';

	if (bHi && bLo)
		sText += "High Ctrl";
	else
	if (bHi)
		sText += "High";
	else
	if (bLo)
		sText += "Ctrl";

	if (bParen)
		sText += ')';

	ConsoleBufferPush( sText.c_str() );

// If we colorize then w must also guard against character ouput $60
//	ConsolePrint( sText.c_str() );

	return ConsoleUpdate();
}


//===========================================================================
Update_t CmdOutputEcho (int nArgs)
{
	if (g_aArgs[1].bType & TYPE_QUOTED_2)
	{
		ConsoleDisplayPush( g_aArgs[1].sArg );
	}
	else
	{
		const char *pText = g_pConsoleFirstArg; // ConsoleInputPeek();
		if (pText)
		{
			ConsoleDisplayPush( pText );
		}
	}

	return ConsoleUpdate();
}


enum PrintState_e
{	  PS_LITERAL
	, PS_TYPE
	, PS_ESCAPE
	, PS_NEXT_ARG_BIN
	, PS_NEXT_ARG_HEX
	, PS_NEXT_ARG_DEC
	, PS_NEXT_ARG_CHR
};

struct PrintFormat_t
{
	int nValue;
	int eType;
};


//===========================================================================
Update_t CmdOutputPrint (int nArgs)
{
	// PRINT "A:",A," X:",X
	// Removed: PRINT "A:%d",A," X: %d",X
	std::string sText;

	if (nArgs <= 0)
		goto _Help;

	for ( int iArg = 1; iArg <= nArgs; iArg++ )
	{
		sText += (!!(g_aArgs[ iArg ].bType & TYPE_QUOTED_2))
			? g_aArgs[ iArg ].sArg
			: WordToHexStr( g_aArgs[ iArg ].nValue );

		iArg++;
		//if (iArg > nArgs)
		//	goto _Help;
		if (iArg <= nArgs && g_aArgs[ iArg ].eToken != TOKEN_COMMA)
			goto _Help;
	}

	if (!sText.empty())
		ConsoleBufferPush( sText.c_str() );

	return ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_OUTPUT_PRINT );
}


//===========================================================================
Update_t CmdOutputPrintf (int nArgs)
{
	// PRINTF "A:%d X:%d",A,X
	// PRINTF "Hex:%x  Dec:%d  Bin:%z",A,A,A

	if (! nArgs)
		return Help_Arg_1( CMD_OUTPUT_PRINTF );

	std::vector<Arg_t> aValues;

	for ( int iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[ iArg ].bType & TYPE_QUOTED_2)
			continue;
		else
		if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
			continue;
		else
		{
			Arg_t entry;
			entry.nValue = g_aArgs[ iArg ].nValue;
			aValues.push_back( entry );
		}
	}

	std::string sText;
	sText.reserve(CONSOLE_WIDTH);

	PrintState_e eThis = PS_LITERAL;
	int nWidth = 0;

	const size_t nParamValues = aValues.size();
	size_t iValue = 0;

	for ( int iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[ iArg ].bType & TYPE_QUOTED_2)
		{
			for ( const char* cp = g_aArgs[iArg].sArg, *ep = cp + strlen(g_aArgs[iArg].sArg); cp < ep; ++cp )
			{
				const char c = *cp;
				switch ( eThis )
				{
					case PS_LITERAL:
						switch ( c )
						{
							case '\\':
								eThis = PS_ESCAPE;
								break;
							case '%':
								eThis = PS_TYPE;
								break;
							default:
								sText += c;
								break;
						}
						break;
					case PS_ESCAPE:
						switch ( c )
						{
							case 'n':
							case 'r':
								eThis = PS_LITERAL;
								sText += '\n';
								break;
						}
						break;
					case PS_TYPE:
						if (iValue >= nParamValues)
						{
							ConsoleBufferPushFormat( "Error: Missing value arg: %" SIZE_T_FMT, iValue + 1 );
							return ConsoleUpdate();
						}
						switch ( c )
						{
							case 'X':
							case 'x': // PS_NEXT_ARG_HEX
								sText += WordToHexStr( aValues[ iValue ].nValue );
								iValue++;
								break;
							case 'D':
							case 'd': // PS_NEXT_ARG_DEC
								sText += StrFormat( "%d", aValues[ iValue ].nValue );
								iValue++;
								break;
							break;
							case 'Z':
							case 'z':
							{
								const int nValue = aValues[ iValue ].nValue;
								if (!nWidth)
									nWidth = 8;
								int nBits = nWidth;
								while (nBits-- > 0)
								{
									sText += ((nValue >> nBits) & 1) ? '1' : '0';
								}
								iValue++;
								break;
							}
							case 'c': // PS_NEXT_ARG_CHR;
								sText += char(  aValues[ iValue ].nValue );
								iValue++;
								break;
							case '%':
							default:
								sText += c;
								break;
						}
						eThis = PS_LITERAL;
						break;
					default:
						break;
				}
			}
		}
		else
		if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
		{			
			iArg++;
			if (iArg > nArgs)
				goto _Help;
		}
		else
			goto _Help;
	}

	if (!sText.empty())
		ConsoleBufferPush( sText.c_str() );

	return ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_OUTPUT_PRINTF );
}


//===========================================================================
Update_t CmdOutputRun (int nArgs)
{
	g_bScriptReadOk = false;

	if (! nArgs)
		return Help_Arg_1( CMD_OUTPUT_RUN );

	if (nArgs != 1)
		return Help_Arg_1( CMD_OUTPUT_RUN );
	
	// Read in script
	// could be made global, to cache last run.
	// Opens up the possibility of:
	// CHEAT [ON | OFF] -> re-run script
	// with conditional logic
	// IF @ON ....
	MemoryTextFile_t script; 

	const std::string pFileName = g_aArgs[ 1 ].sArg;
	std::string sFileName;

	if (pFileName[0] == PATH_SEPARATOR || pFileName[1] == ':')	// NB. Any prefix quote has already been stripped
	{
		// Abs pathname
		sFileName = pFileName;
	}
	else
	{
		// Rel pathname
		sFileName = g_sCurrentDir + pFileName;
	}

	if (script.Read( sFileName ))
	{
		g_bScriptReadOk = true;

		ConsolePrintFormat("%sRun script: ", CHC_INFO);
		ConsolePrintFormat("%s%s", CHC_PATH, sFileName.c_str());	// Output pathname to console to indicate the script has been read

		int nLine = script.GetNumLines();

		Update_t bUpdateDisplay = UPDATE_NOTHING;	

		for ( int iLine = 0; iLine < nLine; iLine++ )
		{
			script.GetLine( iLine, g_pConsoleInput, CONSOLE_WIDTH-2 );
			g_nConsoleInputChars = strlen( g_pConsoleInput );
			bUpdateDisplay |= DebuggerProcessCommand( false );
		}
	}
	else
	{
		ConsolePrintFormat("%sCouldn't load script:", CHC_WARNING);
		ConsolePrintFormat("%s%s", CHC_STRING, sFileName.c_str());
	}

	return ConsoleUpdate();
}


// Source Level Debugging _________________________________________________________________________

//===========================================================================
bool BufferAssemblyListing ( const std::string & pFileName )
{
	bool bStatus = false; // true = loaded

	if (pFileName.empty())
		return bStatus;

	g_AssemblerSourceBuffer.Reset();
	g_AssemblerSourceBuffer.Read( pFileName );

	if (g_AssemblerSourceBuffer.GetNumLines())
	{
		g_bSourceLevelDebugging = true;
		bStatus = true;
	}

	return bStatus;
}


//===========================================================================
int FindSourceLine ( WORD nAddress )
{
	int iAddress = 0;
	int iLine = 0;
	int iSourceLine = NO_SOURCE_LINE;

//	iterate of <address,line>
//	probably should be sorted by address
//	then can do binary search
//	iSourceLine = g_aSourceDebug.find( nAddress );
#if 0 // _DEBUG
	{
		for (int i = 0; i < g_vSourceLines.size(); i++ )
		{
			LogOutput( "%d: %s\n", i, g_vSourceLines[ i ] );
		}
	}
#endif

	SourceAssembly_t::iterator iSource = g_aSourceDebug.begin();
	while (iSource != g_aSourceDebug.end() )
	{
		iAddress = iSource->first;
		iLine = iSource->second;

#if 0 // _DEBUG
		LogOutput( "%04X -> %d line\n", iAddress, iLine );
#endif

		if (iAddress == nAddress)
		{
			iSourceLine = iLine;
			break;
		}

		iSource++;
	}
	// not found

	return iSourceLine;
}

//===========================================================================
bool ParseAssemblyListing ( bool bBytesToMemory, bool bAddSymbols )
{
	bool bStatus = false; // true = loaded

	// Assembler source listing file:
	//
	// xxxx:_b1_[b2]_[b3]__n_[label]_[opcode]_[param]
//	char  sByte1[ 2 ];
//	char  sByte2[ 2 ];
//	char  sByte3[ 2 ];
//	char  sLineN[ W ];
	char sName[ MAX_SYMBOLS_LEN ];
//	char  sAsm  [ W ];
//	char  sParam[ W ];

	const int MAX_LINE = 256;
	char  sLine[ MAX_LINE ];
	char  sText[ MAX_LINE ];
//	char  sLabel[ MAX_LINE ];

	g_nSourceAssembleBytes = 0;
	g_nSourceAssemblySymbols = 0;

	const uint32_t INVALID_ADDRESS = _6502_MEM_END + 1;

	int nLines = g_AssemblerSourceBuffer.GetNumLines();
	for ( int iLine = 0; iLine < nLines; iLine++ )
	{
		g_AssemblerSourceBuffer.GetLine( iLine, sText, MAX_LINE - 1 );

		uint32_t nAddress = INVALID_ADDRESS;

		strcpy( sLine, sText );
		char *p = sLine;
		p = strstr( sLine, ":" );
		if (p)
		{
			*p = 0;
			//	sscanf( sLine, "%s %s %s %s %s %s %s %s", sAddr1, sByte1, sByte2, sByte3, sLineN, sLabel, sAsm, sParam );
			sscanf( sLine, "%X", &nAddress );

			if (nAddress >= INVALID_ADDRESS) // || (sName[0] == 0) )
				continue;

			if (bBytesToMemory)
			{
				char *pEnd = p + 1;				
				char *pStart;
				int  iByte;
				for (iByte = 0; iByte < 4; iByte++ ) // BUG: Some assemblers also put 4 bytes on a line
				{
					// xx xx xx
					// ^ ^
					// | |
					// | end
					// start
					pStart = pEnd + 1;
					pEnd = const_cast<char*>( SkipUntilWhiteSpace( pStart ));
					int nLen = (pEnd - pStart);
					if (nLen != 2)
					{
						break;
					}
					*pEnd = 0;
					if (TextIsHexByte( pStart ))
					{
						BYTE nByte = TextConvert2CharsToByte( pStart );
						WriteByteToMemory(((WORD)nAddress) + iByte, nByte);
					}
				}
				g_nSourceAssembleBytes += iByte;
			}

			g_aSourceDebug[ (WORD) nAddress ] = iLine; // g_nSourceAssemblyLines;
		}

		strcpy( sLine, sText );
		if (bAddSymbols)
		{
			// Add user symbol:          symbolname EQU $address
			//  or user symbol: address: symbolname DFB #bytes
 			char *pEQU = strstr( sLine, "EQU" ); // EQUal / EQUate
			char *pDFB = strstr( sLine, "DFB" ); // DeFine Byte
			char *pLabel = NULL;

			if (pEQU)
				pLabel = pEQU;
			if (pDFB)
				pLabel = pDFB;

			if (pLabel)
			{	
				char *pLabelEnd = pLabel - 1;
				pLabelEnd = const_cast<char*>( SkipWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] ));
				char * pLabelStart = NULL; // SkipWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] );
				if (pLabelEnd)
				{
					pLabelStart = const_cast<char*>( SkipUntilWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] ));
					pLabelEnd++;
					pLabelStart++;
					
					int nLen = pLabelEnd - pLabelStart;
					nLen = MIN( nLen, MAX_SYMBOLS_LEN );
					strncpy( sName, pLabelStart, nLen );
					sName[ nLen - 1 ] = 0;

					char *pAddressEQU = strstr( pLabel, "$" );
					char *pAddressDFB = strstr( sLine, ":" ); // Get address from start of line
					char *pAddress = NULL;

					if (pAddressEQU)
						pAddress = pAddressEQU + 1;
					if (pAddressDFB)
					{
						*pAddressDFB = 0;
						pAddress = sLine;
					}

					if (pAddress)
					{
						char *pAddressEnd;
						nAddress = (uint32_t) strtol( pAddress, &pAddressEnd, 16 );
						g_aSymbols[ SYMBOLS_SRC_2 ][ (WORD) nAddress] = sName;
						g_nSourceAssemblySymbols++;
					}
				}
			}
		}
	} // for

	bStatus = true;
	
	return bStatus;
}


//===========================================================================
Update_t CmdSource (int nArgs)
{
	if (! nArgs)
	{
		g_bSourceLevelDebugging = false;
	}
	else
	{
		g_bSourceAddMemory = false;
		g_bSourceAddSymbols = false;

		for ( int iArg = 1; iArg <= nArgs; iArg++ )
		{	
			const std::string pFileName = g_aArgs[ iArg ].sArg;

			int iParam;
			bool bFound = FindParam( pFileName.c_str(), MATCH_EXACT, iParam, _PARAM_SOURCE_BEGIN, _PARAM_SOURCE_END ) > 0 ? true : false;
			if (bFound && (iParam == PARAM_SRC_SYMBOLS))
			{
				g_bSourceAddSymbols = true;
			}
			else
			if (bFound && (iParam == PARAM_SRC_MEMORY))
			{
				g_bSourceAddMemory = true;	
			}
			else
			{
				const std::string sFileName = g_sProgramDir + pFileName;

				const int MAX_MINI_FILENAME = 20; 
				const std::string sMiniFileName = sFileName.substr(0, MIN(MAX_MINI_FILENAME, sFileName.size()));

				if (BufferAssemblyListing( sFileName ))
				{
					g_aSourceFileName = pFileName;

					if (! ParseAssemblyListing( g_bSourceAddMemory, g_bSourceAddSymbols ))
					{
						ConsoleBufferPushFormat( "Couldn't load filename: %s", sMiniFileName.c_str() );
					}
					else
					{
						if (g_nSourceAssembleBytes)
						{
							ConsoleBufferPushFormat( "  Read: %d lines, %d symbols, %d bytes"
								, g_AssemblerSourceBuffer.GetNumLines() // g_nSourceAssemblyLines
								, g_nSourceAssemblySymbols, g_nSourceAssembleBytes );
						}
						else
						{
							ConsoleBufferPushFormat( "  Read: %d lines, %d symbols"
								, g_AssemblerSourceBuffer.GetNumLines() // g_nSourceAssemblyLines
								, g_nSourceAssemblySymbols );
						}
					}
				}
				else
				{
					ConsoleBufferPushFormat( "Error reading: %s", sMiniFileName.c_str() );
				}
			}
		}
		return ConsoleUpdate();
	}

	return UPDATE_CONSOLE_DISPLAY;	
}

//===========================================================================
Update_t CmdSync (int nArgs)
{
	// TODO
	return UPDATE_CONSOLE_DISPLAY;	
}


// Stack __________________________________________________________________________________________


//===========================================================================
Update_t CmdStackPush (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdStackPop (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdStackPopPseudo (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

// Video __________________________________________________________________________________________

Update_t CmdVideoScannerInfo (int nArgs)
{
	if (nArgs != 1)
	{
		return Help_Arg_1(CMD_VIDEO_SCANNER_INFO);
	}
	else
	{
		if (strcmp(g_aArgs[1].sArg, "dec") == 0)
			g_videoScannerDisplayInfo.isDecimal = true;
		else if (strcmp(g_aArgs[1].sArg, "hex") == 0)
			g_videoScannerDisplayInfo.isDecimal = false;
		else if (strcmp(g_aArgs[1].sArg, "real") == 0)
			g_videoScannerDisplayInfo.isHorzReal = true;
		else if (strcmp(g_aArgs[1].sArg, "apple") == 0)
			g_videoScannerDisplayInfo.isHorzReal = false;
		else
			return Help_Arg_1(CMD_VIDEO_SCANNER_INFO);
	}

	ConsoleBufferPushFormat("Video-scanner display updated: %s", g_aArgs[1].sArg);
	ConsoleBufferToDisplay();

	return UPDATE_ALL;
}

// Cycles __________________________________________________________________________________________

Update_t CmdCyclesInfo (int nArgs)
{
	if (nArgs != 1)
	{
		return Help_Arg_1(CMD_CYCLES_INFO);
	}
	else
	{
		if (strcmp(g_aArgs[1].sArg, "abs") == 0)
			g_videoScannerDisplayInfo.cycleMode = VideoScannerDisplayInfo::abs;
		else if (strcmp(g_aArgs[1].sArg, "rel") == 0)
			g_videoScannerDisplayInfo.cycleMode = VideoScannerDisplayInfo::rel;
		else if (strcmp(g_aArgs[1].sArg, "part") == 0)
			g_videoScannerDisplayInfo.cycleMode = VideoScannerDisplayInfo::part;
		else
			return Help_Arg_1(CMD_CYCLES_INFO);

		if (g_videoScannerDisplayInfo.cycleMode == VideoScannerDisplayInfo::part)
			CmdCyclesReset(0);
	}

	ConsoleBufferPushFormat("Cycles display updated: %s", g_aArgs[1].sArg);
	ConsoleBufferToDisplay();

	return UPDATE_ALL;
}

Update_t CmdCyclesReset (int /*nArgs*/)
{
	g_videoScannerDisplayInfo.savedCumulativeCycles = g_nCumulativeCycles;
	return UPDATE_ALL;
}

// View ___________________________________________________________________________________________

// See: CmdWindowViewOutput (int nArgs)
// NOTE: Keep in sync: ViewVideoPage_t, , and getVideoScannerAddressTXT
enum ViewVideoPage_t
{
	VIEW_PAGE_X, // current page
	VIEW_PAGE_0, // Pseudo
	VIEW_PAGE_1,
	VIEW_PAGE_2,
	VIEW_PAGE_3, // Pseudo
	VIEW_PAGE_4, // Pseudo
	VIEW_PAGE_5, // Pseudo
	VIEW_PAGE_6, // Pseudo
	VIEW_PAGE_7, // Pseudo
	VIEW_PAGE_8  // Pseudo
};

static Update_t _ViewOutput ( ViewVideoPage_t iPage, UINT bVideoModeFlags )
{
	switch ( iPage )
	{
		// NOTE: Keep in sync: _ViewOutput() getVideoScannerAddressHGR()
		case VIEW_PAGE_X:
			bVideoModeFlags |= (!GetVideo().VideoGetSW80STORE() && GetVideo().VideoGetSWPAGE2()) ? VF_PAGE2 : 0;
			bVideoModeFlags |= GetVideo().VideoGetSWMIXED() ? VF_MIXED : 0;
			break; // Page Current & current MIXED state
		case VIEW_PAGE_0: bVideoModeFlags |= VF_PAGE0; break; // Pseudo   Page 0 ($0000)
		case VIEW_PAGE_1: bVideoModeFlags |= 0       ; break; // Hardware Page 1 ($2000), NOTE: VF_HIRES will be passed in
		case VIEW_PAGE_2: bVideoModeFlags |= VF_PAGE2; break; // Hardware Page 2 ($4000)
		case VIEW_PAGE_3: bVideoModeFlags |= VF_PAGE3; break; // Pseudo   Page 3 ($6000)
		case VIEW_PAGE_4: bVideoModeFlags |= VF_PAGE4; break; // Pseudo   Page 4 ($8000)
		case VIEW_PAGE_5: bVideoModeFlags |= VF_PAGE5; break; // Pseudo   Page 5 ($A000)
		case VIEW_PAGE_6: bVideoModeFlags |= VF_PAGE6; break; // Pseudo   Page 6 (LC 1/2 $C000,$D000)
		case VIEW_PAGE_7: bVideoModeFlags |= VF_PAGE7; break; // Pseudo   Page 7 (LC 2/- $D000,$E000)
		case VIEW_PAGE_8: bVideoModeFlags |= VF_PAGE8; break; // Pseudo   Page 8 (LC RAM $E000,$F000)
		default:
			_ASSERT(0);
			break;
	}

	bVideoModeFlags |= GetVideo().VideoGet80COLAUXEMPTY() ? VF_80COL_AUX_EMPTY : 0;	// Preserve this flag

	DebugVideoMode::Instance().Set(bVideoModeFlags);
	GetFrame().VideoRefreshScreen( bVideoModeFlags, true );
	return UPDATE_NOTHING; // intentional
}

// Text 40
	Update_t CmdViewOutput_Text4X (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, VF_TEXT );
	}
	Update_t CmdViewOutput_Text41 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_TEXT );
	}
	Update_t CmdViewOutput_Text42 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, VF_TEXT );
	}
// Text 80
	Update_t CmdViewOutput_Text8X (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, VF_TEXT | VF_80COL );
	}
	Update_t CmdViewOutput_Text81 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_TEXT | VF_80COL );
	}
	Update_t CmdViewOutput_Text82 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, VF_TEXT | VF_80COL );
	}
// Lo-Res
	Update_t CmdViewOutput_GRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, 0 );
	}
	Update_t CmdViewOutput_GR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, 0 );
	}
	Update_t CmdViewOutput_GR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, 0 );
	}
// Double Lo-Res
	Update_t CmdViewOutput_DGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, VF_DHIRES | VF_80COL );
	}
	Update_t CmdViewOutput_DGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_DHIRES | VF_80COL );
	}
	Update_t CmdViewOutput_DGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, VF_DHIRES | VF_80COL );
	}
// Hi-Res
	Update_t CmdViewOutput_HGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, VF_HIRES );
	}
	Update_t CmdViewOutput_HGR0 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_0, VF_HIRES ); // Pseudo page ($0000)
	}
	Update_t CmdViewOutput_HGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_HIRES );
	}
	Update_t CmdViewOutput_HGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, VF_HIRES );
	}
	Update_t CmdViewOutput_HGR3 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_3, VF_HIRES ); // Pseudo page ($6000)
	}
	Update_t CmdViewOutput_HGR4 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_4, VF_HIRES ); // Pseudo page ($8000)
	}
	Update_t CmdViewOutput_HGR5 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_5, VF_HIRES ); // Pseudo page ($A000)
	}
	Update_t CmdViewOutput_HGR6 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_6, VF_HIRES ); // Pseudo page (LC Bank 1/2 $C000,$D000)
	}
	Update_t CmdViewOutput_HGR7 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_7, VF_HIRES ); // Pseudo page (LC Bank 2/RAM $D000,$E000)
	}
	Update_t CmdViewOutput_HGR8 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_8, VF_HIRES ); // Pseudo page (LC RAM $E000,$F000)
	}
// Double Hi-Res
	Update_t CmdViewOutput_DHGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, VF_HIRES | VF_DHIRES | VF_80COL );
	}
	Update_t CmdViewOutput_DHGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_HIRES | VF_DHIRES | VF_80COL);
	}
	Update_t CmdViewOutput_DHGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, VF_HIRES | VF_DHIRES | VF_80COL );
	}
// Super Hi-Res
	Update_t CmdViewOutput_SHR(int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, VF_SHR );
	}

// Watches ________________________________________________________________________________________


//===========================================================================
Update_t CmdWatchAdd (int nArgs)
{
	// WA [address]
	// WA # address
	if (!nArgs)
	{
		return CmdWatchList(0);
	}

	int iArg = 1;
	int iWatch = NO_6502_TARGET;
	if (nArgs > 1)
	{
		iWatch = g_aArgs[1].nValue;
		if (iWatch >= MAX_WATCHES)
		{
			ConsoleDisplayPushFormat("Watch index too big.  (Max: %d)", MAX_WATCHES - 1);
			return ConsoleUpdate();
		}

		iArg++;
	}

	bool bAdded = false;
	for (; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[iArg].eToken == TOKEN_ALPHANUMERIC && g_aArgs[iArg].sArg[0] == 'v')	// 'video' ?
		{
			g_aWatches[iWatch].bSet = true;
			g_aWatches[iWatch].bEnabled = true;
			g_aWatches[iWatch].eSource = BP_SRC_VIDEO_SCANNER;
			g_aWatches[iWatch].nAddress = 0xDEAD;
			bAdded = true;
			g_nWatches++;
			iWatch++;
			continue;
		}

		WORD nAddress = g_aArgs[iArg].nValue;

		// Make sure address isn't an IO address
		if ((nAddress >= APPLE_IO_BEGIN) && (nAddress <= APPLE_IO_END))
			return ConsoleDisplayError("You cannot watch an I/O location.");

		if (iWatch == NO_6502_TARGET)
		{
			iWatch = 0;
			while ((iWatch < MAX_WATCHES) && (g_aWatches[iWatch].bSet))
			{
				iWatch++;
			}
		}

		if ((iWatch >= MAX_WATCHES) && !bAdded)
		{
			ConsoleDisplayPushFormat("All watches are currently in use.  (Max: %d)", MAX_WATCHES);
			return ConsoleUpdate();
		}

		if ((iWatch < MAX_WATCHES) && (g_nWatches < MAX_WATCHES))
		{
			g_aWatches[iWatch].bSet = true;
			g_aWatches[iWatch].bEnabled = true;
			g_aWatches[iWatch].eSource = BP_SRC_MEM_RW;
			g_aWatches[iWatch].nAddress = (WORD) nAddress;
			bAdded = true;
			g_nWatches++;
			iWatch++;
		}
	}

	if (!bAdded)
		goto _Help;
	
	return UPDATE_WATCH;
	
_Help:
	return Help_Arg_1( CMD_WATCH_ADD );
}

//===========================================================================
Update_t CmdWatchClear (int nArgs)
{
	if (!g_nWatches)
		return ConsoleDisplayError("There are no watches defined.");

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_CLEAR );	

	_BWZ_ClearViaArgs( nArgs, g_aWatches, MAX_WATCHES, g_nWatches );

//	if (! g_nWatches)
//	{
//		UpdateDisplay(UPDATE_BACKGROUND); // 1
//		return UPDATE_NOTHING; // 0
//	}

	return UPDATE_CONSOLE_DISPLAY | UPDATE_WATCH; // 1
}

//===========================================================================
Update_t CmdWatchDisable (int nArgs)
{
	if (! g_nWatches)
		return ConsoleDisplayError("There are no watches defined.");

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_DISABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, false );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchEnable (int nArgs)
{
	if (! g_nWatches)
		return ConsoleDisplayError("There are no watches defined.");

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_ENABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, true );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchList (int nArgs)
{
	if (! g_nWatches)
	{
		ConsoleBufferPushFormat( "  There are no current watches.  (Max: %d)", MAX_WATCHES );
	}
	else
	{
//		_BWZ_List( g_aWatches, MAX_WATCHES );
		_BWZ_ListAll( g_aWatches, MAX_WATCHES );
	}
	return ConsoleUpdate();
}

/*
//===========================================================================
Update_t CmdWatchLoad (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_LOAD );

	return UPDATE_CONSOLE_DISPLAY;
}
*/

//===========================================================================
Update_t CmdWatchSave (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_SAVE );

	return UPDATE_CONSOLE_DISPLAY;
}


// Window _________________________________________________________________________________________

//===========================================================================
void _WindowJoin ()
{
	g_aWindowConfig[ g_iWindowThis ].bSplit = false;
}

//===========================================================================
void _WindowSplit ( Window_e eNewBottomWindow )
{
	g_aWindowConfig[ g_iWindowThis ].bSplit = true;
	g_aWindowConfig[ g_iWindowThis ].eBot = eNewBottomWindow;
}

//===========================================================================
void _WindowLast ()
{
	int eNew = g_iWindowLast;
	g_iWindowLast = g_iWindowThis;
	g_iWindowThis = eNew;
}

//===========================================================================
void _WindowSwitch( int eNewWindow )
{
	g_iWindowLast = g_iWindowThis;
	g_iWindowThis = eNewWindow;
}

//===========================================================================
Update_t _CmdWindowViewCommon ( int iNewWindow )
{
	// Switching to same window, remove split
	if (g_iWindowThis == iNewWindow)
	{
		g_aWindowConfig[ iNewWindow ].bSplit = false;
	}
	else
	{
		_WindowSwitch( iNewWindow ); 
	}

//	WindowUpdateConsoleDisplayedSize();
	WindowUpdateSizes();
	return UPDATE_ALL;
}

//===========================================================================
Update_t _CmdWindowViewFull ( int iNewWindow )
{
	if (g_iWindowThis != iNewWindow)
	{
		g_aWindowConfig[ iNewWindow ].bSplit = false;
		_WindowSwitch( iNewWindow ); 
		WindowUpdateConsoleDisplayedSize();
	}
	return UPDATE_ALL;
}

//===========================================================================
void WindowUpdateConsoleDisplayedSize ()
{
	g_nConsoleDisplayLines = MIN_DISPLAY_CONSOLE_LINES;
#if USE_APPLE_FONT
	g_bConsoleFullWidth = true;
	g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;

	if (g_iWindowThis == WINDOW_CONSOLE)
	{
		g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
		g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
		g_bConsoleFullWidth = true;
	}

	g_nConsoleInputMaxLen      = g_nConsoleDisplayWidth-1; // -1 prompt at Start-of-Line, -1 for cursor at End-of-Line
	g_nConsoleInputScrollWidth = g_nConsoleDisplayWidth-1; // Maximum number of characters for the horizontol scrolling window on the input line
#else
	g_nConsoleDisplayWidth = (CONSOLE_WIDTH / 2) + 10;
	g_bConsoleFullWidth = false;

//	g_bConsoleFullWidth = false;
//	g_nConsoleDisplayWidth = CONSOLE_WIDTH - 10;

	if (g_iWindowThis == WINDOW_CONSOLE)
	{
		g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
		g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
		g_bConsoleFullWidth = true;
	}
#endif
}

//===========================================================================
int WindowGetHeight( int iWindow )
{
//	if (iWindow == WINDOW_CODE)
	return g_nDisasmWinHeight;
}

//===========================================================================
void WindowUpdateDisasmSize ()
{
	if (g_aWindowConfig[ g_iWindowThis ].bSplit)
	{
		g_nDisasmWinHeight = (MAX_DISPLAY_LINES - g_nConsoleDisplayLines) / 2;
	}
	else
	{
		g_nDisasmWinHeight = MAX_DISPLAY_LINES - g_nConsoleDisplayLines;
	}
	g_nDisasmCurLine = MAX(0, (g_nDisasmWinHeight - 1) / 2);
#if _DEBUG
#endif
}

//===========================================================================
void WindowUpdateSizes ()
{
	WindowUpdateDisasmSize();
	WindowUpdateConsoleDisplayedSize();
}


//===========================================================================
Update_t CmdWindowCycleNext (int nArgs)
{
	g_iWindowThis++;
	if (g_iWindowThis >= NUM_WINDOWS)
		g_iWindowThis = 0;

	WindowUpdateSizes();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowCyclePrev (int nArgs)
{
	g_iWindowThis--;
	if (g_iWindowThis < 0)
		g_iWindowThis = NUM_WINDOWS-1;

	WindowUpdateSizes();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowShowCode (int nArgs)
{
	// g_bWindowDisplayShowChild = false;
	// g_bWindowDisplayShowRoot  = WINDOW_CODE;

	if (g_iWindowThis == WINDOW_CODE)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = false;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_CODE; // not really needed, but SAFE HEX ;-)
	}
	else	
	if (g_iWindowThis == WINDOW_DATA)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_CODE;
	}

	WindowUpdateSizes();

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowCode1 (int nArgs)
{
/*
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis != WINDOW_DATA))
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eTop = WINDOW_CODE;
		
		Window_e eWindow = WINDOW_CODE;
		if (g_iWindowThis == WINDOW_DATA)
			eWindow = WINDOW_DATA;

		g_aWindowConfig[ g_iWindowThis ].eBot = eWindow;
		return UPDATE_ALL;
	}
*/
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowCode2 (int nArgs)
{
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis == WINDOW_DATA))
	{
		if (g_iWindowThis == WINDOW_CODE)
		{
			_WindowJoin();
			WindowUpdateDisasmSize();
		}
		else		
		if (g_iWindowThis == WINDOW_DATA)
		{
			_WindowSplit( WINDOW_CODE );
			WindowUpdateDisasmSize();
		}		
		return UPDATE_DISASM;

	}
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowData (int nArgs)
{
	if (g_iWindowThis == WINDOW_CODE)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_DATA;
		return UPDATE_ALL;
	}
	else
	if (g_iWindowThis == WINDOW_DATA)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = false;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_DATA; // not really needed, but SAFE HEX ;-)
		return UPDATE_ALL;
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowData1 (int nArgs)
{
/*
	if (g_iWindowThis != PARAM_CODE_1)
	{
		g_iWindowLast = g_iWindowThis;
		g_iWindowThis = PARAM_DATA_1;
		return UPDATE_ALL;
	}
*/
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowData2 (int nArgs)
{
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis == WINDOW_DATA))
	{
		if (g_iWindowThis == WINDOW_CODE)
		{
			_WindowSplit( WINDOW_DATA );
		}
		else		
		if (g_iWindowThis == WINDOW_DATA)
		{
			_WindowJoin();
		}		
		return UPDATE_DISASM;

	}
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowSource (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowSource1 (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowSource2 (int nArgs)
{
	_WindowSplit( WINDOW_SOURCE );
	WindowUpdateSizes();

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowViewCode (int nArgs)
{
	return _CmdWindowViewCommon( WINDOW_CODE );
}

//===========================================================================
Update_t CmdWindowViewConsole (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
}

//===========================================================================
Update_t CmdWindowViewData (int nArgs)
{
	return _CmdWindowViewCommon( WINDOW_DATA );
}

//===========================================================================
Update_t CmdWindowViewOutput (int nArgs)
{
	GetFrame().VideoRedrawScreen();

	DebugVideoMode::Instance().Set( GetVideo().GetVideoMode() );

	return UPDATE_NOTHING; // intentional
}

//===========================================================================
Update_t CmdWindowViewSource (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
}

//===========================================================================
Update_t CmdWindowViewSymbols (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
}

//===========================================================================
Update_t CmdWindow (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WINDOW );

	int iParam;
	char *pName = g_aArgs[1].sArg;
	int nFound = FindParam( pName, MATCH_EXACT, iParam, _PARAM_WINDOW_BEGIN, _PARAM_WINDOW_END );
	if (nFound)
	{
		switch (iParam)
		{
			case PARAM_CODE   : return CmdWindowViewCode(0)   ; break;
			case PARAM_CONSOLE: return CmdWindowViewConsole(0); break;
			case PARAM_DATA   : return CmdWindowViewData(0)   ; break;
//			case PARAM_INFO   : CmdWindowInfo(); break;
			case PARAM_SOURCE : return CmdWindowViewSource(0) ; break;
			case PARAM_SYMBOLS: return CmdWindowViewSymbols(0); break;
			default:
				return Help_Arg_1( CMD_WINDOW );
				break;
		}
	}	

	WindowUpdateConsoleDisplayedSize();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowLast (int nArgs)
{
	_WindowLast();
	WindowUpdateConsoleDisplayedSize();
	return UPDATE_ALL;
}

// ZeroPage _______________________________________________________________________________________


//===========================================================================
Update_t CmdZeroPageAdd (int nArgs)
{
	// ZP [address]
	// ZP # address [address...]
	if (!nArgs)
	{
		return CmdZeroPageList(0);
	}

	int iArg = 1;
	int iZP = NO_6502_TARGET;

	if (nArgs > 1)
	{
		iZP = g_aArgs[1].nValue;
		if (iZP >= MAX_ZEROPAGE_POINTERS)
		{
			ConsoleDisplayPushFormat("Zero page pointer index too big.  (Max: %d)", MAX_ZEROPAGE_POINTERS - 1);
			return ConsoleUpdate();
		}

		iArg++;
	}
	
	bool bAdded = false;
	for (; iArg <= nArgs; iArg++ )
	{
		WORD nAddress = g_aArgs[iArg].nValue;

		// Make sure address is a ZP address
		if (nAddress > _6502_ZEROPAGE_END)
		{
			ConsoleDisplayPushFormat("Zero page pointer must be in the range: [00..%02X].", _6502_ZEROPAGE_END);
			return ConsoleUpdate();
		}

		if (iZP == NO_6502_TARGET)
		{
			iZP = 0;
			while ((iZP < MAX_ZEROPAGE_POINTERS) && (g_aZeroPagePointers[iZP].bSet))
			{
				iZP++;
			}
		}

		if ((iZP >= MAX_ZEROPAGE_POINTERS) && !bAdded)
		{
			ConsoleDisplayPushFormat("All zero page pointers are currently in use.  (Max: %d)", MAX_ZEROPAGE_POINTERS);
			return ConsoleUpdate();
		}
		
		if ((iZP < MAX_ZEROPAGE_POINTERS) && (g_nZeroPagePointers < MAX_ZEROPAGE_POINTERS))
		{
			g_aZeroPagePointers[iZP].bSet = true;
			g_aZeroPagePointers[iZP].bEnabled = true;
			g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
			bAdded = true;
			g_nZeroPagePointers++;
			iZP++;
		}
	}

	if (!bAdded)
		goto _Help;
	
	return UPDATE_ZERO_PAGE | ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_ZEROPAGE_POINTER_ADD );

}

Update_t _ZeroPage_Error()
{
//	return ConsoleDisplayError( "There are no (ZP) pointers defined." );
//	ConsoleBufferPushFormat( "  There are no current (ZP) pointers.  (Max: %d)", MAX_ZEROPAGE_POINTERS );
	return ConsoleDisplayErrorFormat( "  There are no current (ZP) pointers.  (Max: %d)", MAX_ZEROPAGE_POINTERS );
}

//===========================================================================
Update_t CmdZeroPageClear (int nArgs)
{
	if (!g_nZeroPagePointers)
		return _ZeroPage_Error();

	// CHECK FOR ERRORS
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_CLEAR );

	_BWZ_ClearViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, g_nZeroPagePointers );

	if (!g_nZeroPagePointers)
	{
		UpdateDisplay( UPDATE_BACKGROUND );
		return UPDATE_CONSOLE_DISPLAY;
	}

	return UPDATE_CONSOLE_DISPLAY | UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageDisable (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_DISABLE );
	if (! g_nZeroPagePointers)
		return _ZeroPage_Error();

	_BWZ_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, false );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageEnable (int nArgs)
{
	if (! g_nZeroPagePointers)
		return _ZeroPage_Error();

	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_ENABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, true );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageList (int nArgs)
{
	if (! g_nZeroPagePointers)
	{
		_ZeroPage_Error();
	}
	else
	{	
		_BWZ_ListAll( g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS );
	}
	return ConsoleUpdate();
}

/*
//===========================================================================
Update_t CmdZeroPageLoad    (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;

}
*/

//===========================================================================
Update_t CmdZeroPageSave (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdZeroPagePointer (int nArgs)
{
	// p[0..4]                : disable
	// p[0..4] <ZeroPageAddr> : enable

	if ( (nArgs != 0) && (nArgs != 1) )
		return Help_Arg_1( g_iCommand );
//		return DisplayHelp(CmdZeroPagePointer);

//	int nPtrNum = g_aArgs[0].sArg[1] - '0'; // HACK: hard-coded to command length
	int iZP = g_iCommand - CMD_ZEROPAGE_POINTER_0;

	if ( (iZP < 0) || (iZP >= MAX_ZEROPAGE_POINTERS) )
		return Help_Arg_1( g_iCommand );

	if (nArgs == 0)
	{
		g_aZeroPagePointers[iZP].bEnabled = false;
	}
	else
	{
		g_aZeroPagePointers[iZP].bSet = true;
		g_aZeroPagePointers[iZP].bEnabled = true;

		WORD nAddress = g_aArgs[1].nValue;
		g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
	}

	return UPDATE_ZERO_PAGE;
}


// Command Input __________________________________________________________________________________


// Note: Range is [iParamBegin,iParamEnd], not the usually (STL) expected [iParamBegin,iParamEnd)
//===========================================================================
int FindParam (LPCTSTR pLookupName, Match_e eMatch, int & iParam_, int iParamBegin, int iParamEnd, const bool bCaseSensitive /* false */ )
{
	int nFound = 0;
	int nLen     = strlen( pLookupName );
	int iParam = 0;

	if (! nLen)
		return nFound;

#if ALLOW_INPUT_LOWERCASE
	if (! bCaseSensitive) // HACK: Until We fixup all callers using MATCH_EXACT with MATCH_ANYCASE we need to preserve behavior of ALLOW_INPUT_LOWERCASE always being MATCH_FUZZY
		eMatch = MATCH_FUZZY;
#endif

	if (eMatch == MATCH_EXACT)
	{
//		while (iParam < NUM_PARAMS )
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			char *pParamName = g_aParameters[iParam].m_sName;
			int eCompare = strcmp(pLookupName, pParamName);
			if (! eCompare) // exact match?
			{
				nFound++;
				iParam_ = g_aParameters[iParam].iCommand;
				break;
			}
		}
	}
	else
	if (eMatch == MATCH_FUZZY)
	{	
#if ALLOW_INPUT_LOWERCASE
		char aLookup[ 256 ] = "";
		for ( int i = 0; i < nLen; i++ )
		{
			aLookup[ i ] = toupper( pLookupName[ i ] );
		}
#endif
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			char *pParamName = g_aParameters[ iParam ].m_sName;
// _tcsnccmp

#if ALLOW_INPUT_LOWERCASE
			if (! strncmp(aLookup, pParamName ,nLen))
#else
			if (! strncmp(pLookupName, pParamName ,nLen))
#endif
			{
				nFound++;
				iParam_ = g_aParameters[iParam].iCommand;

				if (!_stricmp(pLookupName, pParamName)) // exact match?
				{
					nFound = 1; // Exact match takes precidence over fuzzy matches
					break;
				}
			}		
		}
	}
	return nFound;
}

//===========================================================================
int FindCommand ( LPCTSTR pName, CmdFuncPtr_t & pFunction_, int * iCommand_ )
{
	g_vPotentialCommands.clear();

	int nFound   = 0;
	int nLen     = strlen( pName );
	int iCommand = 0;

	if (! nLen)
		return nFound;

	char sCommand[ CONSOLE_WIDTH ];
	strcpy( sCommand, pName );
	_strupr( sCommand );

	while ((iCommand < NUM_COMMANDS_WITH_ALIASES)) // && (name[0] >= g_aCommands[iCommand].aName[0])) Command no longer in Alphabetical order
	{
		char *pCommandName = g_aCommands[iCommand].m_sName;
//		int iCmp = strcasecmp( sCommand, pCommandName, nLen )
		if (! strncmp(sCommand, pCommandName, nLen))
		{
			pFunction_ = g_aCommands[iCommand].pFunction;
			if (pFunction_)
			{
				g_iCommand = g_aCommands[iCommand].iCommand;

				// Don't push the same comamnd/alias if already on the list
				if (std::find( g_vPotentialCommands.begin(), g_vPotentialCommands.end(), g_iCommand) == g_vPotentialCommands.end())
				{
					nFound++;
					g_vPotentialCommands.push_back( g_iCommand );

					if (iCommand_)
						*iCommand_ = iCommand;
// !strcmp
					if (!_stricmp(sCommand, pCommandName)) // exact match?
					{
	//					if (iCommand_)
	//						*iCommand_ = iCommand;

						nFound = 1; // Exact match takes precidence over fuzzy matches
						g_vPotentialCommands.clear();
						break;
					}
				}
			}
		}
		iCommand++;
	}

//	if (nFound == 1)
//	{
//
//	}

	return nFound;
}

//===========================================================================
void DisplayAmbigiousCommands ( int nFound )
{
	ConsolePrintFormat("Ambiguous " CHC_NUM_DEC "%" SIZE_T_FMT CHC_DEFAULT " Commands:"
		, g_vPotentialCommands.size()
	);

	int iCommand = 0;
	while (iCommand < nFound)
	{
		std::string sPotentialCommands = CHC_COMMAND " ";

		while ((iCommand < nFound) && (sPotentialCommands.length() < size_t(g_nConsoleDisplayWidth)))
		{
			int const nCommand = g_vPotentialCommands[ iCommand ];
			const char *const pName = g_aCommands[ nCommand ].m_sName;
			size_t const nLen = strlen( pName );

			if ((sPotentialCommands.length() + nLen) >= (CONSOLE_WIDTH - 1))
				break;

			sPotentialCommands.append(pName, nLen).append(1, ' ');

			iCommand++;
		}
		ConsolePrint( sPotentialCommands.c_str() );
	}
}

bool IsHexDigit( char c )
{
	if ((c >= '0') && (c <= '9'))
		return true;
	else
	if ((c >= 'A') && (c <= 'F'))
		return true;
	else
	if ((c >= 'a') && (c <= 'f'))
		return true;
	return false;
}


//===========================================================================
Update_t ExecuteCommand (int nArgs) 
{
	Arg_t * pArg     = & g_aArgs[ 0 ];
	char  * pCommand = & pArg->sArg[0];

	CmdFuncPtr_t pFunction = NULL;
	int nFound = FindCommand( pCommand, pFunction );

//	int nCookMask = (1 << NUM_TOKENS) - 1; // ArgToken_e used as bit mask!
	
	// BUGFIX: commands that are also valid hex addresses
	// ####:# [#]
	// #<#.#M
	int nLen = pArg->nArgLen;

	if ((! nFound) || (nLen < 6))
	{
		// 2.7.0.36 Fixed: empty command was re-triggering previous command. Example: DW 6062, // test
		if ( nLen > 0 )
		{
			// verify pCommand[ 0 .. (nLen-1) ] are hex digits
			bool bIsHex = false;
			char *pChar = pCommand;
			for (int iChar = 0; iChar < (nLen - 1); iChar++, pChar++ )
			{
				bIsHex = IsHexDigit( *pChar );
				if ( !bIsHex )
				{
					break;
				}
			}
			
			if (bIsHex)
			{
				// Support Apple Monitor commands

				WORD nAddress = 0;

				// ####G -> JMP $address (exit debugger)
				// NB. AppleWin 'g','gg' commands handled via pFunction below
				if ((pCommand[nLen-1] == 'G') ||
					(pCommand[nLen-1] == 'g'))
				{
					pCommand[nLen-1] = 0;
					ArgsGetValue( pArg, & nAddress );

					regs.pc = nAddress;

					g_nAppMode = MODE_RUNNING; // exit the debugger

					nFound = 1;
					g_iCommand = CMD_OUTPUT_ECHO; // hack: don't cook args
				}
				else
				// ####L -> Unassemble $address
				if (((pCommand[nLen-1] == 'L') ||
				     (pCommand[nLen-1] == 'l'))&&
				    (_stricmp("cl", pCommand) != 0)) // workaround for ambiguous "cl": must be handled by "clear flag" command
				{
					pCommand[nLen-1] = 0;
					ArgsGetValue( pArg, & nAddress );

					g_iCommand = CMD_UNASSEMBLE;

					// replace: addrL
					// with:    comamnd addr
					pArg[1] = pArg[0];
					strcpy( pArg->sArg, g_aCommands[ g_iCommand ].m_sName );
					pArg->nArgLen = strlen( pArg->sArg );

					pArg++;
					pArg->nValue = nAddress;
					nArgs++;
					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}					
				else
				// address: byte ...
				if ((pArg+1)->eToken == TOKEN_COLON)
				{
					g_iCommand = CMD_MEMORY_ENTER_BYTE;

					// replace: addr :
					// with:    command addr
					pArg[1] = pArg[0];

					strcpy( pArg->sArg, g_aCommands[ g_iCommand ].m_sName );
					pArg->nArgLen = strlen( pArg->sArg );

//					nCookMask &= ~ (1 << TOKEN_COLON);
//					nArgs++;

					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}
				else
				// #<#.#M
				if	(pArg[1].eToken == TOKEN_LESS_THAN)
				{
					// Look for period
					nLen = pArg[2].nArgLen;

					char *pDst = pArg[0].sArg;
					char *pSrc = pArg[2].sArg;
					char *pEnd = 0;

					bool bFoundSrc = false;
					bool bFoundLen = false;

					pChar = pSrc;
					while ( *pChar )
					{
						if ( *pChar == '.' )
						{
							if ( pEnd ) // only allowed one period
							{
								pEnd = 0;
								break;
							}

							*pChar = 0; // ':';
							pEnd = pChar + 1;
							bFoundSrc = true;
						} else
						if ( !IsHexDigit( *pChar ) )
						{
							break;
						}	
						pChar++;
					}
					if ( pEnd ) {
						if (	(*pChar == 'M')
						||	(*pChar == 'm'))
						{
							*pChar++ = 0;
							if ( ! *pChar )
								bFoundLen = true;
						}

						if ( bFoundSrc && bFoundLen )
						{
							//ArgsGetValue( pArg, & nAddress );
							//ConsolePrintFormat( "Dst:%s  Src: %s  End: %s", pDst, pSrc, pEnd );

							g_iCommand = CMD_MEMORY_MOVE;

							strcpy( pArg[4].sArg, pEnd );
							strcpy( pArg[3].sArg, g_aTokens[ TOKEN_COLON ].sToken );
							strcpy( pArg[2].sArg, pSrc );
							strcpy( pArg[1].sArg, pDst );
							strcpy( pArg[0].sArg, g_aCommands[ g_iCommand ].m_sName );
							// pDst moved from arg0 to arg1 !
							pArg[1].bType = TYPE_VALUE;
							pArg[2].bType = TYPE_VALUE;
							pArg[3].bType = TYPE_OPERATOR;
							pArg[4].bType = TYPE_VALUE;

							ArgsGetValue( &pArg[1], &pArg[1].nValue );
							ArgsGetValue( &pArg[2], &pArg[2].nValue );
							pArg[3].eToken = TOKEN_COLON;
							ArgsGetValue( &pArg[4], &pArg[4].nValue );

							if (pArg[4].nValue < pArg[2].nValue)
							{
								ConsolePrintFormat(
									CHC_WARNING "WARN" CHC_ARG_SEP ":"
									CHC_DEFAULT " End source address " CHC_ARG_SEP "$" CHC_ADDRESS "%04X"
									CHC_ARG_SEP " <"
									CHC_DEFAULT " Start source address " CHC_ARG_SEP "$" CHC_ADDRESS "%04X"
									CHC_DEFAULT "."
									CHC_INFO " Aborting."
									, (pArg[4].nValue & _6502_MEM_END)
									, (pArg[2].nValue & _6502_MEM_END)
								);
								ConsoleUpdate();
								pFunction = nullptr;
							}
							else
							{
								pFunction = g_aCommands[ g_iCommand ].pFunction;
							}
							nFound = 1;
							nArgs = 4;
						}
					}
				}

				// TODO: display memory at address
				// addr1 [addr2] -> display byte at address
				// MDB memory display byte (is deprecated, so can be re-used)
			}
		} else {
			return UPDATE_CONSOLE_DISPLAY;
		}
	}

	if (nFound > 1)
	{
// ASSERT (nFound == g_vPotentialCommands.size() );
		DisplayAmbigiousCommands( nFound );
		
		return ConsoleUpdate();
//		return ConsoleDisplayError( gaPotentialCommands );
	}

	if (nFound)
	{
		bool bCook = true;
		if (g_iCommand == CMD_OUTPUT_ECHO)
			bCook = false;

		int nArgsCooked = nArgs;
		if (bCook)
			nArgsCooked = ArgsCook( nArgs ); // nCookMask

		if (nArgsCooked == ARG_SYNTAX_ERROR)
			return ConsoleDisplayError( "Syntax Error" );

		if (pFunction)
			return pFunction( nArgsCooked ); // Eat them

		return UPDATE_CONSOLE_DISPLAY;
	}
	else
		return ConsoleDisplayError( "Illegal Command" );
}



// ________________________________________________________________________________________________


//===========================================================================
void OutputTraceLine ()
{
	if (!g_hTraceFile)
		return;

	DisasmLine_t line;
	GetDisassemblyLine( regs.pc, line );

	// DrawDisassemblyLine( 0,regs.pc, sDisassembly); // Get Disasm String
	std::string sDisassembly = FormatDisassemblyLine( line );

	char sFlags[] = "........";
	WORD nRegFlags = regs.ps;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		int iFlag = (_6502_NUM_FLAGS - nFlag - 1);
		bool bSet = (nRegFlags & 1);
		if (bSet)
			sFlags[nFlag] = g_aBreakpointSource[BP_SRC_FLAG_C + iFlag][0];
		nRegFlags >>= 1;
	}

	if (g_bTraceHeader)
	{
		g_bTraceHeader = false;

		if (g_bTraceFileWithVideoScanner)
		{
			fprintf( g_hTraceFile,
//				"0000 0000 0000 00   00 00 00 0000 --------  0000:90 90 90  NOP"
				"Vert Horz Addr Data A: X: Y: SP:  Flags     Addr:Opcode    Mnemonic\n");
		}
		else
		{
			fprintf( g_hTraceFile,
//				"00000000 00 00 00 0000 --------  0000:90 90 90  NOP"
				"Cycles   A: X: Y: SP:  Flags     Addr:Opcode    Mnemonic\n");
		}
	}

	//std::string const sTarget = (line.bTargetValue)
	//	? StrFormat( "%s:%s", line.sTargetPointer , line.sTargetValue )
	//	: std::string();

	if (g_bTraceFileWithVideoScanner)
	{
		uint16_t vert, horz;
		NTSC_GetVideoVertHorzForDebugger(vert, horz);		// update video scanner's vert/horz position - needed for when in fullspeed (GH#1164)

		uint32_t data;
		int dataSize;
		uint16_t addr = NTSC_GetScannerAddressAndData(data, dataSize);

		fprintf( g_hTraceFile,
			"%04X %04X %04X   %02X %02X %02X %02X %04X %s  %s\n",
			vert,
			horz,
			addr,
			(uint8_t)data,	// truncated
			(unsigned)regs.a,
			(unsigned)regs.x,
			(unsigned)regs.y,
			(unsigned)regs.sp,
			sFlags
			, sDisassembly.c_str()
			//, sTarget.c_str() // TODO: Show target?
		);
	}
	else
	{
		const UINT cycles = (UINT)g_nCumulativeCycles;
		fprintf( g_hTraceFile,
			"%08X %02X %02X %02X %04X %s  %s\n",
			cycles,
			(unsigned)regs.a,
			(unsigned)regs.x,
			(unsigned)regs.y,
			(unsigned)regs.sp,
			sFlags
			, sDisassembly.c_str()
			//, sTarget.c_str() // TODO: Show target?
		);
	}
}

//===========================================================================
int ParseInput ( LPTSTR pConsoleInput, bool bCook )
{
	int nArg = 0;

	// TODO: need to check for non-quoted command separator ';', and buffer input
	RemoveWhiteSpaceReverse( pConsoleInput );

	ArgsClear();
	nArg = ArgsGet( pConsoleInput ); // Get the Raw Args

	int iArg;
	for ( iArg = 0; iArg <= nArg; iArg++ )
	{
		g_aArgs[ iArg ] = g_aArgRaw[ iArg ];
	}

	return nArg;
}

// Return address of next line to write to.
//===========================================================================
ProfileLine_t ProfileLinePeek ( int iLine )
{
	if (iLine < 0)
		iLine = 0;
	
	return ( g_nProfileLine == 0 || iLine <= g_nProfileLine )
		? ProfileLine_t( g_aProfileLine[ iLine ], sizeof(g_aProfileLine[iLine]) )
		: ProfileLine_t();
}

//===========================================================================
ProfileLine_t ProfileLinePush ()
{
	if (g_nProfileLine < NUM_PROFILE_LINES)
	{
		g_nProfileLine++;	
	}

	return ProfileLinePeek( g_nProfileLine  );
}

void ProfileLineReset ()
{
	g_nProfileLine = 0;
}


#define DELIM "%s"
//===========================================================================
void ProfileFormat ( bool bExport, ProfileFormat_e eFormatMode )
{
	std::string sSeparator7;
	std::string sSeparator2;
	std::string sSeparator1;

	if (eFormatMode == PROFILE_FORMAT_COMMA)
	{
		sSeparator7 = ',';
		sSeparator2 = ',';
		sSeparator1 = ',';
	}
	else
	if (eFormatMode == PROFILE_FORMAT_SPACE)
	{
		sSeparator7.assign(7, ' ');
		sSeparator2.assign(2, ' ');
		sSeparator1 = ' ';
	}
	else
	{
		sSeparator7 = '\t';
		sSeparator2 = '\t';
		sSeparator1 = '\t';
	}

	ProfileLineReset();

	bool bOpcodeGood = true;
	bool bOpmodeGood = true;

	std::vector< ProfileOpcode_t > vProfileOpcode( &g_aProfileOpcodes[0], &g_aProfileOpcodes[ NUM_OPCODES ] );
	std::vector< ProfileOpmode_t > vProfileOpmode( &g_aProfileOpmodes[0], &g_aProfileOpmodes[ NUM_OPMODES ] ); 

	// sort >
	std::sort( vProfileOpcode.begin(), vProfileOpcode.end(), ProfileOpcode_t() );
	std::sort( vProfileOpmode.begin(), vProfileOpmode.end(), ProfileOpmode_t() );

	Profile_t nOpcodeTotal = 0;
	Profile_t nOpmodeTotal = 0;

	for ( int iOpcode = 0; iOpcode < NUM_OPCODES; ++iOpcode )
	{
		nOpcodeTotal += vProfileOpcode[ iOpcode ].m_nCount;
	}

	for ( int iOpmode = 0; iOpmode < NUM_OPMODES; ++iOpmode )
	{
		nOpmodeTotal += vProfileOpmode[ iOpmode ].m_nCount;
	}

	if (nOpcodeTotal < 1.)
	{
		nOpcodeTotal = 1;
		bOpcodeGood = false;
	}

	const char *pColorOperator = "";
	const char *pColorNumber   = "";
	const char *pColorOpcode   = "";
	const char *pColorMnemonic = "";
	const char *pColorOpmode   = "";
	const char *pColorTotal    = "";
	if (! bExport)
	{
		pColorOperator = CHC_ARG_SEP; // grey
		pColorNumber   = CHC_NUM_DEC; // cyan
		pColorOpcode   = CHC_NUM_HEX; // yellow
		pColorMnemonic = CHC_COMMAND; // green
		pColorOpmode   = CHC_USAGE  ; // yellow
		pColorTotal    = CHC_DEFAULT; // white
	}
	
	ProfileLine_t prfline = ProfileLinePeek(0);

	// Opcode
	if (bExport) // Export = SeperateColumns
		prfline.Format( "\"Percent\"" DELIM "\"Count\"" DELIM "\"Opcode\"" DELIM "\"Mnemonic\"" DELIM "\"Addressing Mode\"\n",
						sSeparator7.c_str(), sSeparator2.c_str(), sSeparator1.c_str(), sSeparator1.c_str() );
	else
		prfline.Format( "Percent" DELIM "Count" DELIM "Mnemonic" DELIM "Addressing Mode\n",
						sSeparator7.c_str(), sSeparator2.c_str(), sSeparator1.c_str() );
	prfline = ProfileLinePush();
			
	for ( int iOpcode = 0; iOpcode < NUM_OPCODES; ++iOpcode )
	{
		ProfileOpcode_t tProfileOpcode = vProfileOpcode.at( iOpcode );

		Profile_t nCount  = tProfileOpcode.m_nCount;

		// Don't spam with empty data if dumping to the console
		if ((! nCount) && (! bExport))
			continue;

		int       nOpcode = tProfileOpcode.m_iOpcode;
		int       nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;
		double    nPercent = (100. * nCount) / nOpcodeTotal;
		
		//std::string sOpmode = StrFormat( g_aOpmodes[ nOpmode ].m_sFormat, 0 );

		// Excel Bug: Quoted numbers are NOT treated as strings in .csv! WTF?
		// @reference: http://support.microsoft.com/default.aspx?scid=kb;EN-US;Q214233
		//
		// Workaround: Prefix with (') apostrophe -- this doesn't break HEX2DEC()
		// This works properly in Openoffice.
		// In Excel, this ONLY works IF you TYPE it in!
		// 
		// Solution: Quote the numbers, but you must select the "TEXT" Column data format for the "Opcode" column.
		// We don't use .csv, since you aren't given the Import Dialog in Excel!
		// StrFormat( "'%02X", nOpcode ); // SHOULD work with Excel, but only works with OpenOffice.
		const std::string sOpcode = (bExport)
			? StrFormat("\"%02X\"", nOpcode) // Works with Excel, IF using Import dialog & choose Text. (also works with OpenOffice)
			: ByteToHexStr(nOpcode);
		const std::string sAddress = (bExport)
			? ( '"' + std::string(g_aOpmodes[nOpmode].m_sName) + '"' )
			: std::string(g_aOpmodes[ nOpmode ].m_sName);
		
		// BUG: Yeah 100% is off by 1 char. Profiling only one opcode isn't worth fixing this visual alignment bug.
		prfline.Format( "%s%7.4f%s%%" DELIM "%s%9u" DELIM "%s%s" DELIM "%s%s" DELIM "%s%s\n"
			, pColorNumber
			, nPercent
			, pColorOperator
			, sSeparator2.c_str()
			, pColorNumber
			, static_cast<unsigned int>(nCount), sSeparator2.c_str()
			, pColorOpcode
			, sOpcode.c_str(), sSeparator2.c_str()
			, pColorMnemonic
			, g_aOpcodes[ nOpcode ].sMnemonic, sSeparator2.c_str()
			, pColorOpmode
			, sAddress.c_str()
		);
		prfline = ProfileLinePush();
	}

	if (! bOpcodeGood)
		nOpcodeTotal = 0;

	prfline.Format( "Total:  " DELIM "%s%9u\n"
		, sSeparator2.c_str()
		, pColorTotal
		, static_cast<unsigned int>(nOpcodeTotal) );
	prfline = ProfileLinePush();

	prfline.Assign( "\n" );
	prfline = ProfileLinePush();

// Opmode
	//	"Percent     Count  Adressing Mode\n" );
	if (bExport)
		// Note: 2 extra dummy columns are inserted to keep Addressing Mode in same column
		prfline.Format( "\"Percent\"" DELIM "\"Count\"" DELIM DELIM DELIM "\"Addressing Mode\"\n",
						sSeparator7.c_str(), sSeparator2.c_str(), sSeparator2.c_str(), sSeparator2.c_str() );
	else
		prfline.Format( "Percent" DELIM "Count" DELIM "Addressing Mode\n",
						sSeparator7.c_str(), sSeparator2.c_str() );
	prfline = ProfileLinePush();

	if (nOpmodeTotal < 1)
	{
		nOpmodeTotal = 1.;
		bOpmodeGood = false;
	}

	for ( int iOpmode = 0; iOpmode < NUM_OPMODES; ++iOpmode )
	{
		ProfileOpmode_t tProfileOpmode = vProfileOpmode.at( iOpmode );
		Profile_t nCount  = tProfileOpmode.m_nCount;

		// Don't spam with empty data if dumping to the console
		if ((! nCount) && (! bExport))
			continue;

		int       nOpmode = tProfileOpmode.m_iOpmode;
		double    nPercent = (100. * nCount) / nOpmodeTotal;

		const std::string sAddress = (bExport)
			// Note: 2 extra dummy columns are inserted to keep Addressing Mode in same column
			? StrFormat( "%s%s\"%s\"", sSeparator1.c_str(), sSeparator1.c_str(), g_aOpmodes[ nOpmode ].m_sName )
			// not qouted if dumping to console
			: std::string( g_aOpmodes[ nOpmode ].m_sName );

		// BUG: Yeah 100% is off by 1 char. Profiling only one opcode isn't worth fixing this visual alignment bug.
		prfline.Format( "%s%7.4f%s%%" DELIM "%s%9u" DELIM "%s%s\n"
			, pColorNumber
			, nPercent
			, pColorOperator
			, sSeparator2.c_str()
			, pColorNumber
			, static_cast<unsigned int>(nCount), sSeparator2.c_str()
			, pColorOpmode
			, sAddress.c_str()
		);
		prfline = ProfileLinePush();
	}

	if (! bOpmodeGood)
		nOpmodeTotal = 0;

	prfline.Format( "Total:  " DELIM "%s%9u\n"
		, sSeparator2.c_str()
		, pColorTotal
		, static_cast<unsigned int>(nOpmodeTotal) );
	prfline = ProfileLinePush();

	prfline.Assign( "===================\n" );
	prfline = ProfileLinePush();

	unsigned int cycles = static_cast<unsigned int>(g_nCumulativeCycles - g_nProfileBeginCycles);
	prfline.Format( "Cycles: " DELIM "%s%9u\n"
		, sSeparator2.c_str()
		, pColorNumber
		, cycles );
	prfline = ProfileLinePush();
}
#undef DELIM


//===========================================================================
void ProfileReset ()
{
	for ( int iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		g_aProfileOpcodes[ iOpcode ].m_iOpcode = iOpcode;
		g_aProfileOpcodes[ iOpcode ].m_nCount = 0;
	}

	for ( int iOpmode = 0; iOpmode < NUM_OPMODES; iOpmode++ )
	{
		g_aProfileOpmodes[ iOpmode ].m_iOpmode = iOpmode;
		g_aProfileOpmodes[ iOpmode ].m_nCount = 0;
	}

	g_nProfileBeginCycles = g_nCumulativeCycles;
}


//===========================================================================
bool ProfileSave ()
{
	bool bStatus = false;

	const std::string sFilename = g_sProgramDir + g_FileNameProfile; // TODO: Allow user to decide?

	FILE *hFile = fopen( sFilename.c_str(), "wt" );

	if (hFile)
	{
		const int nLine = g_nProfileLine;

		for ( int iLine = 0; iLine < nLine; iLine++ )
		{
			ProfileLine_t prfline = ProfileLinePeek( iLine );
			if ( prfline.buf )
			{
				fputs( prfline.buf, hFile );
			}
		}
		
		fclose( hFile );
		bStatus = true;
	}
	return bStatus;
}


static void InitDisasm (void)
{
	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();
}

//  _____________________________________________________________________________________
// |                                                                                     |
// |                           Public Functions                                          |
// |                                                                                     |
// |_____________________________________________________________________________________|

//===========================================================================
void DebugBegin ()
{
	// This is called every time the debugger is entered.

	GetDebuggerMemDC();

	g_nAppMode = MODE_DEBUG;
	GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_DISK_STATUS);

	if (GetMainCpu() == CPU_6502)
	{
		g_aOpcodes = & g_aOpcodes6502[ 0 ];		// Apple ][, ][+, //e
		g_aOpmodes[ AM_2 ].m_nBytes = 1;
		g_aOpmodes[ AM_3 ].m_nBytes = 1;
	}
	else
	{
		g_aOpcodes = & g_aOpcodes65C02[ 0 ];	// Enhanced Apple //e
		g_aOpmodes[ AM_2 ].m_nBytes = 2;
		g_aOpmodes[ AM_3 ].m_nBytes = 3;
	}

	InitDisasm();

	DebugVideoMode::Instance().Reset();
	UpdateDisplay( UPDATE_ALL );

	g_LBR = LBR_UNDEFINED;	// reset LBR, so LBR isn't stale from a previous debugging session

#if DEBUG_APPLE_FONT
	int iFG = 7;
	int iBG = 4;

//	DebuggerSetColorFG( aColors[ iFG ] );
//	DebuggerSetColorBG( aColors[ iBG ] );

	int iChar = 0;
	int x = 0;
	int y = 0;
	for (iChar = 0; iChar < 256; iChar++)
	{
		x = (iChar % 16);
		y = (iChar / 16);

		iFG = (x >> 1); // (iChar % 8);
		iBG = (y >> 1) & 7; // (iChar / 8) & 7;
		DebuggerSetColorFG( aConsoleColors[ iFG ] );
		DebuggerSetColorBG( aConsoleColors[ iBG ] );

		DebuggerPrintChar( x * (APPLE_FONT_WIDTH / 2), y * (APPLE_FONT_HEIGHT / 2), iChar );
	}
#endif
}

//===========================================================================
void DebugExitDebugger ()
{
	ClearTempBreakpoints();  // make sure we remove temp breakpoints before checking
	if (g_nBreakpoints == 0 && g_hTraceFile == NULL)
	{
		DebugEnd();
		return;
	}

	// Still have some BPs set or tracing to file, so continue single-stepping

	if (!g_bLastGoCmdWasFullSpeed)
		CmdGoNormalSpeed(0);
	else
		CmdGoFullSpeed(0);
}

//===========================================================================

static void CheckBreakOpcode ( int iOpcode )
{
	if (iOpcode == 0x00)	// BRK
		g_bDebugBreakpointHit |= IsDebugBreakOnInvalid(AM_IMPLIED) ? BP_HIT_INVALID : 0;

	if (g_aOpcodes[iOpcode].sMnemonic[0] >= 'a')	// All 6502/65C02 undocumented opcodes mnemonics are lowercase strings!
	{
		// Translate g_aOpcodes[iOpcode].nAddressMode into {AM_1, AM_2, AM_3}
		int iOpcodeType = AM_1;
		switch (g_aOpcodes[iOpcode].nAddressMode)
		{
			case AM_1:    //    Invalid 1 Byte
			case AM_IMPLIED:
				iOpcodeType = AM_1;
				break;
			case AM_2:    //    Invalid 2 Bytes
			case AM_M:    //  4 #Immediate
			case AM_Z:    //  6 Zeropage
			case AM_ZX:   //  9 Zeropage, X
			case AM_ZY:   // 10 Zeropage, Y
			case AM_R:    // 11 Relative
			case AM_IZX:  // 12 Indexed (Zeropage Indirect, X)
			case AM_NZY:  // 14 Indirect (Zeropage) Indexed, Y
			case AM_NZ:   // 15 Indirect (Zeropage)
				iOpcodeType = AM_2;
				break;
			case AM_3:    //    Invalid 3 Bytes
			case AM_A:    //  5 $Absolute
			case AM_AX:   //  7 Absolute, X
			case AM_AY:   //  8 Absolute, Y
			case AM_IAX:  // 13 Indexed (Absolute Indirect, X)
			case AM_NA:   // 16 Indirect (Absolute) i.e. JMP
				iOpcodeType = AM_3;
				break;
			default:
				_ASSERT(0);
		}
		g_bDebugBreakpointHit |= IsDebugBreakOnInvalid(iOpcodeType) ? BP_HIT_INVALID : 0;
	}

	// User wants to enter debugger on specific opcode? (NB. Can't be BRK)
	if (g_iDebugBreakOnOpcode && g_iDebugBreakOnOpcode == iOpcode)
		g_bDebugBreakpointHit |= BP_HIT_OPCODE;
}

static void UpdateLBR (void)
{
	const BYTE nOpcode = ReadByteFromMemory(regs.pc);

	bool isControlFlowOpcode =
		nOpcode == OPCODE_BRK ||
		nOpcode == OPCODE_JSR ||
		nOpcode == OPCODE_JMP_A ||
		nOpcode == OPCODE_RTI ||
		nOpcode == OPCODE_RTS ||
		nOpcode == OPCODE_JMP_NA;

	if (GetMainCpu() == CPU_65C02 && nOpcode == OPCODE_JMP_IAX)
		isControlFlowOpcode = true;

	if (g_aOpcodes[nOpcode].nAddressMode == AM_R)
	{
		if ((nOpcode == OPCODE_BRA)
		||  (nOpcode == OPCODE_BPL && !(regs.ps & AF_SIGN)    )
		||  (nOpcode == OPCODE_BMI &&  (regs.ps & AF_SIGN)    )
		||  (nOpcode == OPCODE_BVC && !(regs.ps & AF_OVERFLOW))
		||  (nOpcode == OPCODE_BVS &&  (regs.ps & AF_OVERFLOW))
		||  (nOpcode == OPCODE_BCC && !(regs.ps & AF_CARRY)   )
		||  (nOpcode == OPCODE_BCS &&  (regs.ps & AF_CARRY)   )
		||  (nOpcode == OPCODE_BNE && !(regs.ps & AF_ZERO)    )
		||  (nOpcode == OPCODE_BEQ &&  (regs.ps & AF_ZERO)    ))
		{
			isControlFlowOpcode = true; // Branch taken
		}
	}

	if (isControlFlowOpcode)
		g_LBR = regs.pc;
}

void DebugContinueStepping (const bool bCallerWillUpdateDisplay/*=false*/)
{
	static bool bForceSingleStepNext = false; // Allow at least one instruction to execute so we don't trigger on the same invalid opcode

	if (g_nDebugSkipLen > 0)
	{
		if ((regs.pc >= g_nDebugSkipStart) && (regs.pc < (g_nDebugSkipStart + g_nDebugSkipLen)))
		{
			// Enter turbo debugger mode -- UI not updated, etc.
			g_nDebugSteps = -1;
			g_nAppMode = MODE_STEPPING;
		}
		else
		{
			// Enter normal debugger mode -- UI updated every instruction, etc.
			g_nDebugSteps = 1;
			g_nAppMode = MODE_STEPPING;
		}
	}

	if (g_nDebugSteps)
	{
		bool bDoSingleStep = true;

		if (bForceSingleStepNext)
		{
			bForceSingleStepNext = false;
			g_bDebugBreakpointHit = BP_HIT_NONE;	// Don't show 'Stop Reason' msg a 2nd time
		}
		else if (GetActiveCpu() != CPU_Z80)
		{
			if (g_hTraceFile)
				OutputTraceLine();

			g_bDebugBreakpointHit = BP_HIT_NONE;

			if ( MemIsAddrCodeMemory(regs.pc) )
			{
				const BYTE nOpcode = ReadByteFromMemory(regs.pc);

				// Update profiling stats
				int nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;
				g_aProfileOpcodes[ nOpcode ].m_nCount++;
				g_aProfileOpmodes[ nOpmode ].m_nCount++;

				CheckBreakOpcode( nOpcode );	// Can set g_bDebugBreakpointHit
			}
			else
			{
				g_bDebugBreakpointHit = BP_HIT_PC_READ_FLOATING_BUS_OR_IO_MEM;
			}

			if (g_bDebugBreakpointHit)
			{
				bDoSingleStep = false;
				bForceSingleStepNext = true;	// Allow next single-step (after this) to execute
			}
		}

		if (bDoSingleStep)
		{
			UpdateLBR();
			const WORD oldPC = regs.pc;

			SingleStep(g_bGoCmd_ReinitFlag);
			g_bGoCmd_ReinitFlag = false;

			if (IsInterruptInLastExecution())
			{
				g_LBR = oldPC;
				if (g_bDebugBreakOnInterrupt)
					g_bDebugBreakpointHit |= BP_HIT_INTERRUPT;
			}

			g_bDebugBreakpointHit |= CheckBreakpointsIO() | CheckBreakpointsReg() | CheckBreakpointsVideo() | CheckBreakpointsDmaToOrFromIOMemory() | CheckBreakpointsDmaToOrFromMemory(-1);
		}

		if (regs.pc == g_nDebugStepUntil || g_bDebugBreakpointHit)
		{
			std::string stopReason = "Unknown!";
			bool skipStopReason = false;

			if (regs.pc == g_nDebugStepUntil)
				stopReason = StrFormat( CHC_DEFAULT "Register " CHC_REGS "PC" CHC_DEFAULT " matches '" CHC_INFO "Go until" CHC_DEFAULT "' address $" CHC_ADDRESS "%04X", g_nDebugStepUntil);
			else if (g_bDebugBreakpointHit & BP_HIT_INVALID)
				stopReason = "Invalid opcode";
			else if (g_bDebugBreakpointHit & BP_HIT_OPCODE)
				stopReason = StrFormat("Opcode match at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", regs.pc);
			else if (g_bDebugBreakpointHit & BP_HIT_REG)
			{
					if (g_pDebugBreakpointHit)
					{
						int iBreakpoint = (g_pDebugBreakpointHit - g_aBreakpoints);
						stopReason = StrFormat( "Register %s%s%s matches breakpoint %s#%s%d",
							CHC_REGS,
							g_aBreakpointSource[ g_pDebugBreakpointHit->eSource ],
							CHC_DEFAULT,
							CHC_ARG_SEP,
							CHC_NUM_HEX,
							iBreakpoint
						);
					}
					else
						stopReason = "Register matches value";
			}
			else if (g_bDebugBreakpointHit & BP_HIT_MEM)
				stopReason = StrFormat("Memory access at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", g_uBreakMemoryAddress);
			else if (g_bDebugBreakpointHit & BP_HIT_MEMW)
				stopReason = StrFormat("Write access at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", g_uBreakMemoryAddress);
			else if (g_bDebugBreakpointHit & BP_HIT_MEMR)
				stopReason = StrFormat("Read access at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", g_uBreakMemoryAddress);
			else if (g_bDebugBreakpointHit & BP_HIT_PC_READ_FLOATING_BUS_OR_IO_MEM)
				stopReason = "PC reads from floating bus or I/O memory";
			else if (g_bDebugBreakpointHit & BP_HIT_INTERRUPT)
				stopReason = (g_LBR == LBR_UNDEFINED)	? StrFormat("Interrupt occurred (LBR unknown)")
														: StrFormat("Interrupt occurred at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", g_LBR);
			else if (g_bDebugBreakpointHit & BP_HIT_VIDEO_POS)
				stopReason = StrFormat("Video scanner position matches at vpos=$%04X", NTSC_GetVideoVertForDebugger());
			else if (g_bDebugBreakpointHit & BP_DMA_TO_IO_MEM)
				stopReason = StrFormat("HDD DMA to I/O memory or ROM at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X", g_DebugBreakOnDMAIO.memoryAddr);
			else if (g_bDebugBreakpointHit & BP_DMA_FROM_IO_MEM)
				stopReason = StrFormat("HDD DMA from I/O memory at " CHC_ARG_SEP "$" CHC_ADDRESS "%04X ", g_DebugBreakOnDMAIO.memoryAddr);
			else if (g_bDebugBreakpointHit & (BP_DMA_FROM_MEM | BP_DMA_TO_MEM))
				skipStopReason = true;

			if (!skipStopReason)
				ConsolePrintFormat( CHC_INFO "Stop reason: " CHC_DEFAULT "%s", stopReason.c_str() );

			for (int i = 0; i < NUM_BREAK_ON_DMA; i++)
			{
				int nDebugBreakpointHit = CheckBreakpointsDmaToOrFromMemory(i);
				if (nDebugBreakpointHit)
				{
					if (nDebugBreakpointHit & BP_DMA_TO_MEM)
						stopReason = StrFormat("HDD DMA to memory " CHC_ARG_SEP "$" CHC_ADDRESS "%04X" CHC_ARG_SEP "-" CHC_ADDRESS "%04X" CHC_DEFAULT " (breakpoint %s#%s%d%s)", g_DebugBreakOnDMA[i].memoryAddr, g_DebugBreakOnDMA[i].memoryAddrEnd, CHC_ARG_SEP, CHC_NUM_HEX, g_DebugBreakOnDMA[i].BPid, CHC_DEFAULT);
					else if (nDebugBreakpointHit & BP_DMA_FROM_MEM)
						stopReason = StrFormat("HDD DMA from memory " CHC_ARG_SEP "$" CHC_ADDRESS "%04X" CHC_ARG_SEP "-" CHC_ADDRESS "%04X" CHC_DEFAULT " (breakpoint %s#%s%d%s)", g_DebugBreakOnDMA[i].memoryAddr, g_DebugBreakOnDMA[i].memoryAddrEnd, CHC_ARG_SEP, CHC_NUM_HEX, g_DebugBreakOnDMA[i].BPid, CHC_DEFAULT);
					ConsolePrintFormat( CHC_INFO "Stop reason: " CHC_DEFAULT "%s", stopReason.c_str() );
				}
			}

			ConsoleUpdate();

			g_nDebugSteps = 0;
		}

		if (g_nDebugSteps > 0)
			g_nDebugSteps--;
	}

	if (!g_nDebugSteps)
	{
		SoundCore_SetFade(FADE_OUT);	// NB. Call when MODE_STEPPING (not MODE_DEBUG) - see function

		g_nAppMode = MODE_DEBUG;
		GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_DISK_STATUS);
// BUG: PageUp, Trace - doesn't center cursor

		g_nDisasmCurAddress = regs.pc;

		DisasmCalcTopBotAddress();

		if (!bCallerWillUpdateDisplay)
			UpdateDisplay( UPDATE_ALL );
	}
}

//===========================================================================
void DebugStopStepping (void)
{
	_ASSERT(g_nAppMode == MODE_STEPPING);

	if (g_nAppMode != MODE_STEPPING)
		return;

	g_nDebugSteps = 0; // On next DebugContinueStepping(), stop single-stepping and transition to MODE_DEBUG
}

//===========================================================================
void DebugDestroy ()
{
	DebugEnd();
	FontsDestroy();

//	DeleteObject(g_hFontDisasm  );
//	DeleteObject(g_hFontDebugger);
//	DeleteObject(g_hFontWebDings);

	// TODO: Symbols_Clear()
	for ( int iTable = 0; iTable < NUM_SYMBOL_TABLES; iTable++ )
	{
		_CmdSymbolsClear( (SymbolTable_Index_e) iTable );
	}
	// TODO: DataDisassembly_Clear()

	ReleaseConsoleFontDC();
}


//===========================================================================
static void DebugEnd ()
{
	// Stepping ... calls us when key hit?!  FrameWndProc() ProcessButtonClick() DebugEnd()
	if (g_bProfiling)
	{
		// See: .csv / .txt note in CmdProfile()
		ProfileFormat( true, PROFILE_FORMAT_TAB ); // Export in Excel-ready text format.
		ProfileSave();
	}
	
	if (g_hTraceFile)
	{
		fclose(g_hTraceFile);
		g_hTraceFile = NULL;
	}

	g_vMemorySearchResults.clear();

	g_nAppMode = MODE_RUNNING;

	ReleaseDebuggerMemDC();
}


//===========================================================================
void DebugInitialize ()
{
	AssemblerOff(); // update prompt

#if _DEBUG
	uint32_t nError = 0;
#endif

#if _DEBUG
	nError = GetLastError();
#endif

	// Must select a bitmap into the temp DC !
//	HDC hTmpDC  = CreateCompatibleDC( FrameGetDC() );

#if _DEBUG
	nError = GetLastError();
#endif

	GetConsoleFontDC(); // Load font

	memset( g_aConsoleDisplay, 0, sizeof( g_aConsoleDisplay ) ); // CONSOLE_WIDTH * CONSOLE_HEIGHT );
	ConsoleInputReset();

	for ( int iWindow = 0; iWindow < NUM_WINDOWS; iWindow++ )
	{
		WindowSplit_t *pWindow = & g_aWindowConfig[ iWindow ];

		pWindow->bSplit = false;
		pWindow->eTop = (Window_e) iWindow;
		pWindow->eBot = (Window_e) iWindow;
	}

	g_iWindowThis = WINDOW_CODE;
	g_iWindowLast = WINDOW_CODE;

	WindowUpdateDisasmSize();

	ConfigColorsReset();

	WindowUpdateConsoleDisplayedSize();

	// CLEAR THE BREAKPOINT AND WATCH TABLES
	memset( g_aBreakpoints     , 0, MAX_BREAKPOINTS       * sizeof(Breakpoint_t));
	g_nBreakpoints = 0;
	memset( g_aWatches         , 0, MAX_WATCHES           * sizeof(Watches_t) );
	g_nWatches = 0;
	memset( g_aZeroPagePointers, 0, MAX_ZEROPAGE_POINTERS * sizeof(ZeroPagePointers_t));
	g_nZeroPagePointers = 0;

	// Load Main, Applesoft, and User Symbols
	g_bSymbolsDisplayMissingFile = false;

	g_iCommand = CMD_SYMBOLS_ROM;
	CmdSymbolsLoad(0);

	g_iCommand = CMD_SYMBOLS_APPLESOFT;
	CmdSymbolsLoad(0);

	// 2.9.2.5 Added: Symbol table A2_DOS33.SYM2
	g_iCommand = CMD_SYMBOLS_DOS33;
	CmdSymbolsLoad(0);

	// ,0x7,0xFF // Treat zero-page as data
	// $00 GOWARM   JSR ...
	// $01 LOC1 DW
	// $03 GOSTROUT JSR ...
	// $07..$B0
	// $B1 CHRGET
	// $C8
	// $C9 RNDSEED DW
	// $D0..$FF

	g_iCommand = CMD_SYMBOLS_USER_1;
	CmdSymbolsLoad(0);

	g_bSymbolsDisplayMissingFile = true;

#if OLD_FONT
	// CREATE A FONT FOR THE DEBUGGING SCREEN
	int nArgs = _Arg_1( g_sFontNameDefault );
#endif

	FontsInitialize();

	int iColor;
	
	iColor = FG_CONSOLE_OUTPUT;
	COLORREF nColor = g_aColorPalette[ g_aColorIndex[ iColor ] ];
	g_anConsoleColor[ CONSOLE_COLOR_x ] = nColor;

/*
	g_hFontDebugger = CreateFont( 
		  g_nFontHeight // Height
		, 0 // Width
		, 0 // Escapement
		, 0 // Orientatin
		, FW_MEDIUM // Weight
		, 0 // Italic
		, 0 // Underline
		, 0 // Strike Out
		, DEFAULT_CHARSET // "OEM_CHARSET"  DEFAULT_CHARSET
		, OUT_DEFAULT_PRECIS
		, CLIP_DEFAULT_PRECIS
		, ANTIALIASED_QUALITY // DEFAULT_QUALITY
		, FIXED_PITCH | FF_MODERN // HACK: MAGIC #: 4 // FIXED_PITCH
		, g_sFontNameDefault );

	g_hFontWebDings = CreateFont(
		  g_nFontHeight // Height
		, 0 // Width
		, 0 // Escapement
		, 0 // Orientatin
		, FW_MEDIUM // Weight
		, 0 // Italic
		, 0 // Underline
		, 0 // Strike Out
		, DEFAULT_CHARSET // ANSI_CHARSET  // OEM_CHARSET  DEFAULT_CHARSET
		, OUT_DEFAULT_PRECIS
		, CLIP_DEFAULT_PRECIS
		, ANTIALIASED_QUALITY // DEFAULT_QUALITY
		, DEFAULT_PITCH | FF_DECORATIVE // FIXED_PITCH | 4 | FF_MODERN
		, g_sFontNameBranch );
*/
//	if (g_hFontWebDings)
#if !USE_APPLE_FONT
	if (g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont)
	{
		g_iConfigDisasmBranchType = DISASM_BRANCH_FANCY;
	}
	else
	{
		g_iConfigDisasmBranchType = DISASM_BRANCH_PLAIN;
	}	
#endif

	//	ConsoleInputReset(); already called in DebugInitialize()

	VerifyDebuggerCommandTable();

	// Check all summary help to see if it fits within the console
	for (int iCmd = 0; iCmd < NUM_COMMANDS; iCmd++ )
	{	
		const char *pHelp = g_aCommands[ iCmd ].pHelpSummary;
		if (pHelp)
		{
			int nLen = strlen( pHelp ) + 2;
			if (nLen > (CONSOLE_WIDTH-1))
			{
				ConsoleBufferPushFormat( "Warning: %s help is %d chars", pHelp, nLen );
			}
		}
	}
	
#if _DEBUG
	//g_bConsoleBufferPaused = true;
#endif

	_Bookmark_Reset();

	static bool doneAutoRun = false;
	if (!doneAutoRun)	// Don't re-run on a VM restart
	{
		doneAutoRun = true;

		const std::string debuggerAutoRunName = "DebuggerAutoRun.txt";

		// Look in g_sCurrentDir, otherwise try g_sProgramDir

		std::string pathname = g_sCurrentDir + debuggerAutoRunName;
		errno_t error = strncpy_s(g_aArgs[1].sArg, MAX_PATH, pathname.c_str(), pathname.size());
		if (error != 0)
		{
			ConsolePrintFormat("%sPathname too long:", CHC_ERROR);
			ConsolePrintFormat("%s%s", CHC_STRING, pathname.c_str());
		}
		else
		{
			CmdOutputRun(1);
		}

		if (!g_bScriptReadOk)
		{
			pathname = g_sProgramDir + debuggerAutoRunName;
			error = strncpy_s(g_aArgs[1].sArg, MAX_PATH, pathname.c_str(), pathname.size());
			if (error != 0)
			{
				ConsolePrintFormat("%sPathname too long:", CHC_ERROR);
				ConsolePrintFormat("%s%s", CHC_STRING, pathname.c_str());
			}
			else
			{
				CmdOutputRun(1);
			}
		}
	}

	CmdMOTD(0);
}

//===========================================================================
void DebugReset (void)
{
	g_videoScannerDisplayInfo.Reset();
	g_LBR = LBR_UNDEFINED;
}

// Add character to the input line
//===========================================================================
void DebuggerInputConsoleChar ( char ch )
{
	_ASSERT(g_nAppMode == MODE_DEBUG);

	if (g_nAppMode != MODE_DEBUG)
		return;

	if (g_bConsoleBufferPaused)
		return;

	if (g_bIgnoreNextKey)
	{
		g_bIgnoreNextKey = false;
		return;
	}

	if (ch == CONSOLE_COLOR_ESCAPE_CHAR)
		return;

	if (g_nConsoleInputSkip == ch)
		return;

	if (ch == CHAR_SPACE)
	{
		// If don't have console input, don't pass space to the input line
		// exception: pass to assembler
		if ((! g_nConsoleInputChars) && (! g_bAssemblerInput))
			return;
	}
	
	if (g_nConsoleInputChars > g_nConsoleInputMaxLen)
		return;

	if ((ch >= CHAR_SPACE) && (ch <= 126)) // HACK MAGIC # 32 -> ' ', # 126 
	{
		if ((ch == TCHAR_QUOTE_DOUBLE) || (ch == TCHAR_QUOTE_SINGLE))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		if (!g_bConsoleInputQuoted)
		{
			// TODO: must fix param matching to ignore case
#if ALLOW_INPUT_LOWERCASE
#else
			ch = (char)CharUpper((LPTSTR)ch);
#endif
		}
		ConsoleInputChar( ch );

		DebuggerCursorNext();

		DrawConsoleInput();
		StretchBltMemToFrameDC();
	}
	else
	if (ch == 0x16) // HACK: Ctrl-V.  WTF!?
	{
		ProcessClipboardCommands();
	}
}


// Triggered when ENTER is pressed, or via script
//===========================================================================
Update_t DebuggerProcessCommand ( const bool bEchoConsoleInput )
{
	Update_t bUpdateDisplay = UPDATE_NOTHING;

	if (bEchoConsoleInput)
		ConsoleDisplayPush( ConsoleInputPeek() );

	if (g_bAssemblerInput)
	{
		if (g_nConsoleInputChars)
		{
			ParseInput( g_pConsoleInput, false ); // Don't cook the args
			bUpdateDisplay |= _CmdAssemble( g_nAssemblerAddress, 0, g_nArgRaw );
		}
		else
		{
			AssemblerOff();

			int nDelayedTargets = AssemblerDelayedTargetsSize();
			if (nDelayedTargets)
			{
				ConsoleDisplayPushFormat( " Asm: %d sym declared, not defined", nDelayedTargets );
				bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
			}
		}
		ConsoleInputReset();
		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY | UPDATE_CONSOLE_INPUT;
		ConsoleUpdate(); // udpate console, don't pause
	}
	else
	if (g_nConsoleInputChars)
	{
		// BufferedInputPush( 
		// Handle Buffered Input
		// while ( BufferedInputPeek() )
		int nArgs = ParseInput( g_pConsoleInput );
		if (nArgs == ARG_SYNTAX_ERROR)
		{
			bUpdateDisplay |= ConsoleDisplayErrorFormat( "Syntax error: %s", g_aArgs[0].sArg );
		}
		else
		{
			bUpdateDisplay |= ExecuteCommand( nArgs ); // ParseInput());
		}
		
		if (!g_bConsoleBufferPaused)
		{
			ConsoleInputReset();
		}
	}

	return bUpdateDisplay;
}

void ToggleFullScreenConsole ()
{
	// Switch to Console Window
	if (g_iWindowThis != WINDOW_CONSOLE)
	{
		CmdWindowViewConsole( 0 );
	}
	else // switch back to last window
	{
		CmdWindowLast( 0 );
	}
}

//===========================================================================
void DebuggerProcessKey ( int keycode )
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	if (DebugVideoMode::Instance().IsSet())
	{
		if ((VK_SHIFT == keycode) || (VK_CONTROL == keycode) || (VK_MENU == keycode))
		{
			return;
		}

		// Normally any key press takes us out of "Viewing Apple Output" g_nAppMode
		// VK_F# are already processed, so we can't use them to cycle next video g_nAppMode
//		    if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))

		// 2.9.2.7 Added: QoL for Debugger's view output screen.
		//    When using the debugger to view the ouput screen such as `HGR`, `HGR2`, etc. allow the
		//    keys 0-5 to display the specificed video # page, or 9 to see the current video mode.
		//      0 Pseudo   Page 0 ($0000 for graphics, else text page 1)
		//      1 Hardware Page 1 ($2000 for graphics, else text $0400)
		//      2 Hardware Page 2 ($4000 for graphics, else text $0800)
		//      3 Pseudo   Page 3 ($6000 for graphics, else text page 1)
		//      4 Pseudo   Page 4 ($8000 for graphics, else text page 1)
		//      5 Pseudo   Page 5 ($A000 for graphics, else text page 1)
		//      9 Current mode and page
		//
		// NOTE: Do we want to allow viewing mixed/full mode since 0-5 always sets fullscreen?
		//      7 Mixed-screen mode
		//      8 Full-screen mode
		//
		// NOTE: Keep in sync: ViewVideoPage_t, DebuggerProcessKey(), _ViewOutput()
		if ((keycode >= '0') && (keycode <= '9'))
		{
			ViewVideoPage_t eVideoPage  = VIEW_PAGE_X;
			UINT            bVideoFlags = 0;

			DebugVideoMode::Instance().Get( &bVideoFlags );
			uint32_t        bSavedVideoModeFlags = bVideoFlags;

			bVideoFlags &= ~(VF_MIXED | VF_PAGE0 | VF_PAGE2 | VF_PAGE3 | VF_PAGE4 | VF_PAGE5 | VF_PAGE6 | VF_PAGE7 | VF_PAGE8);
			switch (keycode)
			{
				case '0': eVideoPage = VIEW_PAGE_0; bVideoFlags |= VF_PAGE0; break;
				case '1': eVideoPage = VIEW_PAGE_1; /*                   */; break;
				case '2': eVideoPage = VIEW_PAGE_2; bVideoFlags |= VF_PAGE2; break;
				case '3': eVideoPage = VIEW_PAGE_3; bVideoFlags |= VF_PAGE3; break;
				case '4': eVideoPage = VIEW_PAGE_4; bVideoFlags |= VF_PAGE4; break;
				case '5': eVideoPage = VIEW_PAGE_5; bVideoFlags |= VF_PAGE5; break;
				case '6': eVideoPage = VIEW_PAGE_6; bVideoFlags |= VF_PAGE6; break;
				case '7': eVideoPage = VIEW_PAGE_7; bVideoFlags |= VF_PAGE7; break;
				case '8': eVideoPage = VIEW_PAGE_8; bVideoFlags |= VF_PAGE8; break;
				case '9': /* Don't use VIEW_PAGE_X as it is handled below*/; break;
				default:
					bool bUnknownViewVideoPage = false;
					assert( bUnknownViewVideoPage );
					break;
			}

			if (keycode == '9')
			{
				GetFrame().VideoRedrawScreen(); // See: CmdWindowViewOutput()
			}
			else
			{
				_ViewOutput( eVideoPage, bVideoFlags );
				DebugDisplay();
			}

			// We need to restore the video mode since the original output may be mixed mode
			// but switching to page 0-5 will have set full mode.
			DebugVideoMode::Instance().Set( bSavedVideoModeFlags );
			g_bIgnoreNextKey = true;
		}
		else
		{
			GetVideo().ClearSHRResidue();	// Clear the framebuffer to remove any SHR residue in the borders
			DebugVideoMode::Instance().Reset();

			// Technically this is a bug/feature: Leaving the debugger view output can sometimes be in the wrong view mode
			// GetFrame().VideoRedrawScreen();
			// DebugVideoMode::Instance().Set( GetVideo().GetVideoMode() );

			UpdateDisplay( UPDATE_ALL ); // 1
		}

		return;
	}

	Update_t bUpdateDisplay = UPDATE_NOTHING;

	// For long output, allow user to read it
	if (g_nConsoleBuffer)
	{
		if ((VK_SPACE == keycode) || (VK_RETURN == keycode) || (VK_TAB == keycode) || (VK_ESCAPE == keycode))
		{		
			int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayLines - 1 ); // was -2
			if (VK_ESCAPE == keycode) // user doesn't want to read all this stu
			{
				nLines = g_nConsoleBuffer;
			}
			ConsoleBufferTryUnpause( nLines );

			// don't really need since 'else if (keycode = VK_BACK)' but better safe then sorry
			keycode = 0; // don't single-step 
		}

		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY | UPDATE_CONSOLE_INPUT;
		ConsoleDisplayPause();
	}
	else
 	// If have console input, don't invoke curmovement
	// TODO: Probably should disable all "movement" keys to map them to line editing g_nAppMode
 	if ((keycode == VK_SPACE) && g_nConsoleInputChars)
		return;
	else if (keycode == VK_ESCAPE)
	{
		g_bConsoleInputQuoted = false;
		ConsoleInputReset();
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_BACK)
	{
		// Note: Checks prev char if QUTOE - SINGLE or DOUBLE
//		ConsoleUpdateCursor( CHAR_SPACE );
		if (! ConsoleInputBackSpace())
		{
			// CmdBeep();
		}
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_RETURN)
	{
//		ConsoleUpdateCursor( 0 );

		if (! g_nConsoleInputChars)
		{
			// bugfix: 2.6.1.35 Fixed: Pressing enter on blank line while in assembler wouldn't exit it.
			if ( g_bAssemblerInput )
			{
				bUpdateDisplay |= DebuggerProcessCommand( false );
			}
			else
			{
				ToggleFullScreenConsole();
				bUpdateDisplay |= UPDATE_ALL;
			}
		}
		else
		{
			ConsoleScrollEnd();
			bUpdateDisplay |= DebuggerProcessCommand( true ); // copy console input to console output

			// BUGFIX: main disassembly listing doesn't get updated in full screen console
			//bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
			bUpdateDisplay |= UPDATE_ALL;
		}		
	}
	else if (( keycode == VK_OEM_3 ) ||	// US: Tilde ~ (key to the immediate left of numeral 1)
			 ( keycode == VK_OEM_8 ))	// UK: Logical NOT ¬ (key to the immediate left of numeral 1)
	{
		if (KeybGetCtrlStatus())
		{
			ToggleFullScreenConsole();
			bUpdateDisplay |= UPDATE_ALL;
		}
		else
		{
			g_nConsoleInputSkip = 0; // VK_OEM_3;
			DebuggerInputConsoleChar( '~' );
		}
		g_nConsoleInputSkip = '~'; // VK_OEM_3;
	}
	else
	{	
		switch (keycode)
		{
			case VK_TAB:
			{
				if (g_nConsoleInputChars)
				{
					// TODO: TabCompletionCommand()
					// TODO: TabCompletionSymbol()
					bUpdateDisplay |= ConsoleInputTabCompletion();
				}
				else
				if (KeybGetCtrlStatus() && KeybGetShiftStatus())
					bUpdateDisplay |= CmdWindowCyclePrev( 0 );
				else
				if (KeybGetCtrlStatus())
					bUpdateDisplay |= CmdWindowCycleNext( 0 );
				else
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				break;
			}
			case VK_SPACE:
				if (g_bAssemblerInput)
				{
//					if (g_nConsoleInputChars)
//					{
//						ParseInput( g_pConsoleInput, false ); // Don't cook the args
//						bUpdateDisplay |= _CmdAssemble( g_nAssemblerAddress, 0, g_nArgRaw );
//					}
				}
				else
				{				
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdStepOut(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdStepOver(0);
					else
						bUpdateDisplay |= CmdTrace(0);
				}
				break;

			case VK_HOME:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					ConsoleScrollHome();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollHome();
					}
					else
					{
						// Move cursor to start of console input
					}
				}
				else
				{
					// If you really want $000 at the top of the screen...
					// g_nDisasmTopAddress = _6502_MEM_BEGIN;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcBotFromTopAddress();

					g_nDisasmCurAddress = _6502_MEM_BEGIN;
					DisasmCalcTopBotAddress();
				}
				bUpdateDisplay |= UPDATE_DISASM;
				break;

			case VK_END:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					ConsoleScrollEnd();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollEnd();
					}
					else
					{
						// Move cursor to end of console input
					}
				}
				else
				{
					// If you really want $8000 at the top of the screen...
					// g_nDisasmTopAddress =  (_6502_MEM_END / 2) + 1;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcTopBotAddress();

					g_nDisasmCurAddress =  (_6502_MEM_END / 2) + 1;
					DisasmCalcTopBotAddress();
				}
				bUpdateDisplay |= UPDATE_DISASM;
				break;

			case VK_PRIOR: // VK_PAGE_UP
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollPageUp();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollPageUp();
					}
					else
					{
						// Scroll through console input history
						bUpdateDisplay |= ConsoleScrollUp( 3 );
					}
				}
				else
				{
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorPageUp256(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorPageUp4K(0);
					else		
						bUpdateDisplay |= CmdCursorPageUp(0);
				}
				break;

			case VK_NEXT: // VK_PAGE_DN
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollPageDn();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollPageDn();
					}
					else
					{
						// Scroll through console input history
						bUpdateDisplay |= ConsoleScrollDn( 3 );
					}
				}
				else
				{
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorPageDown256(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorPageDown4K(0);
					else		
						bUpdateDisplay |= CmdCursorPageDown(0);
				}
				break;

			case VK_UP:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollUp( 1 );
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollUp( 1 );
					}
					else
					{
						// TODO: FIXME: Scroll through console input history
					}
				}
				else
				{
					// Shift the Top offset up by 1 byte
					// i.e. no smart disassembly like LineUp()
					// Normally UP moves to the previous "line" which may be multiple bytes.
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorLineUp(1);
					else
						bUpdateDisplay |= CmdCursorLineUp(0); // smart disassembly
				}
				break;

			case VK_DOWN:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollDn( 1 );
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollDn( 1 );
					}
					else
					{
						// TODO: FIXME: Scroll through console input history
					}
				}
				else
				{
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorRunUntil(0);
					else
					if (KeybGetShiftStatus())
						// Shift the Offest down by 1 byte
						// i.e. no smart disassembly like LineDown()
						bUpdateDisplay |= CmdCursorLineDown(1);
					else
						bUpdateDisplay |= CmdCursorLineDown(0);
				}
				break;

			case VK_RIGHT:
				if (KeybGetCtrlStatus())
					bUpdateDisplay |= CmdCursorSetPC(0/*unused*/);
				else
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_TOP );
				else
				if (KeybGetAltStatus())
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				else
					bUpdateDisplay |= CmdCursorFollowTarget( CURSOR_ALIGN_TOP );
				break;

			case VK_LEFT:
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_TOP ); // Jump to Caller
				else
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_CENTER );
				break;

			default:
				if ((keycode >= '0') && (keycode <= '9'))
				{
					int nArgs = 1;
					int iBookmark = keycode - '0';
					if (KeybGetCtrlStatus() && KeybGetShiftStatus())
					{
						nArgs = 2;
						g_aArgs[ 1 ].nValue = iBookmark;
						g_aArgs[ 2 ].nValue = g_nDisasmCurAddress;
						bUpdateDisplay |= CmdBookmarkAdd( nArgs );
						g_bIgnoreNextKey = true;
					}
					else
					if (KeybGetCtrlStatus())
					{
						nArgs = 1;
						g_aArgs[ 1 ].nValue = iBookmark;
						bUpdateDisplay |= CmdBookmarkGoto( nArgs );
						g_bIgnoreNextKey = true;
					}
				}

				break;
		} // switch
	}

	if (bUpdateDisplay && !DebugVideoMode::Instance().IsSet()) //  & UPDATE_BACKGROUND)
		UpdateDisplay( bUpdateDisplay );
}

void DebugDisplay ( BOOL bInitDisasm/*=FALSE*/ )
{
	if (bInitDisasm)
		InitDisasm();

	if (DebugVideoMode::Instance().IsSet())
	{
		uint32_t mode = 0;
		DebugVideoMode::Instance().Get(&mode);
		GetFrame().VideoRefreshScreen(mode, true);
		return;
	}

	UpdateDisplay( UPDATE_ALL );
}


//===========================================================================
void DebuggerUpdate ()
{
	DebuggerCursorUpdate();
}


//===========================================================================
void DebuggerCursorUpdate ()
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	const  int nUpdatesPerSecond = 4;
	const  uint32_t nUpdateInternal_ms = 1000 / nUpdatesPerSecond;
	static uint32_t nBeg = GetTickCount(); // timeGetTime();
	       uint32_t nNow = GetTickCount(); // timeGetTime();

	if (((nNow - nBeg) >= nUpdateInternal_ms) && !DebugVideoMode::Instance().IsSet())
	{
		nBeg = nNow;
		
		DebuggerCursorNext();

		DrawConsoleCursor();
		StretchBltMemToFrameDC();
	}
	else
	{
		Sleep(1);		// Stop process hogging CPU
	}
}


//===========================================================================
void DebuggerCursorNext ()
{
	g_bInputCursor ^= true;
	if (g_bInputCursor)
		ConsoleUpdateCursor( g_aInputCursor[ g_iInputCursor ] );
	else
		ConsoleUpdateCursor( 0 ); // show char under cursor
}


//===========================================================================
//char DebuggerCursorGet ()
//{
//	return g_aInputCursor[ g_iInputCursor ];
//}


//===========================================================================
bool IsDebugSteppingAtFullSpeed (void)
{
	return (g_nAppMode == MODE_STEPPING) && g_bDebugFullSpeed;
}
