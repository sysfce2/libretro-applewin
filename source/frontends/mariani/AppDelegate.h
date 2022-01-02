//
//  AppDelegate.h
//  Mariani
//
//  Created by sh95014 on 12/27/21.
//

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSOpenSavePanelDelegate>

- (void)applyVideoModeChange;
- (BOOL)emulationHardwareChanged;
- (IBAction)rebootEmulatorAction:(id)sender;
- (void)restartFrame;
- (void)updateDrives;
- (int)showModalAlertofType:(int)type withMessage:(const char *)message information:(const char *)information;

@end

#define theAppDelegate ((AppDelegate *)[[NSApplication sharedApplication] delegate])

#endif // __OBJC__

// for calling into AppDelegate from C++
int ShowModalAlertOfType(int type, const char *message, const char *information);
