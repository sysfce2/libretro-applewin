//
//  PreferencesViewController.mm
//  Mariani
//
//  Created by sh95014 on 12/30/21.
//

// This controller handles all the preferences panes, but there's one instance
// created per pane. When necessary they use the "vcId" user-defined run-time
// attribute to disambiguate.

#import "PreferencesViewController.h"
#import "AppDelegate.h"
#import "UserDefaults.h"

// AppleWin
#include "StdAfx.h"
#import <string>

#import "wincompat.h"
#import "winhandles.h"
#import "Card.h"
#import "CardManager.h"
#import "Common.h"
#import "Core.h"
#import "Interface.h"
#import "Memory.h"
void CreateLanguageCard(void); // should be in Memory.h
#import "Mockingboard.h"
#import "Speaker.h"
#import "Tfe.h"
#import "tfesupp.h"

#import "linux/registry.h"

// these need to match the values set in Preferences.storyboard for key "vcId"
#define GENERAL_PANE_ID         @"general"
#define COMPUTER_PANE_ID        @"computer"
#define AUDIO_VIDEO_PANE_ID     @"audioVideo"
#define STORAGE_PANE_ID         @"storage"

@interface PreferencesViewController ()

@property (weak) IBOutlet NSButton *generalDeveloperToolsButton;
@property (weak) IBOutlet NSButton *generalScreenshotsFolderButton;

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

@property (weak) IBOutlet NSButton *storageEnhancedSpeedButton;
@property (weak) IBOutlet NSButton *storageHardDiskFolderButton;

@property NSMutableDictionary *keyValueStore;
@property BOOL configured;

@end

@implementation PreferencesViewController

BOOL configured;

- (void)awakeFromNib {
    // just have the first PreferencesViewController that is awakened configure
    // the views for everybody
    if (!self.configured) {
        NSString *vcId = [self valueForKey:@"vcId"];
        if ([vcId isEqualToString:GENERAL_PANE_ID]) {
            [self configureGeneral];
        }
        if ([vcId isEqualToString:COMPUTER_PANE_ID]) {
            [self configureComputer];
        }
        else if ([vcId isEqualToString:AUDIO_VIDEO_PANE_ID]) {
            [self configureAudio];
            [self configureVideo];
        }
        else if ([vcId isEqualToString:STORAGE_PANE_ID]) {
            [self configureStorage];
        }
        self.configured = YES;
    }
}

- (void)setValue:(id)value forKey:(NSString *)key {
    if (self.keyValueStore == nil) {
        self.keyValueStore = [NSMutableDictionary dictionary];
    }
    [self.keyValueStore setValue:value forKey:key];
}

- (id)valueForKey:(NSString *)key {
    return [self.keyValueStore valueForKey:key];
}

- (void)viewDidLoad {
    [super viewDidLoad];

    // causes the preferences window to resize depending on the preferred size
    // of each pane
    self.preferredContentSize = self.view.frame.size;
}

- (void)viewDidAppear {
    [super viewDidAppear];
    
    self.view.window.title = self.title;
}

#pragma mark - Configuration

- (void)configureGeneral {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    self.generalDeveloperToolsButton.state = NSControlStateValueOff;
    self.generalDeveloperToolsButton.enabled = NO;

    NSURL *folder = [[UserDefaults sharedInstance] screenshotsFolder];
    self.generalScreenshotsFolderButton.title = [NSString stringWithUTF8String:[folder fileSystemRepresentation]];
}

// types of main boards, ordered as we want them to appear in UI
const eApple2Type computerTypes[] = {
    A2TYPE_APPLE2, A2TYPE_APPLE2PLUS, A2TYPE_APPLE2JPLUS, A2TYPE_APPLE2E,
    A2TYPE_APPLE2EENHANCED, A2TYPE_PRAVETS82, A2TYPE_PRAVETS8M,
    A2TYPE_PRAVETS8A, A2TYPE_TK30002E, A2TYPE_BASE64A
};

