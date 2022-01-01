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

  protected:
    void GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const override;

  private:
    // FIXME: without this hack the app crashes randomly elsewhere
    Uint8 padding[1];
  };

}
