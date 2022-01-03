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
- (void)reconfigureDrives;
- (int)showModalAlertofType:(int)type withMessage:(NSString *)message information:(NSString *)information;
- (void)updateDriveLights;

@end

#define theAppDelegate ((AppDelegate *)[[NSApplication sharedApplication] delegate])

#endif // __OBJC__

// for calling into AppDelegate from C++
int ShowModalAlertOfType(int type, const char *message, const char *information);
void UpdateDriveLights();
const void *ResourceNamed(const char *name, size_t expectedSize);
const char *PathToResourceNamed(const char *name);
