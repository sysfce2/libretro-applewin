#include "StdAfx.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"

#include "Log.h"
#include "windows.h"
#include "frontends/qt/applicationname.h"

#include <boost/property_tree/ini_parser.hpp>

namespace
{

  void parseOption(const std::string & s, std::string & path, std::string & value)
  {
    const size_t pos = s.find('=');
    if (pos == std::string::npos)
    {
      throw std::runtime_error("Invalid option format: " + s + ", expected: section.key=value");
    }
    path = s.substr(0, pos);
    std::replace(path.begin(), path.end(), '_', ' ');
    value = s.substr(pos + 1);
    std::replace(value.begin(), value.end(), '_', ' ');
  }

  class Configuration : public common2::PTreeRegistry
  {
  public:
    Configuration(const std::string & filename, const bool saveOnExit);
    ~Configuration();

    void addExtraOptions(const std::vector<std::string> & options);

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
      LogFileOutput("Registry: configuration file '%s' not found\n", filename.c_str());
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

  void Configuration::addExtraOptions(const std::vector<std::string> & options)
  {
    for (const std::string & option : options)
    {
      std::string path, value;
      parseOption(option, path, value);
      myINI.put(path, value);
    }
  }

}

namespace common2
{


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

    std::shared_ptr<Configuration> config(new Configuration(filename, saveOnExit));
    config->addExtraOptions(options.registryOptions);

    return config;
  }

}
