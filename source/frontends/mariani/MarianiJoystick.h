//
//  MarianiJoystick.h
//  Mariani
//
//  Created by sh95014 on 2/10/22.
//

#pragma once

#include "linux/paddle.h"

namespace mariani
{

class Gamepad : public Paddle
{
public:
    bool getButton(int i) const override;
    double getAxis(int i) const override;
};

}
