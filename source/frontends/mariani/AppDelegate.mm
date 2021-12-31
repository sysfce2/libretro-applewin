//
//  AppDelegate.m
//  Mariani
//
//  Created by sh95014 on 12/27/21.
//

#import "AppDelegate.h"
#import "windows.h"

#import "context.h"

#import <SDL.h>
#import "fileregistry.h"
#import "gamepad.h"
#import "programoptions.h"
#import "sdirectsound.h"
#import "sdlrendererframe.h"
#import "utils.h"

// AppleWin
#import "Card.h"
#import "Interface.h"
#import "Utilities.h"
#import "Video.h"

#import "CommonTypes.h"
#import "EmulatorViewController.h"
#import "PreferencesWindowController.h"

#define TARGET_FPS          60

@interface AppDelegate ()

@property (strong) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSMenu *displayTypeMenu;
@property (weak) IBOutlet EmulatorViewController *emulatorVC;
@property (strong) PreferencesWindowController *preferencesWC;
@property (weak) IBOutlet NSButton *driveLightButtonTemplate;
@property (weak) IBOutlet NSButton *volumeToggleButton;

@property NSTimer *timer;
@property Initialisation *initialisation;
@property LoggerContext *logger;
@property RegistryContext *registryContext;
@property NSArray *driveLightButtons;
@property NSData *driveLightButtonTemplateArchive;

@end

@implementation AppDelegate

std::shared_ptr<sa2::SDLFrame> frame;
FrameBuffer frameBuffer;

Disk_Status_e driveStatus[NUM_SLOTS * NUM_DRIVES];

- (int)getRefreshRate {
    SDL_DisplayMode current;

    const int should_be_zero = SDL_GetCurrentDisplayMode(0, &current);
    if (should_be_zero) {
        throw std::runtime_error(SDL_GetError());
    }
    return current.refresh_rate;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    const Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (SDL_Init(flags) != 0) {
        NSLog(@"SDL_Init error %s", SDL_GetError());
    }
    
    common2::EmulatorOptions options;

    Video &video = GetVideo();
    const int sw = video.GetFrameBufferBorderlessWidth();
    const int sh = video.GetFrameBufferBorderlessHeight();

    options.geometry.empty = true;
    options.geometry.width = sw * 2;
    options.geometry.height = sh * 2;
    options.geometry.x = SDL_WINDOWPOS_UNDEFINED;
    options.geometry.y = SDL_WINDOWPOS_UNDEFINED;
    const bool run = getEmulatorOptions(0, NULL, "macOS", options);
    
    if (run) {
        self.logger = new LoggerContext(options.log);
        self.registryContext = new RegistryContext(CreateFileRegistry(options));

        frame.reset(new sa2::SDLRendererFrame(options));

        std::shared_ptr<Paddle> paddle(new sa2::Gamepad(0));
        self.initialisation = new Initialisation(frame, paddle);
        applyOptions(options);
        frame->Begin();
        
        NSLog(@"Default GL swap interval: %d", SDL_GL_GetSwapInterval());
        
        const int fps = [self getRefreshRate];
        NSLog(@"Video refresh rate: %d Hz, %f", fps, 1000.0 / fps);
        
        Video &video = GetVideo();
        frameBuffer.borderWidth = video.GetFrameBufferBorderWidth();
        frameBuffer.borderHeight = video.GetFrameBufferBorderHeight();
        frameBuffer.bufferWidth = video.GetFrameBufferWidth();
        frameBuffer.bufferHeight = video.GetFrameBufferHeight();
        frameBuffer.pixelWidth = video.GetFrameBufferBorderlessWidth();
        frameBuffer.pixelHeight = video.GetFrameBufferBorderlessHeight();
        [self.emulatorVC createScreen:&frameBuffer];
        
        self.window.delegate = self;
        
        // populate the Display Type menu with options
        const VideoType_e currentVideoType = video.GetVideoType();
        for (NSInteger videoType = VT_MONO_CUSTOM; videoType < NUM_VIDEO_MODES; videoType++) {
            NSString *itemTitle = [self localizedVideoType:videoType];
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:itemTitle
                                                          action:@selector(displayTypeAction:)
                                                   keyEquivalent:@""];
            item.tag = videoType;
            item.state = (currentVideoType == videoType) ? NSControlStateValueOn : NSControlStateValueOff;
            [self.displayTypeMenu addItem:item];
        }
        
        self.driveLightButtonTemplateArchive = [self archiveFromTemplateView:self.driveLightButtonTemplate];
        [self createDriveLightButtons];
        
        [self startEmulationTimer];
    }
}

