//
//  PreferencesWindowController.m
//  Mariani
//
//  Created by Steven Huang on 12/30/21.
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
