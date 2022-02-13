//
//  NSMutableString+RTF.m
//  Mariani
//
//  Created by sh95014 on 2/12/22.
//

#import "NSMutableString+RTF.h"
#import <objc/runtime.h>

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

- (void)RTFBegin {
    NSString *rtfHdrStart =
@"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\deflangfe1033{\\fonttbl"
    "{\\f0\\fmodern\\fprq1\\fcharset0 Courier New;}"
    "{\\f1\\froman\\fprq2\\fcharset0 Times New Roman;}"
    "{\\f2\\fswiss\\fprq2\\fcharset0 Arial;}"
    "{\\f3\\froman\\fprq2\\fcharset2 Symbol;}"
    "}\r\n";

    NSString *rtfColorTable =
@"{\\colortbl;"
    "\\red0\\green0\\blue0;\\red0\\green0\\blue255;\\red0\\green255\\blue255;\\red0\\green255\\blue0;"
    "\\red255\\green0\\blue255;\\red255\\green0\\blue0;\\red255\\green255\\blue0;\\red255\\green255\\blue255;"
    "\\red0\\green0\\blue128;\\red0\\green128\\blue128;\\red0\\green128\\blue0;\r\n"
    "\\red128\\green0\\blue128;\\red128\\green0\\blue0;\\red128\\green128\\blue0;\\red128\\green128\\blue128;"
    "\\red192\\green192\\blue192;\\red64\\green64\\blue64;\\red255\\green153\\blue0;}\r\n";

    NSString *rtfHdrEnd =
@"\\viewkind4\\uc1\\pard\\f0\\fs25 ";

    [self setString:rtfHdrStart];
    [self appendString:rtfColorTable];
    [self appendString:rtfHdrEnd];
    
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
