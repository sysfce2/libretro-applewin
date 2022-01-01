//
//  sdldummyframe.cpp
//  Mariani
//
//  Created by sh95014 on 1/2/22.
//

#include "StdAfx.h"
#include "sdldummyframe.hpp"

namespace sa2
{

  SDLDummyFrame::SDLDummyFrame(const common2::EmulatorOptions & options)
    : SDLFrame(options)
  {
  }

  void SDLDummyFrame::Initialize(bool resetVideoState)
  {
    SDLFrame::Initialize(resetVideoState);
  }

  void SDLDummyFrame::VideoPresentScreen()
  {
  }

  void SDLDummyFrame::GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const
  {
  }


}
