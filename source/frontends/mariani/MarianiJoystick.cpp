//
//  MarianiJoystick.cpp
//  Mariani
//
//  Created by sh95014 on 2/10/22.
//

#include "MarianiJoystick.h"
#include "AppDelegate.h"

namespace mariani
{

bool Gamepad::getButton(int i) const
{
    return GamepadGetButton(i);
}

double Gamepad::getAxis(int i) const
{
    return GamepadGetAxis(i);
}

}
