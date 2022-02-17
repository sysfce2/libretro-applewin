#include "StdAfx.h"
#include "frontends/common2/programoptions.h"
#include "linux/version.h"

#include "Memory.h"

#include <getopt.h>
#include <regex>
#include <iostream>
#include <iomanip>

namespace
{

  struct OptionData_t
  {
    const char * name;
    int has_arg;
    int val;
    const char * description;
  };

  bool isShort(const int val)
  {
    return val >= 0 && val <= 0xFF;
  }

  void printHelp(const std::vector<std::pair<std::string, std::vector<OptionData_t>>> & data)
  {
    for (const auto & categories : data)
    {
      std::cout << categories.first << ":" << std::endl;
      for (const auto & option : categories.second)
      {
        std::cout << "  ";
        std::ostringstream value;
        if (isShort(option.val))
        {
          value << "[ -" << char(option.val) << " ] ";
        }
        value << "--" << option.name;
        if (option.has_arg == required_argument)
        {
          value << " arg";
        }
        std::cout << std::left << std::setw(25) << value.str() << "\t" << option.description << std::endl;
      }
      std::cout << std::endl;
    }
  }

  void parseGeometry(const std::string & s, common2::Geometry & geometry)
  {
    std::smatch m;
    if (std::regex_match(s, m, std::regex("^(\\d+)x(\\d+)(\\+(\\d+)\\+(\\d+))?$")))
    {
      const size_t groups = m.size();
      if (groups == 6)
      {
        geometry.width = std::stoi(m.str(1));
        geometry.height = std::stoi(m.str(2));
        if (!m.str(3).empty())
        {
          geometry.x = std::stoi(m.str(4));
          geometry.y = std::stoi(m.str(5));
        }
        return;
      }
    }
    throw std::runtime_error("Invalid geometry: " + s);
  }

  void extractOptions(
    const std::vector<std::pair<std::string, std::vector<OptionData_t>>>  & data,
    std::vector<option>                                                   & longOptions,
    std::string                                                           & shortOptions
    )
  {
    longOptions.clear();
    std::ostringstream shorts;
    for (const auto & categories : data)
    {
      for (const auto & option : categories.second)
      {
        longOptions.push_back({option.name, option.has_arg, nullptr, option.val});
        const int val = option.val;
        if (isShort(val))
        {
          shorts << char(val);
          if (option.has_arg == required_argument)
          {
            shorts << ":";
          }
        }
      }
    }
    longOptions.push_back({nullptr, 0, nullptr, 0});
    shortOptions = shorts.str();
  }

}

namespace common2
{

