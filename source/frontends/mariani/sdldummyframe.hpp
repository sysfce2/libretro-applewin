//
//  sdldummyframe.hpp
//  Mariani
//
//  Created by sh95014 on 1/2/22.
//

#pragma once

#include "sdlframe.h"

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  class SDLDummyFrame : public SDLFrame
  {
  public:
    SDLDummyFrame(const common2::EmulatorOptions & options);

    void VideoPresentScreen() override;
    void Initialize(bool resetVideoState) override;

    void ProcessKeyDown(const SDL_KeyboardEvent & key);
    void ProcessText(const SDL_TextInputEvent & text);

    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;

    void FrameDrawDiskLEDS() override;
      
    BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize) override;
    const std::string GetBitmapFilename(LPCSTR lpBitmapName) override;

  protected:
    void GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const override;

  private:
    // FIXME: without this hack the app crashes randomly elsewhere
    Uint8 padding[1];
  };

}