// CT_Empty is being used as the terminator here. the UI will insert an empty
// option on its own
const SS_CARDTYPE slot0Types[] = { CT_LanguageCard, CT_Saturn128K, CT_Empty };
const SS_CARDTYPE slot1Types[] = { CT_GenericPrinter, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE slot2Types[] = { CT_SSC, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE slot3Types[] = { CT_Uthernet, CT_Uthernet2, CT_VidHD, CT_Empty };
const SS_CARDTYPE slot4Types[] = { CT_MockingboardC, CT_MouseInterface, CT_Phasor, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE slot5Types[] = { CT_MockingboardC, CT_Z80, CT_SAM, CT_Disk2, CT_FourPlay, CT_SNESMAX, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE slot6Types[] = { CT_Disk2, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE slot7Types[] = { CT_GenericHDD, CT_Uthernet2, CT_Empty };
const SS_CARDTYPE *slotTypes[] = {
    slot0Types, slot1Types, slot2Types, slot3Types, slot4Types, slot5Types,
    slot6Types, slot7Types,
};

const SS_CARDTYPE expansionSlotTypes[] = { CT_LanguageCard, CT_Extended80Col, CT_Saturn128K, CT_RamWorksIII, CT_Empty };

- (void)configureComputer {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    // emulation computer types
    NSString *vcId = [self valueForKey:@"vcId"];
    if ([vcId isEqualToString:COMPUTER_PANE_ID]) {
        const eApple2Type computerType = GetApple2Type();

        NSDictionary *map = [self localizedComputerNameMap];
        for (NSInteger i = 0; i < sizeof(computerTypes) / sizeof (*computerTypes); i++) {
            [self.computerMainBoardButton addItemWithTitle:[map objectForKey:@(computerTypes[i])]];
            if (computerType == computerTypes[i]) {
                [self.computerMainBoardButton selectItemAtIndex:i];
            }
        }
    
        // peripheral slots
        CardManager &manager = GetCardMgr();
        NSDictionary *cardNames = [self localizedCardNameMap];
        for (int slot = SLOT1; slot < NUM_SLOTS; slot++) {
            NSPopUpButton *slotButton = [[self slotButtonsArray] objectAtIndex:slot];
            
            // already set up that way in Preferences.storyboard but let's be
            // explicit because slotAction: relies on it.
            slotButton.tag = slot;
            
            // always start with "Empty" as an option
            [slotButton addItemWithTitle:[cardNames objectForKey:@(CT_Empty)]];
            slotButton.lastItem.tag = CT_Empty;
            
            for (int i = 0; slotTypes[slot][i] != CT_Empty; i++) {
                [slotButton addItemWithTitle:[cardNames objectForKey:@(slotTypes[slot][i])]];
                slotButton.lastItem.tag = slotTypes[slot][i];
            }
            
            // show the current item as selected
            [slotButton selectItemWithTag:manager.QuerySlot(slot)];
        }
        
        // expansion slot
        [self.computerExansionSlotButton addItemWithTitle:[cardNames objectForKey:@(CT_Empty)]];
        self.computerExansionSlotButton.lastItem.tag = CT_Empty;
        for (int i = 0; expansionSlotTypes[i] != CT_Empty; i++) {
            [self.computerExansionSlotButton addItemWithTitle:[cardNames objectForKey:@(expansionSlotTypes[i])]];
            self.computerExansionSlotButton.lastItem.tag = expansionSlotTypes[i];
        }
        [self.computerExansionSlotButton selectItemWithTag:GetCurrentExpansionMemType()];
        
        // pcap
        if (tfe_enumadapter_open()) {
            char *name;
            char *description;
            
            while (tfe_enumadapter(&name, &description)) {
                [self.computerPcapSlotButton addItemWithTitle:[NSString stringWithUTF8String:name]];
                lib_free(name);
                lib_free(description);
            }
            tfe_enumadapter_close();
            
            const std::string currentInterface = get_tfe_interface();
            [self.computerPcapSlotButton selectItemWithTitle:[NSString stringWithUTF8String:currentInterface.c_str()]];
        }
    }
}

- (void)configureAudio {
    // speaker volume slider. for some reason lower numbers are quieter so we
    // need to get the complements
    NSString *vcId = [self valueForKey:@"vcId"];
    if ([vcId isEqualToString:AUDIO_VIDEO_PANE_ID]) {
        const int volumeMax = GetPropertySheet().GetVolumeMax();
        self.audioSpeakerVolumeSlider.maxValue = volumeMax;
        self.audioSpeakerVolumeSlider.intValue = volumeMax - SpkrGetVolume();
        
        // Mockingboard volume slider
        self.audioMockingboardVolumeSlider.maxValue = volumeMax;
        self.audioMockingboardVolumeSlider.intValue = volumeMax - MB_GetVolume();
    }
}

- (void)configureVideo {
    NSString *vcId = [self valueForKey:@"vcId"];
    if ([vcId isEqualToString:AUDIO_VIDEO_PANE_ID]) {
        Video &video = GetVideo();

        // Custom Monochrome color
        self.videoCustomColorWell.color = [self colorWithColorRef:video.GetMonochromeRGB()];
        
        // 50% Scan Lines
        self.video50PercentScanLinesButton.state = video.IsVideoStyle(VS_HALF_SCANLINES) ? NSControlStateValueOn : NSControlStateValueOff;
    }
}

- (void)configureStorage {
    CardManager &cardManager = GetCardMgr();

    // Enhanced speed for floppy drives
    self.storageEnhancedSpeedButton.state = cardManager.GetDisk2CardMgr().GetEnhanceDisk() ? NSControlStateValueOn : NSControlStateValueOff;

    // Hard disk
    self.storageHardDiskFolderButton.enabled = NO;
}

#pragma mark - Actions

- (IBAction)toggleDeveloperTools:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)screenshotsFolderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canDownloadUbiquitousContents = YES;
    
    if ([panel runModal] == NSModalResponseOK) {
        self.generalScreenshotsFolderButton.title = [NSString stringWithUTF8String:[panel.URL fileSystemRepresentation]];
        [[UserDefaults sharedInstance] setScreenshotsFolder:panel.URL];
    }
}

- (IBAction)mainBoardAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    NSInteger index = [self.computerMainBoardButton indexOfSelectedItem];
    SetApple2Type(computerTypes[index]);
    RegSaveValue(REG_CONFIG, REGVALUE_APPLE2_TYPE, true, computerTypes[index]);
    NSLog(@"Set main board type to '%@' (%d)",
          [[self localizedComputerNameMap] objectForKey:@(computerTypes[index])],
          computerTypes[index]);
    
    self.computerRebootEmulatorButton.enabled = [theAppDelegate emulationHardwareChanged];
}

- (IBAction)slotAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
    
    if ([sender isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton *slotButton = (NSPopUpButton *)sender;
        const NSInteger currentSlot = slotButton.tag;
        
        CardManager &cardManager = GetCardMgr();
        Video &video = GetVideo();
        
        // special teardown if a VidHD card was removed
        const BOOL oldHasVidHD = video.HasVidHD();
        const BOOL newHasVidHD = (slotButton.selectedTag == CT_VidHD);
        if (oldHasVidHD == YES && newHasVidHD == NO) {
            video.SetVidHD(false);
        }
        
        NSArray *slotButtons = [self slotButtonsArray];
        
        // Mockingboard takes both slots, so inserting in the other available
        // slot than was specified and remove both if removing either
        if (slotButton.selectedTag == CT_MockingboardC) {
            // find the other slot and set it to MockingBoard
            for (NSInteger slot = SLOT1; slot < NUM_SLOTS; slot++) {
                for (int i = 0; slotTypes[slot][i] != CT_Empty; i++) {
                    if (slotTypes[slot][i] == CT_MockingboardC && slot != currentSlot) {
                        cardManager.Insert((SLOTS)slot, (SS_CARDTYPE)slotButton.selectedTag);
                        [slotButtons[slot] selectItemWithTag:slotButton.selectedTag];
                    }
                }
            }
        }
        else if (cardManager.QuerySlot((SLOTS)currentSlot) == CT_MockingboardC) {
            // find the other slot and set it to empty
            for (NSInteger slot = SLOT1; slot < NUM_SLOTS; slot++) {
                for (int i = 0; slotTypes[slot][i] != CT_Empty; i++) {
                    if (slotTypes[slot][i] == CT_MockingboardC && slot != currentSlot) {
                        cardManager.Insert((SLOTS)slot, CT_Empty);
                        [slotButtons[slot] selectItemWithTag:CT_Empty];
                    }
                }
            }
        }
        
        cardManager.Insert((SLOTS)currentSlot, (SS_CARDTYPE)slotButton.selectedTag);
        
        MemInitializeIO();
        
        [theAppDelegate reconfigureDrives];
        if (oldHasVidHD != video.HasVidHD()) {
            [theAppDelegate restartFrame];
        }

        self.computerRebootEmulatorButton.enabled = [theAppDelegate emulationHardwareChanged];
    }
}

- (IBAction)expansionSlotAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    SetExpansionMemType((SS_CARDTYPE)self.computerExansionSlotButton.selectedTag);
    CreateLanguageCard();
    MemInitializeIO();
    
    self.computerRebootEmulatorButton.enabled = [theAppDelegate emulationHardwareChanged];
}

- (IBAction)pcapSlotAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    if ([sender isKindOfClass:[NSPopUpButton class]]) {
        NSPopUpButton *slotButton = (NSPopUpButton *)sender;
        const std::string newInterface([slotButton.selectedItem.title cStringUsingEncoding:NSUTF8StringEncoding]);

        update_tfe_interface(newInterface);
        tfe_SetRegistryInterface(SLOT3, newInterface);
    }
    
    self.computerRebootEmulatorButton.enabled = [theAppDelegate emulationHardwareChanged];
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
        NSLog(@"Enable 50%% scan lines");
    }
    else {
        [self setVideoStyle:VS_HALF_SCANLINES enabled:NO];
        NSLog(@"Disable 50%% scan lines");
    }
    [theAppDelegate applyVideoModeChange];
}