  bool getEmulatorOptions(int argc, char * argv [], const std::string & edition, EmulatorOptions & options)
  {
    const int FIXED_SPEED = 1000;
    const int SDL_DRIVER  = 1001;
    const int GL_SWAP     = 1002;
    const int NO_IMGUI    = 1003;
    const int MEM_CLEAR   = 1004;
    const int GEOMETRY    = 1005;
    const int NO_SQUARING = 1006;
    const int DEVICE_NAME = 1007;
    const int HEADLESS    = 1008;
    const int ROM         = 1009;
    const int F8ROM       = 1010;

    const std::string name = "Apple Emulator for " + edition + " (based on AppleWin " + getVersion() + ")";

    const std::vector<std::pair<std::string, std::vector<OptionData_t>>> allOptions =
      {
        {
          name.c_str(),
          {
            {"help",            no_argument,          'h',            "Print this help message"},
          }
        },
        {
          "General",
          {
            {"log",             no_argument,          'l',            "Log to AppleWin.log"},
            {"conf",            required_argument,    'c',            "Select configuration file"},
            {"qt-ini",          no_argument,          'q',            "Use Qt ini file (read only)"},
            {"registry",        required_argument,    'r',            "Registry options section.path=value"},
          }
        },
        {
          "Emulator",
          {
            {"mem-clear",       required_argument,    MEM_CLEAR,      "Memory initialization pattern [0..7]"},
            {"fixed-speed",     no_argument,          FIXED_SPEED,    "Fixed (non-adaptive) speed"},
            {"no-squaring",     no_argument,          NO_SQUARING,    "Gamepad range is (already) a square"},
            {"device-name",     required_argument,    DEVICE_NAME,    "Gamepad device name"},
            {"headless",        no_argument,          HEADLESS,       "Headless: disable video (freewheel)"},
            {"ntsc",            no_argument,          'n',            "NTSC: execute NTSC code"},
            {"benchmark",       no_argument,          'b',            "Benchmark emulator"},
            {"rom",             required_argument,    ROM,            "Custom 12k/16k ROM"},
            {"f8rom",           required_argument,    F8ROM,          "Custom 2k ROM"},
          }
        },
        {
          "Disk",
          {
            {"d1",              required_argument,    '1',            "Disk in S6D1 drive"},
            {"d2",              required_argument,    '2',            "Disk in S6D2 drive"},
          }
        },
        {
          "Snapshot",
          {
            {"state-filename",  required_argument,    'f',            "Set snapshot filename"},
            {"load-state",      required_argument,    's',            "Load snapshot from file"},
          }
        },
        {
          "SDL",
          {
            {"sdl-driver",      required_argument,    SDL_DRIVER,     "SDL driver"},
            {"gl-swap",         required_argument,    GL_SWAP,        "SDL_GL_SwapInterval"},
            {"no-imgui",        no_argument,          NO_IMGUI,       "Plain SDL2 renderer"},
            {"geometry",        required_argument,    GEOMETRY,       "WxH[+X+Y]"},
          }
        }
      };

    std::vector<option> longOptions;
    std::string shortOptions;
    extractOptions(allOptions, longOptions, shortOptions);

    options.imgui = true;
    options.paddleSquaring = true;

    while (1)
    {
      int optionIndex = 0;
      const int c = getopt_long(argc, argv, shortOptions.c_str(), longOptions.data(), &optionIndex);
      if (c == -1)
      {
        return true;
      }
      switch (c)
      {
        case '?':
        case 'h':
        {
          printHelp(allOptions);
          return false;
        }
        case 'c':
        {
          options.configurationFile = optarg;
          break;
        }
        case 'l':
        {
          options.log = true;
          break;
        }
        case '1':
        {
          options.disk1 = optarg;
          break;
        }
        case '2':
        {
          options.disk2 = optarg;
          break;
        }
        case 'f':
        {
          options.snapshotFilename = optarg;
          options.loadSnapshot = false;
          break;
        }
        case 's':
        {
          options.snapshotFilename = optarg;
          options.loadSnapshot = true;
          break;
        }
        case 'q':
        {
          options.useQtIni = true;
          break;
        }
        case 'r':
        {
          options.registryOptions.emplace_back(optarg);
          break;
        }
        case 'n':
        {
          options.ntsc = true;
          break;
        }
        case 'b':
        {
          options.benchmark = true;
          break;
        }
        case FIXED_SPEED:
        {
          options.fixedSpeed = true;
          break;
        }
        case SDL_DRIVER:
        {
          options.sdlDriver = std::stoi(optarg);
          break;
        }
        case GL_SWAP:
        {
          options.glSwapInterval = std::stoi(optarg);
          break;
        }
        case NO_IMGUI:
        {
          options.imgui = false;
          break;
        }
        case MEM_CLEAR:
        {
          const int memclear = std::stoi(optarg);
          if (memclear >=0 && memclear < NUM_MIP)
          {
            options.memclear = memclear;
          }
          break;
        }
        case GEOMETRY:
        {
          options.geometry.empty = false;
          parseGeometry(optarg, options.geometry);
          break;
        }
        case NO_SQUARING:
        {
          options.paddleSquaring = 0;
          break;
        }
        case DEVICE_NAME:
        {
          options.paddleDeviceName = optarg;
          break;
        }
        case HEADLESS:
        {
          options.headless = true;
          break;
        }
        case ROM:
        {
          options.customRom = optarg;
          break;
        }
        case F8ROM:
        {
          options.customRomF8 = optarg;
          break;
        }
      }
    }

    return true;
  }

}