- (void)timerFired {
    for (NSButton *driveLightButton in self.driveLightButtons) {
        CardManager & cardManager = GetCardMgr();
        UINT slot = (UINT)driveLightButton.tag / 10;
        NSInteger drive = driveLightButton.tag % 10;
        Disk2InterfaceCard * card2 = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));
        Disk_Status_e status[NUM_DRIVES];
        card2->GetLightStatus(&status[0], &status[1]);
        const BOOL active = (status[drive] == DISK_STATUS_READ || status[drive] == DISK_STATUS_WRITE);
        if (active) {
            driveLightButton.image = [NSImage imageWithSystemSymbolName:@"circle.fill" accessibilityDescription:@""];
            driveLightButton.contentTintColor = [NSColor controlAccentColor];
        }
        else {
            driveLightButton.image = [NSImage imageWithSystemSymbolName:@"circle" accessibilityDescription:@""];
            driveLightButton.contentTintColor = [NSColor secondaryLabelColor];
        }
    }
    
    if (self.volumeToggleButton.state == NSControlStateValueOn) {
        sa2::writeAudio();
    }
    
    bool quit = false;
    frame->ProcessEvents(quit);
    if (quit) {
        [NSApp terminate:self];
    }
    
    frame->ExecuteOneFrame(1000.0 / TARGET_FPS);
    
    frame->VideoPresentScreen();
    
    frameBuffer.data = frame->FrameBufferData();
    [self.emulatorVC updateScreen:&frameBuffer];
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    [self.timer invalidate];
    frame->End();
    SDL_Quit();
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)applicationDidHide:(NSNotification *)notification {
    [self.timer invalidate];
}

- (void)applicationWillUnhide:(NSNotification *)notification {
    [self startEmulationTimer];
}

#pragma mark - NSWindowDelegate

- (void)windowWillClose:(NSNotification *)notification {
    // needed despite applicationShouldTerminateAfterLastWindowClosed: because
    // the preferences window may be open
    if (notification.object == self.window) {
        [NSApp terminate:self];
    }
}

#pragma mark - App menu actions

- (IBAction)preferencesAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    NSStoryboard *storyboard = [NSStoryboard storyboardWithName:@"Preferences" bundle:nil];
    self.preferencesWC = [storyboard instantiateInitialController];
    [self.preferencesWC showWindow:sender];
}

#pragma mark - File menu actions

- (IBAction)controlResetAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    CtrlReset();
}

- (IBAction)rebootEmulatorAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    frame->Restart();
}

#pragma mark - View menu actions

- (IBAction)displayTypeAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    if ([sender isKindOfClass:[NSMenuItem class]]) {
        Video &video = GetVideo();
        
        // clear the selected state of the old item
        const VideoType_e currentVideoType = video.GetVideoType();
        NSMenuItem *oldItem = [self.displayTypeMenu itemWithTag:currentVideoType];
        oldItem.state = NSControlStateValueOff;

        // set the new item
        NSMenuItem *newItem = (NSMenuItem *)sender;
        newItem.state = NSControlStateValueOn;
        video.SetVideoType(VideoType_e(newItem.tag));
        frame->ApplyVideoModeChange();
        NSLog(@"Set video type to %ld", (long)newItem.tag);
    }
}

#pragma mark - Main window actions

- (IBAction)screenshotTaken:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

- (IBAction)driveLightAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
}

#pragma mark - Helpers because I can't figure out how to make 'frame' properly global

- (BOOL)emulationHardwareChanged {
    return frame->HardwareChanged();
}

