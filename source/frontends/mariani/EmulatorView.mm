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

enum {
    ASCII_NUL = 0x00,
    ASCII_BS  = 0x08,  // ^H
    ASCII_HT  = 0x09,  // ^I
    ASCII_LF  = 0x0A,  // ^J
    ASCII_VT  = 0x0B,  // ^K
    ASCII_CR  = 0x0D,  // ^M
    ASCII_NAK = 0x15,  // ^U
    ASCII_ESC = 0x1B,  // ^[
    ASCII_SPC = 0x20,
    ASCII_A   = 0x41,
    ASCII_Z   = 0x5A,
    ASCII_a   = 0x61,
    ASCII_z   = 0x7A,
    ASCII_DEL = 0x7F,
};

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
    NSInteger ch = ASCII_NUL;
    
    switch (event.keyCode) {
        case kVK_ANSI_KeypadEnter:
        case kVK_Return:
            ch = ASCII_CR;
            break;
        case kVK_Delete:
            if (event.modifierFlags & NSEventModifierFlagOption) {
                ch = ASCII_DEL;
                break;
            }
            // fallthrough
        case kVK_LeftArrow:
            ch = ASCII_BS;
            break;
        case kVK_RightArrow:
            ch = ASCII_NAK;
            break;
        case kVK_UpArrow:
            ch = ASCII_VT;
            break;
        case kVK_DownArrow:
            ch = ASCII_LF;
            break;
        case kVK_Escape:
            ch = ASCII_ESC;
            break;
        case kVK_Tab:
            ch = ASCII_HT;
            break;
        default: {
            // maybe it's already a printable character
            unichar c = [event.charactersIgnoringModifiers characterAtIndex:0];
            if (event.modifierFlags & NSEventModifierFlagControl) {
                ch = toupper(c);
                if (ch >= ASCII_A && ch <= ASCII_Z) {
                    ch -= 0x40;  // A -> ^A
                }
                else if (ch >= ASCII_a && ch <= ASCII_z) {
                    ch -= 0x60;  // a -> ^A
                }
            }
            else if (c >= ASCII_SPC && c < ASCII_DEL) {
                ch = self.forceCapsLock ? toupper(c) : c;
            }
            break;
        }
    }
    if (ch != ASCII_NUL) {
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
            if (ch == ASCII_LF) {
                // pasted lines end with LF character, but we probably want to
                // paste a CR instead
                addKeyToBuffer(ASCII_CR);
            }
            else if (ch < ASCII_DEL) {
                addKeyToBuffer((BYTE)ch);
            }
        }
    }];
}

@end
