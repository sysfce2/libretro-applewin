//
//  NSMutableString+RTF.m
//  Mariani
//
//  Created by sh95014 on 2/12/22.
//

#import "NSMutableString+RTF.h"
#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

@interface NSMutableStringRTFIVars : NSObject
@property (assign, nonatomic) RTFTextColor rtfTextColor;
+ (NSMutableStringRTFIVars *)fetch:(id)targetInstance;
@end

@implementation NSMutableStringRTFIVars

@synthesize rtfTextColor;

+ (NSMutableStringRTFIVars*)fetch:(id)targetInstance
{
    static void *compactFetchIVarKey = &compactFetchIVarKey;
    NSMutableStringRTFIVars *ivars = objc_getAssociatedObject(targetInstance, &compactFetchIVarKey);
    if (ivars == nil) {
        ivars = [[NSMutableStringRTFIVars alloc] init];
        objc_setAssociatedObject(targetInstance, &compactFetchIVarKey, ivars, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return ivars;
}

@end

@implementation NSMutableString (RTF)

- (RTFTextColor)rtfTextColor {
    return [NSMutableStringRTFIVars fetch:self].rtfTextColor;
}

- (void)setRtfTextColor:(RTFTextColor)textColor {
    [NSMutableStringRTFIVars fetch:self].rtfTextColor = textColor;
}

- (void)appendCharacter:(unichar)character {
    [self appendFormat:@"%c", character];
}

static NSString *RTFFontFamily(NSFont *font)
{
    NSFontDescriptorSymbolicTraits traits = font.fontDescriptor.symbolicTraits;
    
    if (traits & NSFontDescriptorTraitMonoSpace) {
        return @"modern";
    }
    else if (traits & NSFontDescriptorClassScripts) {
        return @"script";
    }
    else if (traits & NSFontDescriptorClassOrnamentals) {
        return @"decor";
    }
    else if (traits & NSFontDescriptorClassSymbolic) {
        return @"tech";
    }
    else if (traits & NSFontDescriptorClassSansSerif) {
        return @"swiss";
    }
    else if (traits & (NSFontDescriptorClassOldStyleSerifs |
                       NSFontDescriptorClassTransitionalSerifs |
                       NSFontDescriptorClassModernSerifs |
                       NSFontDescriptorClassClarendonSerifs |
                       NSFontDescriptorClassSlabSerifs |
                       NSFontDescriptorClassFreeformSerifs)) {
        return @"roman";
    }
    return @"nil";
}

- (void)RTFBeginWithFonts:(NSArray *)fonts colors:(NSArray *)colors {
    NSString *rtfHdrStart =
@"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\deflangfe1033{";
    
    NSString *rtfHdrFontTableStart =
    @"\\fonttbl";
    
    NSString *rtfHdrFontTableFormat =
    @"{\\f%d\\f%@\\fprq%d\\fcharset%d %@;}";
    
    NSString *rtfHdrFontTableEnd =
    @"}\r\n";

    NSString *rtfColorTableStart =
@"{\\colortbl;";
    
    NSString *rtfColorTableFormat =
    @"\\red%.0f\\green%.0f\\blue%.0f;";
    
    NSString *rtfColorTableEnd =
    @"}\r\n";

    NSString *rtfHdrEndFormat =
@"\\viewkind4\\uc1\\pard\\f0\\fs%.0f ";

    [self setString:rtfHdrStart];
    
    [self appendString:rtfHdrFontTableStart];
    NSInteger i = 0;
    for (NSFont *font in fonts) {
        [self appendFormat:rtfHdrFontTableFormat,
            i++,
            RTFFontFamily(font),
            (font.fontDescriptor.symbolicTraits & NSFontDescriptorTraitMonoSpace) ? 1 : 2,
            (font.fontDescriptor.symbolicTraits & NSFontDescriptorClassSymbolic) ? 2 : 0,
            font.fontName
        ];
    }
    [self appendString:rtfHdrFontTableEnd];
    
    [self appendString:rtfColorTableStart];
    for (NSColor *color in colors) {
        [self appendFormat:rtfColorTableFormat,
            color.redComponent * 255, color.greenComponent * 255, color.blueComponent * 255];
    }
    [self appendString:rtfColorTableEnd];
    
    [self appendFormat:rtfHdrEndFormat, [NSFont systemFontSize] * 2];
    
    [self setRtfTextColor:kColorNone];
}

- (void)RTFSetColor:(RTFTextColor)textColor {
    RTFTextColor currentColor = [self rtfTextColor];
    if (currentColor != textColor) {
        [self appendFormat:@"\\cf%d ", textColor];
        [self setRtfTextColor:textColor];
    }
}

- (void)RTFNewPara {
    [self appendString:@"\\par\r\n"];
}

- (void)RTFEnd {
    [self appendFormat:@"}\r\n%c", '\0'];
}

@end
