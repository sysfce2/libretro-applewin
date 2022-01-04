//
//  sdldummyframe.cpp
//  Mariani
//
//  Created by sh95014 on 1/2/22.
//

#include "StdAfx.h"
#include "sdldummyframe.hpp"
#include "linux/resources.h"
#include "AppDelegate.h"

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

  void SDLDummyFrame::ProcessKeyDown(const SDL_KeyboardEvent & key)
  {
  }

  void SDLDummyFrame::ProcessText(const SDL_TextInputEvent & text)
  {
  }

  int SDLDummyFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
  {
    int returnValue = ShowModalAlertOfType(uType, lpCaption, lpText);
    ResetSpeed();
    return returnValue;
  }

  void SDLDummyFrame::FrameDrawDiskLEDS()
  {
    UpdateDriveLights();
  }

  std::string SDLDummyFrame::getResourcePath(const std::string & filename)
  {
    return std::string(PathToResourceNamed(filename.c_str()));
  }

  std::string SDLDummyFrame::Video_GetScreenShotFolder()
  {
    return {};
  }

}
