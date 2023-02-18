#pragma once

#include "linux/linuxframe.h"
#include "Common.h"
#include <vector>
#include <string>

namespace common2
{

  class CommonFrame : public LinuxFrame
  {
  public:
    BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize) override;
    virtual void LoadSnapshot();

    virtual void ResetSpeed();
    bool ChangeMode(const AppMode_e mode);

  protected:
    virtual std::string getResourcePath(const std::string & filename) = 0;

    static std::string getBitmapFilename(const std::string & resource);

    std::vector<BYTE> myResource;
  };

}
