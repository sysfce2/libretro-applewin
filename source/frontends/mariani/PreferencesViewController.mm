//
//  PreferencesViewController.mm
//  Mariani
//
//  Created by Steven Huang on 12/30/21.
//

#import "PreferencesViewController.h"

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
@property (weak) IBOutlet NSTextField *videoCustomColorWell;
@property (weak) IBOutlet NSSlider *audioSpeakerVolumeSlider;
@property (weak) IBOutlet NSSlider *audioMockingboardVolumeSlider;

@property (weak) IBOutlet NSPopUpButton *storageDisk1Button;
@property (weak) IBOutlet NSPopUpButton *storageDisk2Button;
@property (weak) IBOutlet NSButton *storageEnhancedSpeedButton;

@end

@implementation PreferencesViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.preferredContentSize = self.view.frame.size;
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
