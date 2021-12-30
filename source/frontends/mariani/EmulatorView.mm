//
//  EmulatorView.m
//  Mariani
//
//  Created by sh95014 on 12/29/21.
//

#import "EmulatorView.h"
#import "Carbon/Carbon.h"
#import "linux/keyboard.h"

@interface EmulatorView ()
    @property BOOL forceCapsLock;
@end

@implementation EmulatorView


- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder])) {
        self.forceCapsLock = YES;
    }
    return self;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    NSInteger ch = 0;
    
    switch (event.keyCode) {
        case kVK_ANSI_KeypadEnter:
        case kVK_Return:
            ch = 0x0D;
            break;
        case kVK_LeftArrow:
            ch = 0x08;
            break;
        case kVK_RightArrow:
            ch = 0x15;
            break;
        case kVK_UpArrow:
            ch = 0x0B;
            break;
        case kVK_DownArrow:
            ch = 0x0A;
            break;
        case kVK_Delete:
            ch = 0x7F;
            break;
        case kVK_Escape:
            ch = 0x1B;
            break;
        case kVK_Tab:
            ch = 0x09;
            break;
        case kVK_CapsLock:
            self.forceCapsLock = NO;
            break;
    }
    if (ch == 0) {
        // maybe it's already a printable character
        unichar c = [event.characters characterAtIndex:0];
        if (c >= 0x20 && c <= 0x7E) {
            ch = self.forceCapsLock ? toupper(c) : c;
        }
    }
    if (ch != 0) {
        addKeyToBuffer(ch);
    }
}

@end
