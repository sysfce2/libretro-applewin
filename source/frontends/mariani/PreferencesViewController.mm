//
//  PreferencesViewController.mm
//  Mariani
//
//  Created by Steven Huang on 12/30/21.
//

// This controller handles all the preferences panes, but there's one instance
// created per pane. When necessary they use the "vcId" user-defined run-time
// attribute to disambiguate.

#import "PreferencesViewController.h"

// AppleWin
#include "StdAfx.h"
#import <string>

#import "wincompat.h"
#import "winhandles.h"
#import "Common.h"
#import "Core.h"

#import "linux/registry.h"

@interface PreferencesViewController ()

@property (weak) IBOutlet NSButton *generalDeveloperToolsButton;
@property (weak) IBOutlet NSPopUpButton *generalScreenshotsFolderButton;

@property (weak) IBOutlet NSPopUpButton *computerMainBoardButton;
@property (weak) IBOutlet NSPopUpButton *computerSlot1Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot2Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot3Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot4Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot5Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot6Button;
@property (weak) IBOutlet NSPopUpButton *computerSlot7Button;
@property (weak) IBOutlet NSPopUpButton *computerExansionSlotButton;
@property (weak) IBOutlet NSPopUpButton *computerPcapSlotButton;
@property (weak) IBOutlet NSButton *computerRebootEmulatorButton;

@property (weak) IBOutlet NSButton *video50PercentScanLinesButton;
@property (weak) IBOutlet NSColorWell *videoCustomColorWell;
@property (weak) IBOutlet NSSlider *audioSpeakerVolumeSlider;
@property (weak) IBOutlet NSSlider *audioMockingboardVolumeSlider;

@property (weak) IBOutlet NSPopUpButton *storageDisk1Button;
@property (weak) IBOutlet NSPopUpButton *storageDisk2Button;
@property (weak) IBOutlet NSButton *storageEnhancedSpeedButton;

@property NSMutableDictionary *keyValueStore;

@end

@implementation PreferencesViewController

- (void)setValue:(id)value forKey:(NSString *)key {
    if (self.keyValueStore == nil) {
        self.keyValueStore = [NSMutableDictionary dictionary];
    }
    [self.keyValueStore setValue:value forKey:key];
}

- (id)valueForKey:(NSString *)key {
    return [self.keyValueStore valueForKey:key];
}

- (NSDictionary *)localizedComputerNameMap {
    return @{
        @(A2TYPE_APPLE2):           NSLocalizedString(@"Apple ][ Emulator", @""),
        @(A2TYPE_APPLE2PLUS):       NSLocalizedString(@"Apple ][+ Emulator", @""),
        @(A2TYPE_APPLE2JPLUS):      NSLocalizedString(@"Apple ][ J-Plus Emulator", @""),
        @(A2TYPE_APPLE2E):          NSLocalizedString(@"Apple //e Emulator", @""),
        @(A2TYPE_APPLE2EENHANCED):  NSLocalizedString(@"Enhanced Apple //e Emulator", @""),
        @(A2TYPE_APPLE2C):          NSLocalizedString(@"Apple //c Emulator", @""),
        @(A2TYPE_PRAVETS82):        NSLocalizedString(@"Pravets 82 Emulator", @""),
        @(A2TYPE_PRAVETS8M):        NSLocalizedString(@"Pravets 8M Emulator", @""),
        @(A2TYPE_PRAVETS8A):        NSLocalizedString(@"Pravets 8A Emulator", @""),
        @(A2TYPE_TK30002E):         NSLocalizedString(@"TK3000 //e Emulator", @""),
        @(A2TYPE_BASE64A):          NSLocalizedString(@"Base64A Emulator", @""),
    };
}

const eApple2Type computerTypes[] = {
    A2TYPE_APPLE2, A2TYPE_APPLE2PLUS, A2TYPE_APPLE2JPLUS, A2TYPE_APPLE2E,
    A2TYPE_APPLE2EENHANCED, A2TYPE_PRAVETS82, A2TYPE_PRAVETS8M,
    A2TYPE_PRAVETS8A, A2TYPE_TK30002E, A2TYPE_BASE64A
};

- (void)viewDidLoad {
    [super viewDidLoad];

    self.preferredContentSize = self.view.frame.size;
    
    NSString *vcId = [self valueForKey:@"vcId"];
    if ([vcId isEqualToString:@"computer"]) {
        const eApple2Type computerType = GetApple2Type();

        NSDictionary *map = [self localizedComputerNameMap];
        for (NSInteger i = 0; i < sizeof(computerTypes) / sizeof (*computerTypes); i++) {
            [self.computerMainBoardButton addItemWithTitle:[map objectForKey:@(computerTypes[i])]];
            if (computerType == computerTypes[i]) {
                [self.computerMainBoardButton selectItemAtIndex:i];
            }
        }
    }
}

- (void)viewDidAppear {
    [super viewDidAppear];
    
    self.view.window.title = self.title;
}

- (IBAction)toggleDeveloperTools:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)screenshotFolderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)mainBoardAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    NSInteger index = [self.computerMainBoardButton indexOfSelectedItem];
    SetApple2Type(computerTypes[index]);
    RegSaveValue(REG_CONFIG, REGVALUE_APPLE2_TYPE, true, computerTypes[index]);
    
//    self.computerRebootEmulatorButton.enabled = frame->HardwareChanged();
}

- (IBAction)slotAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)rebootEmulatorAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)toggle50PercentScanLines:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)customColorWellAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)speakerVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)mockingboardVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)diskAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)diskEjectAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)toggleDiskEnhancedSpeed:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

@end
