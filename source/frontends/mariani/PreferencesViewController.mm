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
#import "AppDelegate.h"

// AppleWin
#include "StdAfx.h"
#import <string>

#import "wincompat.h"
#import "winhandles.h"
#import "Common.h"
#import "Core.h"
#import "Interface.h"
#import "Mockingboard.h"
#import "Speaker.h"

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

    // causes the preferences window to resize depending on the preferred size
    // of each pane
    self.preferredContentSize = self.view.frame.size;
    
    [self configureGeneral];
    [self configureAudio];
    [self configureVideo];
}

- (void)viewDidAppear {
    [super viewDidAppear];
    
    self.view.window.title = self.title;
}

#pragma mark - Configuration

- (void)configureGeneral {
    // emulation computer types
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

- (void)configureAudio {
    // speaker volume slider. for some reason lower numbers are quieter so we
    // need to get the complements
    const int volumeMax = GetPropertySheet().GetVolumeMax();
    self.audioSpeakerVolumeSlider.maxValue = volumeMax;
    self.audioSpeakerVolumeSlider.intValue = volumeMax - SpkrGetVolume();
    
    // Mockingboard volume slider
    self.audioMockingboardVolumeSlider.maxValue = volumeMax;
    self.audioMockingboardVolumeSlider.intValue = volumeMax - MB_GetVolume();
}

- (void)configureVideo {
    Video & video = GetVideo();

    // Custom Monochrome color
    self.videoCustomColorWell.color = [self colorWithColorRef:video.GetMonochromeRGB()];
    
    // 50% Scan Lines
    self.video50PercentScanLinesButton.state = video.IsVideoStyle(VS_HALF_SCANLINES) ? NSControlStateValueOn : NSControlStateValueOff;
}

#pragma mark - Actions

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
    
    self.computerRebootEmulatorButton.enabled = [theAppDelegate emulationHardwareChanged];
}

- (IBAction)slotAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)rebootEmulatorAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    self.computerRebootEmulatorButton.enabled = NO;
    [theAppDelegate rebootEmulatorAction:sender];
}

- (IBAction)toggle50PercentScanLines:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    if (self.video50PercentScanLinesButton.state == NSControlStateValueOn) {
        [self setVideoStyle:VS_HALF_SCANLINES enabled:YES];
    }
    else {
        [self setVideoStyle:VS_HALF_SCANLINES enabled:NO];
    }
    [theAppDelegate applyVideoModeChange];
}

- (IBAction)customColorWellAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    Video & video = GetVideo();
    const int r = self.videoCustomColorWell.color.redComponent * 0xFF;
    const int g = self.videoCustomColorWell.color.greenComponent * 0xFF;
    const int b = self.videoCustomColorWell.color.blueComponent * 0xFF;
    video.SetMonochromeRGB(RGB(r, g, b));
    [theAppDelegate applyVideoModeChange];
}

- (IBAction)speakerVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    const int volumeMax = GetPropertySheet().GetVolumeMax();
    const int volume = volumeMax - self.audioSpeakerVolumeSlider.intValue;
    SpkrSetVolume(volume, volumeMax);
    RegSaveValue(REG_CONFIG, REGVALUE_SPKR_VOLUME, true, volume);
}

- (IBAction)mockingboardVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    const int volumeMax = GetPropertySheet().GetVolumeMax();
    const int volume = volumeMax - self.audioMockingboardVolumeSlider.intValue;
    MB_SetVolume(volume, volumeMax);
    RegSaveValue(REG_CONFIG, REGVALUE_MB_VOLUME, true, volume);
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

#pragma mark - Utilities

- (NSColor *)colorWithColorRef:(DWORD)colorRef {
    return [NSColor colorWithRed:(colorRef & 0xFF) / 255.0
                           green:((colorRef >> 8) & 0xFF) / 255.0
                            blue:((colorRef >> 16) & 0xFF) / 255.0
                           alpha:1];
}

- (void)setVideoStyle:(VideoStyle_e)style enabled:(BOOL)enabled {
    Video & video = GetVideo();
    VideoStyle_e currentVideoStyle = video.GetVideoStyle();
    if (enabled) {
      currentVideoStyle = VideoStyle_e(currentVideoStyle | style);
    }
    else {
      currentVideoStyle = VideoStyle_e(currentVideoStyle & (~style));
    }
    video.SetVideoStyle(currentVideoStyle);
}

@end