- (IBAction)customColorWellAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    Video &video = GetVideo();
    const int r = self.videoCustomColorWell.color.redComponent * 0xFF;
    const int g = self.videoCustomColorWell.color.greenComponent * 0xFF;
    const int b = self.videoCustomColorWell.color.blueComponent * 0xFF;
    video.SetMonochromeRGB(RGB(r, g, b));
    [theAppDelegate applyVideoModeChange];
    NSLog(@"Set custom monochrome color to #%02X%02X%02X", r, g, b);
}

- (IBAction)speakerVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    const int volumeMax = GetPropertySheet().GetVolumeMax();
    const int volume = volumeMax - self.audioSpeakerVolumeSlider.intValue;
    SpkrSetVolume(volume, volumeMax);
    RegSaveValue(REG_CONFIG, REGVALUE_SPKR_VOLUME, true, volume);
    NSLog(@"Set speaker volume to %d", volume);
}

- (IBAction)mockingboardVolumeSliderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    const int volumeMax = GetPropertySheet().GetVolumeMax();
    const int volume = volumeMax - self.audioMockingboardVolumeSlider.intValue;
    MB_SetVolume(volume, volumeMax);
    RegSaveValue(REG_CONFIG, REGVALUE_MB_VOLUME, true, volume);
    NSLog(@"Set Mockingboard volume to %d", volume);
}

