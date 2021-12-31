//
//  PreferencesWindowController.m
//  Mariani
//
//  Created by sh95014 on 12/30/21.
//

#import "PreferencesWindowController.h"

@interface PreferencesWindowController ()


@end

@implementation PreferencesWindowController

- (BOOL)windowShouldClose:(NSWindow *)sender {
    // just hide, don't close
    [self.window orderOut:sender];
    return NO;
}

@end
