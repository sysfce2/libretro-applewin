#include "StdAfx.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"

#include "Log.h"
#include "windows.h"
#include "frontends/qt/applicationname.h"

#include <boost/property_tree/ini_parser.hpp>

#include <sys/stat.h>

namespace
{

  class Configuration : public common2::PTreeRegistry
  {
  public:
    Configuration(const std::string & filename, const bool saveOnExit);
    ~Configuration();

  private:
    const std::string myFilename;
    bool mySaveOnExit;
  };

  Configuration::Configuration(const std::string & filename, const bool saveOnExit) : myFilename(filename), mySaveOnExit(saveOnExit)
  {
    if (GetFileAttributes(myFilename.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
      boost::property_tree::ini_parser::read_ini(myFilename, myINI);
    }
    else
    {
      LogFileOutput("Registry: configuration file '%s' not found\n", myFilename.c_str());
    }
  }

  Configuration::~Configuration()
  {
    if (mySaveOnExit)
    {
      try
      {
        boost::property_tree::ini_parser::write_ini(myFilename, myINI);
      }
      catch(const std::exception& e)
      {
        LogFileOutput("Registry: cannot save settings to '%s': %s\n", myFilename.c_str(), e.what());
      }
    }
  }

}

namespace common2
{

  std::string GetHomeDir()
  {
    const char* homeDir = getenv("HOME");
    if (!homeDir)
    {
      throw std::runtime_error("${HOME} not set, cannot locate configuration file");
    }

    return std::string(homeDir);
  }

  std::string GetConfigFile(const std::string & filename)
  {
    const std::string dir = GetHomeDir() + "/.applewin";
    const int status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (!status || (errno == EEXIST))
    {
      return dir + "/" + filename;
    }
    else
    {
      const char * s = strerror(errno);
      LogFileOutput("No registry. Cannot create %s in %s: %s\n", filename.c_str(), dir.c_str(), s);
      return std::string();
    }
  }

  std::shared_ptr<Registry> CreateFileRegistry(const EmulatorOptions & options)
  {
    const std::string homeDir = GetHomeDir();

    std::string filename;
    bool saveOnExit;

    if (options.useQtIni)
    {
      filename = homeDir + "/.config/" + ORGANIZATION_NAME + "/" + APPLICATION_NAME + ".conf";
      saveOnExit = false;
    }
    else
    {
      filename = options.configurationFile;
      saveOnExit = true;
    }

    std::shared_ptr<Configuration> config = std::make_shared<Configuration>(filename, saveOnExit);
    config->addExtraOptions(options.registryOptions);

    return config;
  }

}
