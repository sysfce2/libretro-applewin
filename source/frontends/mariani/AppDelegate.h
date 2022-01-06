//
//  AppDelegate.h
//  Mariani
//
//  Created by sh95014 on 12/27/21.
//

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>
#import "EmulatorViewController.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSOpenSavePanelDelegate, EmulatorViewControllerDelegate>

- (void)applyVideoModeChange;
- (BOOL)emulationHardwareChanged;
- (IBAction)rebootEmulatorAction:(id)sender;
- (void)reinitializeFrame;
- (void)reconfigureDrives;
- (int)showModalAlertofType:(int)type withMessage:(NSString *)message information:(NSString *)information;
- (void)updateDriveLights;
- (void)terminateWithReason:(NSString *)reason;

@end

#define theAppDelegate ((AppDelegate *)[[NSApplication sharedApplication] delegate])

#endif // __OBJC__

// for calling into AppDelegate from C++
int ShowModalAlertOfType(int type, const char *message, const char *information);
void UpdateDriveLights(void);
const char *PathToResourceNamed(const char *name);
const char *GetSupportDirectory(void);