- (void)applyVideoModeChange {
    frame->ApplyVideoModeChange();
}

#pragma mark - Utilities

- (NSString *)localizedVideoType:(NSInteger)videoType {
    static NSDictionary *videoTypeNames;
    static dispatch_once_t token;
    dispatch_once(&token, ^{
        videoTypeNames = @{
            @(VT_MONO_CUSTOM): NSLocalizedString(@"Monochrome (Custom)", @""),
            @(VT_COLOR_IDEALIZED): NSLocalizedString(@"Color (Composite Idealized)", @"Color rendering from AppleWin 1.25 (GH#357)"),
            @(VT_COLOR_VIDEOCARD_RGB): NSLocalizedString(@"Color (RGB Card/Monitor)", @"Real RGB card rendering"),
            @(VT_COLOR_MONITOR_NTSC): NSLocalizedString(@"Color (Composite Monitor)", @"NTSC or PAL"),
            @(VT_COLOR_TV): NSLocalizedString(@"Color TV", @""),
            @(VT_MONO_TV): NSLocalizedString(@"B&W TV", @""),
            @(VT_MONO_AMBER): NSLocalizedString(@"Monochrome (Amber)", @""),
            @(VT_MONO_GREEN): NSLocalizedString(@"Monochrome (Green)", @""),
            @(VT_MONO_WHITE): NSLocalizedString(@"Monochrome (White)", @""),
        };
    });
    
    NSString *name = [videoTypeNames objectForKey:[NSNumber numberWithInt:(int)videoType]];
    return (name != nil) ? name : NSLocalizedString(@"Unknown", @"");
}

- (NSView *)viewCopyFromTemplateView:(NSView *)templateView {
    return [self viewCopyFromArchive:[self archiveFromTemplateView:templateView]];
}

- (NSData *)archiveFromTemplateView:(NSView *)templateView {
    NSError *error;
    NSData *archive = [NSKeyedArchiver archivedDataWithRootObject:templateView requiringSecureCoding:NO error:&error];
    if (error != nil) {
        NSLog(@"%s: %@", __PRETTY_FUNCTION__, error.localizedDescription);
        return nil;
    }
    return archive;
}

- (NSView *)viewCopyFromArchive:(NSData *)templateArchive {
    NSError *error;
    NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:templateArchive error:&error];
    unarchiver.requiresSecureCoding = NO;
    NSView *view = [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];
    if (error != nil) {
        NSLog(@"%s: %@", __PRETTY_FUNCTION__, error.localizedDescription);
    }
    return view;
}

- (void)createDriveLightButtons {
    // only using as template, never actually show this button
    self.driveLightButtonTemplate.hidden = YES;
    
    // remove the old light buttons, if any
    for (NSView *button in self.driveLightButtons) {
        [button removeFromSuperview];
    }
    
    NSMutableArray *driveLightButtons = [NSMutableArray array];
    NSInteger position = 0;
    CardManager & cardManager = GetCardMgr();
    for (int slot = SLOT0; slot < NUM_SLOTS; slot++) {
        if (cardManager.QuerySlot(slot) == CT_Disk2) {
            for (int drive = DRIVE_1; drive < NUM_DRIVES; drive++) {
                NSButton *driveLightButton = (NSButton *)[self viewCopyFromArchive:self.driveLightButtonTemplateArchive];

                [driveLightButtons addObject:driveLightButton];
                [[self.driveLightButtonTemplate superview] addSubview:driveLightButton];
                
                // offset each drive light button from the left
                CGRect driveLightButtonFrame = driveLightButton.frame;
                driveLightButtonFrame.origin.x = self.driveLightButtonTemplate.frame.origin.x + position * self.driveLightButtonTemplate.frame.size.width;
                driveLightButton.frame = driveLightButtonFrame;
                
                driveLightButton.tag = slot * 10 + drive;
                driveLightButton.hidden = NO;
                
                position++;
            }
        }
    }
    
    self.driveLightButtons = driveLightButtons;
}

- (void)startEmulationTimer {
    self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / TARGET_FPS target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
}

@end
