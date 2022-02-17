#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "linux/paddle.h"

#include "StdAfx.h"
#include "Utilities.h"
#include "Core.h"

namespace common2
{

  EmulatorOptions::EmulatorOptions()
  {
    memclear = g_nMemoryClearType;
    configurationFile = GetConfigFile("applewin.conf");
  }

  void applyOptions(const EmulatorOptions & options)
  {
    g_nMemoryClearType = options.memclear;

    LPCSTR szImageName_drive[NUM_DRIVES] = {nullptr, nullptr};
	  bool driveConnected[NUM_DRIVES] = {true, true};

    if (!options.disk1.empty())
    {
      szImageName_drive[DRIVE_2] = options.disk1.c_str();
    }

    if (!options.disk2.empty())
    {
      szImageName_drive[DRIVE_2] = options.disk2.c_str();
    }

    bool bBoot;
    InsertFloppyDisks(SLOT6, szImageName_drive, driveConnected, bBoot);

    if (!options.customRom.empty())
    {
      CloseHandle(g_hCustomRom);
      g_hCustomRom = CreateFile(options.customRom.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
      if (g_hCustomRom == INVALID_HANDLE_VALUE)
      {
        LogFileOutput("Init: Failed to load Custom ROM: %s\n", options.customRom.c_str());
      }
    }

    if (!options.customRomF8.empty())
    {
      CloseHandle(g_hCustomRomF8);
      g_hCustomRomF8 = CreateFile(options.customRomF8.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
      if (g_hCustomRomF8 == INVALID_HANDLE_VALUE)
      {
        LogFileOutput("Init: Failed to load custom F8 ROM: %s\n", options.customRomF8.c_str());
      }
    }

    Paddle::setSquaring(options.paddleSquaring);
  }

}
