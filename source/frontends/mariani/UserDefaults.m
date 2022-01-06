//
//  UserDefaults.m
//  Mariani
//
//  Created by sh95014 on 1/2/22.
//

#import "UserDefaults.h"

#define RECORDINGS_FOLDER_KEY   @"RecordingsFolder"
#define SCREENSHOTS_FOLDER_KEY  @"ScreenshotsFolder"

@implementation UserDefaults

+ (instancetype)sharedInstance {
    static dispatch_once_t once;
    static id sharedInstance;
    dispatch_once(&once, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (NSURL *)recordingsFolder {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSURL *folder = [defaults URLForKey:RECORDINGS_FOLDER_KEY];
    if (folder == nil) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, YES);
        folder = [NSURL fileURLWithPath:[paths objectAtIndex:0]];
    }
    return folder;
}

- (void)setRecordingsFolder:(NSURL *)recordingsFolder {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setURL:recordingsFolder forKey:RECORDINGS_FOLDER_KEY];
}

- (NSURL *)screenshotsFolder {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSURL *folder = [defaults URLForKey:SCREENSHOTS_FOLDER_KEY];
    if (folder == nil) {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, YES);
        folder = [NSURL fileURLWithPath:[paths objectAtIndex:0]];
    }
    return folder;
}

- (void)setScreenshotsFolder:(NSURL *)screenshotsFolder {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setURL:screenshotsFolder forKey:SCREENSHOTS_FOLDER_KEY];
}

@end
