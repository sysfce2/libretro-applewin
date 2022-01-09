//
//  EmulatorView.mm
//  Mariani
//
//  Created by sh95014 on 12/29/21.
//

#import "EmulatorView.h"
#import "Carbon/Carbon.h"
#import "linux/keyboard.h"
#import "linux/paddle.h"

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

// Going by the Apple ][ Reference Manual, the rules are basically:
// - the number keys are modified by SHIFT only
// - the letter keys are modified by CTRL only (not true for //e)
// - 0 is unmodified by CTRL or SHIFT, but that's not relevant to us
// - special keys like ← or → are sent as control characters
- (void)keyDown:(NSEvent *)event {
    NSInteger ch = 0;
    
    switch (event.keyCode) {
        case kVK_ANSI_KeypadEnter:
        case kVK_Return:
            ch = 0x0D;  // ^M
            break;
        case kVK_LeftArrow:
            ch = 0x08;  // ^H
            break;
        case kVK_Delete:
            ch = 0x7F;
            break;
        case kVK_RightArrow:
            ch = 0x15;  // ^U
            break;
        case kVK_UpArrow:
            ch = 0x0B;  // ^K
            break;
        case kVK_DownArrow:
            ch = 0x0A;  // ^J
            break;
        case kVK_Escape:
            ch = 0x1B;
            break;
        case kVK_Tab:
            ch = 0x09;  // ^I
            break;
        default: {
            // maybe it's already a printable character
            unichar c = [event.characters characterAtIndex:0];
            if (event.modifierFlags & NSEventModifierFlagControl) {
                ch = toupper(c);
                if (ch >= 'A' && ch <= 'Z') {
                    ch -= 0x40;  // A -> ^A
                }
            }
            else if (c >= 0x20 && c <= 0x7E) {
                ch = self.forceCapsLock ? toupper(c) : c;
            }
            break;
        }
    }
    if (ch != 0) {
        addKeyToBuffer(ch);
    }
}

- (void)flagsChanged:(NSEvent *)event
{
    switch (event.keyCode) {
        case kVK_CapsLock:
            self.forceCapsLock = NO;
            break;
        case kVK_Option:
            if (event.modifierFlags & NSEventModifierFlagOption) {
                Paddle::setButtonPressed(Paddle::ourOpenApple);
            }
            else {
                Paddle::setButtonReleased(Paddle::ourOpenApple);
            }
            break;
        case kVK_RightOption:
            if (event.modifierFlags & NSEventModifierFlagOption) {
                Paddle::setButtonPressed(Paddle::ourSolidApple);
            }
            else {
                Paddle::setButtonReleased(Paddle::ourSolidApple);
            }
            break;
        default:
            break;
    }
}

- (void)addStringToKeyboardBuffer:(NSString *)string {
    [string enumerateSubstringsInRange:NSMakeRange(0, string.length)
                               options:NSStringEnumerationByComposedCharacterSequences
                            usingBlock:^(NSString * _Nullable substring, NSRange substringRange, NSRange enclosingRange, BOOL * _Nonnull stop) {
        // filter out multi-byte characters
        if (substringRange.length == 1) {
            unichar ch = [substring characterAtIndex:0];
            if (ch == 0x0A) {
                // pasted lines end with LF character, but we probably want to
                // paste a CR instead
                addKeyToBuffer(0x0D);
            }
            else if (ch <= 0x7E) {
                addKeyToBuffer((BYTE)ch);
            }
        }
    }];
}

@end
