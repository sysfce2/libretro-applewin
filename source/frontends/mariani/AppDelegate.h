//
//  AppDelegate.h
//  Mariani
//
//  Created by sh95014 on 12/27/21.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>

- (BOOL)emulationHardwareChanged;
- (IBAction)rebootEmulatorAction:(id)sender;
- (void)applyVideoModeChange;

@end

#define theAppDelegate ((AppDelegate *)[[NSApplication sharedApplication] delegate])