- (IBAction)diskAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)diskEjectAction:(id)sender {
    NSLog(@"%s (%ld)", __PRETTY_FUNCTION__, (long)[(NSView *)sender tag]);
}

- (IBAction)toggleDiskEnhancedSpeed:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    CardManager &cardManager = GetCardMgr();
    BOOL enhancedDisk = (self.storageEnhancedSpeedButton.state == NSControlStateValueOn);
    cardManager.GetDisk2CardMgr().SetEnhanceDisk(enhancedDisk);
    RegSaveValue(REG_CONFIG, REGVALUE_ENHANCE_DISK_SPEED, true, enhancedDisk);
}

- (IBAction)hardDiskFolderAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

#pragma mark - Utilities

- (NSDictionary *)localizedComputerNameMap {
    // helps map eApple2Type to a readable string
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

- (NSArray *)slotButtonsArray {
    // so that we can index computerSlotButtons by SLOT1..SLOT7
    return @[
        [NSNull null],              // SLOT0
        self.computerSlot1Button,
        self.computerSlot2Button,
        self.computerSlot3Button,
        self.computerSlot4Button,
        self.computerSlot5Button,
        self.computerSlot6Button,
        self.computerSlot7Button,
    ];
}

- (NSDictionary *)localizedCardNameMap {
    // helps map SS_CARDTYPE to a readable string
    return @{
        @(CT_Empty):                NSLocalizedString(@"—", @"empty slot"),
        @(CT_Disk2):                NSLocalizedString(@"Apple Disk ][", @""),
        @(CT_SSC):                  NSLocalizedString(@"Apple Super Serial Card", @""),
        @(CT_MockingboardC):        NSLocalizedString(@"Mockingboard C (sound)", @""),
        @(CT_GenericPrinter):       NSLocalizedString(@"Generic Printer", @""),
        @(CT_GenericHDD):           NSLocalizedString(@"Generic Hard Disk Drive", @""),
        @(CT_GenericClock):         NSLocalizedString(@"Generic Clock", @""),
        @(CT_MouseInterface):       NSLocalizedString(@"Mouse Interface", @""),
        @(CT_Z80):                  NSLocalizedString(@"Z-80", @""),
        @(CT_Phasor):               NSLocalizedString(@"Phasor (sound)", @""),
        @(CT_Echo):                 NSLocalizedString(@"Echo (speech)", @""),
        @(CT_SAM):                  NSLocalizedString(@"Software Automatic Mouth (speech)", @""),
        @(CT_80Col):                NSLocalizedString(@"80-column text card (1K)", @""),
        @(CT_Extended80Col):        NSLocalizedString(@"Extended 80-column text card (64K)", @""),
        @(CT_RamWorksIII):          NSLocalizedString(@"RamWorks III (up to 8MB)", @""),
        @(CT_Uthernet):             NSLocalizedString(@"Uthernet I (network)", @""),
        @(CT_LanguageCard):         NSLocalizedString(@"Apple Language Card", @""),
        @(CT_LanguageCardIIe):      NSLocalizedString(@"Apple Language Card //e", @""),
        @(CT_Saturn128K):           NSLocalizedString(@"Saturn 128K (memory)", @""),
        @(CT_FourPlay):             NSLocalizedString(@"4play (joystick)", @""),
        @(CT_SNESMAX):              NSLocalizedString(@"SNES MAX (game controller)", @""),
        @(CT_VidHD):                NSLocalizedString(@"VidHD (video)", @""),
        @(CT_Uthernet2):            NSLocalizedString(@"Uthernet II (network)", @""),
    };
}

- (NSColor *)colorWithColorRef:(DWORD)colorRef {
    // reverse of RGB()
    return [NSColor colorWithRed:(colorRef & 0xFF) / 255.0
                           green:((colorRef >> 8) & 0xFF) / 255.0
                            blue:((colorRef >> 16) & 0xFF) / 255.0
                           alpha:1];
}

- (void)setVideoStyle:(VideoStyle_e)style enabled:(BOOL)enabled {
    Video &video = GetVideo();
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
