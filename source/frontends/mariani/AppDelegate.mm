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

// AppleWin imports
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
@property (weak) IBOutlet NSImageView *drive1LightView;
@property (weak) IBOutlet NSPopUpButton *drive1PopUpButton;
@property (weak) IBOutlet NSImageView *drive2LightView;
@property (weak) IBOutlet NSPopUpButton *drive2PopUpButton;
@property (weak) IBOutlet NSButton *volumeToggleButton;

@property NSTimer *timer;
@property Initialisation *initialisation;
@property LoggerContext *logger;
@property RegistryContext *registryContext;

@end

@implementation AppDelegate

std::shared_ptr<sa2::SDLFrame> frame;
FrameBuffer frameBuffer;

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

    Video & video = GetVideo();
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
        
        Video & video = GetVideo();
        frameBuffer.borderWidth = video.GetFrameBufferBorderWidth();
        frameBuffer.borderHeight = video.GetFrameBufferBorderHeight();
        frameBuffer.bufferWidth = video.GetFrameBufferWidth();
        frameBuffer.bufferHeight = video.GetFrameBufferHeight();
        frameBuffer.pixelWidth = video.GetFrameBufferBorderlessWidth();
        frameBuffer.pixelHeight = video.GetFrameBufferBorderlessHeight();
        [self.emulatorVC createScreen:&frameBuffer];
        
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
        
        self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / TARGET_FPS target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
    }
}

- (void)timerFired {
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
        Video & video = GetVideo();
        
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

- (IBAction)volumeToggled:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    if (self.volumeToggleButton.state == NSControlStateValueOn) {
        self.volumeToggleButton.state == NSControlStateValueOff;
    }
    else {
        self.volumeToggleButton.state == NSControlStateValueOn;
    }
}

#pragma mark - Utilties

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

@end
