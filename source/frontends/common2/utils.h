#pragma once

#include <string>

namespace common2
{
  struct Geometry;

  std::string GetConfigFile(const std::string & filename);
  std::string GetHomeDir();

  void setSnapshotFilename(const std::string & filename);

  void loadGeometryFromRegistry(const std::string &section, Geometry & geometry);
  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry);
}
