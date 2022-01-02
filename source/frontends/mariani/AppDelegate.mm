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
#import "sdldummyframe.hpp"
#import "utils.h"

// AppleWin
#import "Card.h"
#import "Interface.h"
#import "Utilities.h"
#import "Video.h"

#import "CommonTypes.h"
#import "EmulatorViewController.h"
#import "PreferencesWindowController.h"
#import "UserDefaults.h"

#define TARGET_FPS          60
#define STATUS_BAR_HEIGHT   32

#define SCREENSHOT_FILE_NAME @"Mariani Screen Shot"

@interface AppDelegate ()

@property (strong) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSMenu *displayTypeMenu;
@property (weak) IBOutlet EmulatorViewController *emulatorVC;
@property (strong) PreferencesWindowController *preferencesWC;
@property (weak) IBOutlet NSButton *driveLightButtonTemplate;
@property (weak) IBOutlet NSButton *volumeToggleButton;
@property (weak) IBOutlet NSMenuItem *showHideStatusBarMenuItem;

@property NSTimer *timer;
@property Initialisation *initialisation;
@property LoggerContext *logger;
@property RegistryContext *registryContext;
@property NSArray *driveLightButtons;
@property NSData *driveLightButtonTemplateArchive;
@property BOOL hasStatusBar;
@property (readonly) double statusBarHeight;
@property (weak) IBOutlet NSView *statusBarView;

@end

@interface NSAlert (Synchronous)

- (NSModalResponse)runModalSheetForWindow:(NSWindow *)aWindow;
- (NSModalResponse)runModalSheet;

@end // NSAlert (Synchronous)

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
    _hasStatusBar = YES;
    
    const Uint32 flags = SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO;
    if (SDL_Init(flags) != 0) {
        NSLog(@"SDL_Init error %s", SDL_GetError());
    }
    
    common2::EmulatorOptions options;
    self.logger = new LoggerContext(options.log);
    self.registryContext = new RegistryContext(CreateFileRegistry(options));

    frame.reset(new sa2::SDLDummyFrame(options));

    std::shared_ptr<Paddle> paddle(new sa2::Gamepad(0));
    self.initialisation = new Initialisation(frame, paddle);
    applyOptions(options);
    frame->Begin();
    
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

- (void)timerFired {
#ifdef DEBUG
    NSDate *start = [NSDate now];
#endif
    
    if (self.volumeToggleButton.state == NSControlStateValueOn) {
        sa2::writeAudio();
    }
#ifdef DEBUG
    NSTimeInterval audioWriteTimeOffset = -[start timeIntervalSinceNow];
#endif

    bool quit = false;
    frame->ProcessEvents(quit);
    if (quit) {
        [NSApp terminate:self];
    }
#ifdef DEBUG
    NSTimeInterval eventProcessingTimeOffset = -[start timeIntervalSinceNow];
#endif

    frame->ExecuteOneFrame(1000.0 / TARGET_FPS);
#ifdef DEBUG
    NSTimeInterval executionTimeOffset = -[start timeIntervalSinceNow];
#endif

    frame->VideoPresentScreen();

    frameBuffer.data = frame->FrameBufferData();
    [self.emulatorVC updateScreen:&frameBuffer];
#ifdef DEBUG
    NSTimeInterval screenPresentationTimeOffset = -[start timeIntervalSinceNow];
#endif

#ifdef DEBUG
    NSTimeInterval duration = -[start timeIntervalSinceNow];
    if (duration > 1.0 / TARGET_FPS) {
        // oops, took too long
        NSLog(@"Frame time exceeded: %f ms", duration * 1000);
        NSLog(@"    Write audio:                %f ms", (audioWriteTimeOffset * 1000));
        NSLog(@"    Process events:             %f ms", (eventProcessingTimeOffset - audioWriteTimeOffset) * 1000);
        NSLog(@"    Execute:                    %f ms", (executionTimeOffset - eventProcessingTimeOffset) * 1000);
        NSLog(@"    Present screen:             %f ms", (screenPresentationTimeOffset - executionTimeOffset) * 1000);
    }
#endif // DEBUG
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

#pragma mark - NSOpenSavePanelDelegate

- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url {
    // always allow navigation into directories
    NSNumber *isDirectory;
    if ([url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil] &&
        [isDirectory boolValue]) {
        return YES;
    }
    
    NSArray *supportedTypes = @[ @"BIN", @"DO", @"DSK", @"NIB", @"PO", @"WOZ", @"ZIP", @"GZIP", @"GZ" ];
    return [supportedTypes containsObject:url.pathExtension.uppercaseString];
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

- (IBAction)toggleStatusBarAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    self.hasStatusBar = !self.hasStatusBar;
    self.statusBarView.hidden = !_hasStatusBar;
    [self updateDriveLights];
    
    CGRect emulatorFrame = self.emulatorVC.view.frame;
    CGRect windowFrame = self.window.frame;
    if (self.window.styleMask & NSWindowStyleMaskFullScreen) {
        emulatorFrame = windowFrame;
        if (self.hasStatusBar) {
            emulatorFrame.size.height -= STATUS_BAR_HEIGHT;
            emulatorFrame.origin.y += STATUS_BAR_HEIGHT;
        }
    }
    else {
        // windowed
        const double statusBarHeight = STATUS_BAR_HEIGHT;
        if (self.hasStatusBar) {
            self.showHideStatusBarMenuItem.title = NSLocalizedString(@"Hide Status Bar", @"");
            emulatorFrame.origin.y = statusBarHeight;
            windowFrame.size.height += statusBarHeight;
            windowFrame.origin.y -= statusBarHeight;
        }
        else {
            self.showHideStatusBarMenuItem.title = NSLocalizedString(@"Show Status Bar", @"");
            emulatorFrame.origin.y = 0;
            windowFrame.size.height -= statusBarHeight;
            windowFrame.origin.y += statusBarHeight;
        }
        
        // need to resize the window before the emulator view because the window
        // tries to resize its children when it's being resized.
        [self.window setFrame:windowFrame display:YES animate:NO];
    }
    [self.emulatorVC.view setFrame:emulatorFrame];
}

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

- (IBAction)actualSizeAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);

    CGRect frame = [self windowRectAtScale:1];
    [self.window setFrame:frame display:YES animate:NO];
}

- (IBAction)doubleSizeAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);

    CGRect frame = [self windowRectAtScale:2];
    [self.window setFrame:frame display:YES animate:NO];
}

- (IBAction)increaseSizeAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);

    [self scaleWindowByFactor:1.2];
}

- (IBAction)decreaseSizeAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);

    [self scaleWindowByFactor:0.8];
}

#pragma mark - Main window actions

- (IBAction)screenshotTaken:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);

#ifdef DEBUG
    NSDate *start = [NSDate now];
#endif
    
    // have the BMP screenshot written to a memory stream instead of a file...
    char *buffer;
    size_t bufferSize;
    FILE *memStream = open_memstream(&buffer, &bufferSize);
    GetVideo().Video_MakeScreenShot(memStream, Video::SCREENSHOT_560x384);
    fclose(memStream);

#ifdef DEBUG
    NSTimeInterval duration = -[start timeIntervalSinceNow];
    NSLog(@"Screenshot took: %f ms", duration * 1000);
#endif // DEBUG
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
#ifdef DEBUG
        NSDate *start = [NSDate now];
#endif
        // ...and then convert it to PNG for saving
        NSImage *image = [[NSImage alloc] initWithData:[NSData dataWithBytes:buffer length:bufferSize]];
        CGImageRef cgRef = [image CGImageForProposedRect:NULL context:nil hints:nil];
        NSBitmapImageRep *newRep = [[NSBitmapImageRep alloc] initWithCGImage:cgRef];
        [newRep setSize:[image size]];
        NSData *pngData = [newRep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        [pngData writeToURL:[self composeScreenshotPath] atomically:YES];
        free(buffer);
#ifdef DEBUG
    NSTimeInterval duration = -[start timeIntervalSinceNow];
    NSLog(@"PNG conversion took: %f ms", duration * 1000);
#endif // DEBUG
        [[NSSound soundNamed:@"Blow"] play];
    });
}

- (IBAction)driveLightAction:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    NSView *view = (NSView *)sender;
    const int slot = (int)(view.tag / 10);
    const int drive = (int)(view.tag % 10);
    // menu doesn't have a tag, so we stash the slot/drive in the title
    NSMenu *menu = [[NSMenu alloc] initWithTitle:[NSString stringWithFormat:@"%ld", view.tag]];
    menu.minimumWidth = 200;
    
    // if there's a disk in the drive, show it
    CardManager &cardManager = GetCardMgr();
    Disk2InterfaceCard *card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));
    NSString *diskName = [NSString stringWithUTF8String:card->GetFullDiskFilename(drive).c_str()];
    if ([diskName length] > 0) {
        [menu addItemWithTitle:diskName action:nil keyEquivalent:@""];
        [menu addItemWithTitle:NSLocalizedString(@"Eject", @"eject disk image")
                        action:@selector(ejectDisk:)
                 keyEquivalent:@""];
        [menu addItem:[NSMenuItem separatorItem]];
    }
    
    [menu addItemWithTitle:NSLocalizedString(@"Other…", @"open another disk image")
                    action:@selector(openOtherDisk:)
             keyEquivalent:@""];
    [menu popUpMenuPositioningItem:nil atLocation:CGPointZero inView:view];
}

- (void)ejectDisk:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    if ([sender isKindOfClass:[NSMenuItem class]]) {
        NSMenuItem *menuItem = (NSMenuItem *)sender;
        const int tag = [menuItem.menu.title intValue];
        const int slot = tag / 10;
        const int drive = tag % 10;
        
        CardManager &cardManager = GetCardMgr();
        Disk2InterfaceCard *card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));
        card->EjectDisk(drive);
    }
}

- (void)openOtherDisk:(id)sender {
    NSLog(@"%s", __PRETTY_FUNCTION__);
    
    if ([sender isKindOfClass:[NSMenuItem class]]) {
        NSMenuItem *menuItem = (NSMenuItem *)sender;
        const int tag = [menuItem.menu.title intValue];
        const int slot = tag / 10;
        const int drive = tag % 10;
        
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.canDownloadUbiquitousContents = YES;
        panel.delegate = self;
        
        if ([panel runModal] == NSModalResponseOK) {
            const char *fileSystemRepresentation = [panel.URL fileSystemRepresentation];
            std::string filename(fileSystemRepresentation);
            CardManager &cardManager = GetCardMgr();
            Disk2InterfaceCard *card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));
            const ImageError_e error = card->InsertDisk(drive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
            if (error == eIMAGE_ERROR_NONE) {
                NSLog(@"Loaded '%s' into slot %d drive %d",
                      fileSystemRepresentation, slot, drive);
            }
            else {
                NSLog(@"Failed to load '%s' into slot %d drive %d due to error %d",
                      fileSystemRepresentation, slot, drive, error);
                card->NotifyInvalidImage(drive, fileSystemRepresentation, error);
            }
        }
    }
}

#pragma mark - Helpers because I can't figure out how to make 'frame' properly global

- (BOOL)emulationHardwareChanged {
    return frame->HardwareChanged();
}

- (void)applyVideoModeChange {
    frame->ApplyVideoModeChange();
}

- (void)restartFrame {
    frame->Destroy();
    frame->Initialize(true);
}

- (void)reconfigureDrives {
    [self createDriveLightButtons];
}

- (int)showModalAlertofType:(int)type
                withMessage:(const char *)message
                information:(const char *)information
{
    NSAlert *alert = [[NSAlert alloc] init];
    
    if (message != NULL) {
        alert.messageText = [NSString stringWithUTF8String:message];
    }
    if (information != NULL) {
        alert.informativeText = [NSString stringWithUTF8String:information];
    }

    // the #defines unfortunately don't have bitmasks defined, but we'll
    // assume that's the intention.
    
    switch (type & 0x000000F0) {
        case MB_ICONINFORMATION:  // also MB_ICONASTERISK
            alert.alertStyle = NSAlertStyleInformational;
            alert.icon = [NSImage imageWithSystemSymbolName:@"info.circle" accessibilityDescription:@""];
            break;
        case MB_ICONSTOP:  // also MB_ICONHAND
            alert.alertStyle = NSAlertStyleCritical;
            // NSAlertStyleCritical comes with its own image already
            break;
        case MB_ICONQUESTION:
            alert.alertStyle = NSAlertStyleWarning;
            alert.icon = [NSImage imageWithSystemSymbolName:@"questionmark.circle" accessibilityDescription:@""];
            break;
        default:  // MB_ICONWARNING
            alert.alertStyle = NSAlertStyleWarning;
            alert.icon = [NSImage imageWithSystemSymbolName:@"exclamationmark.triangle" accessibilityDescription:@""];
            break;
    }
    
    switch (type & 0x0000000F) {
        case MB_YESNO:
            [alert addButtonWithTitle:NSLocalizedString(@"Yes", @"")];
            [alert addButtonWithTitle:NSLocalizedString(@"No", @"")];
            break;
        case MB_YESNOCANCEL:
            [alert addButtonWithTitle:NSLocalizedString(@"Yes", @"")];
            [alert addButtonWithTitle:NSLocalizedString(@"No", @"")];
            [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"")];
            break;
        case MB_OKCANCEL:
            [alert addButtonWithTitle:NSLocalizedString(@"OK", @"")];
            [alert addButtonWithTitle:NSLocalizedString(@"Cancel", @"")];
            break;
        case MB_OK:
        default:
            [alert addButtonWithTitle:NSLocalizedString(@"OK", @"")];
            break;
    }
    
    NSModalResponse returnCode = [alert runModalSheet];
    
    switch (type & 0x0000000F) {
        case MB_YESNO:
            return (returnCode == NSAlertFirstButtonReturn) ? IDYES : IDNO;
        case MB_YESNOCANCEL:
            if (returnCode == NSAlertFirstButtonReturn) {
                return IDYES;
            }
            else if (returnCode == NSAlertSecondButtonReturn) {
                return IDNO;
            }
            else {
                return IDCANCEL;
            }
        case MB_OK:
            return IDOK;
        case MB_OKCANCEL:
            return (returnCode == NSAlertFirstButtonReturn) ? IDOK : IDCANCEL;
    }
    return IDOK;

}

- (void)updateDriveLights {
    if (self.hasStatusBar) {
        for (NSButton *driveLightButton in self.driveLightButtons) {
            CardManager &cardManager = GetCardMgr();
            const UINT slot = (UINT)driveLightButton.tag / 10;
            const int drive = driveLightButton.tag % 10;
            Disk2InterfaceCard *card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));
            if (card->IsDriveEmpty(drive)) {
                driveLightButton.image = [NSImage imageWithSystemSymbolName:@"circle.dotted" accessibilityDescription:@""];
                driveLightButton.contentTintColor = [NSColor secondaryLabelColor];
            }
            else {
                Disk_Status_e status[NUM_DRIVES];
                card->GetLightStatus(&status[0], &status[1]);
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
        }
    }
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
    
    const NSInteger oldDriveLightButtonsCount = self.driveLightButtons.count;
    
    // remove the old light buttons, if any
    for (NSView *button in self.driveLightButtons) {
        [button removeFromSuperview];
    }
    
    NSMutableArray *driveLightButtons = [NSMutableArray array];
    NSInteger position = 0;
    CardManager &cardManager = GetCardMgr();
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
    
    if (self.driveLightButtons.count != oldDriveLightButtonsCount) {
        // constrain our window to not allow it to be resized so small that our
        // status bar buttons overlap
        [self.window setContentMinSize:[self minimumContentSizeAtScale:1]];
    }
}

- (CGSize)minimumContentSizeAtScale:(double)scale {
    NSSize minimumSize;
    // width of all the things in the status bar...
    minimumSize.width =
        self.driveLightButtonTemplate.frame.origin.x +                                      // left margin
        self.driveLightButtonTemplate.frame.size.width * self.driveLightButtons.count +     // drive light buttons
        40 +                                                                                // a healthy margin
        (self.window.frame.size.width - self.volumeToggleButton.frame.origin.x);            // buttons on the right
    // ...but no less than 2 pt per Apple ][ pixel
    Video &video = GetVideo();
    if (minimumSize.width < video.GetFrameBufferBorderlessWidth() * scale) {
        minimumSize.width = video.GetFrameBufferBorderlessWidth() * scale;
    }
    minimumSize.height = video.GetFrameBufferBorderlessHeight() * scale + self.statusBarHeight;  // status bar height
    return minimumSize;
}

- (CGRect)windowRectAtScale:(double)scale {
    CGRect windowFrame = self.window.frame;
    CGRect contentFrame = self.window.contentLayoutRect;

    CGRect frame;
    frame.size = [self minimumContentSizeAtScale:scale];
    frame.size.height += windowFrame.size.height - contentFrame.size.height;  // window chrome?

    // center the new window at the center of the old one
    frame.origin.x = windowFrame.origin.x + (windowFrame.size.width - frame.size.width) / 2;
    frame.origin.y = windowFrame.origin.y + (windowFrame.size.height - frame.size.height) / 2;
    
    return frame;
}

- (void)scaleWindowByFactor:(double)factor {
    CGRect windowFrame = self.window.frame;
    CGRect contentFrame = self.window.contentLayoutRect;
    
    CGRect frame;
    frame.size.width = contentFrame.size.width * factor;
    // keep status bar out of the scaling because it's fixed height
    frame.size.height = (contentFrame.size.height - [self statusBarHeight]) * factor;
    frame.size.height += [self statusBarHeight];

    // but no smaller than minimum
    CGSize minimumSize = [self minimumContentSizeAtScale:1];
    if (frame.size.width < minimumSize.width || frame.size.height < minimumSize.height) {
        frame.size = minimumSize;
    }
    
    frame.size.height += windowFrame.size.height - contentFrame.size.height;  // window chrome?
    
    // center the new window at the center of the old one
    frame.origin.x = windowFrame.origin.x + (windowFrame.size.width - frame.size.width) / 2;
    frame.origin.y = windowFrame.origin.y + (windowFrame.size.height - frame.size.height) / 2;
    
    [self.window setFrame:frame display:YES animate:NO];
}

- (void)startEmulationTimer {
    self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / TARGET_FPS target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
}

- (double)statusBarHeight {
    return self.hasStatusBar ? STATUS_BAR_HEIGHT : 0;
}

- (NSURL *)composeScreenshotPath {
    NSURL *folder = [[UserDefaults sharedInstance] screenshotsFolder];
    
    // walk through the folder to make a set of files that have our prefix
    NSDirectoryEnumerator *enumerator = [[NSFileManager defaultManager]
        enumeratorAtURL:folder
        includingPropertiesForKeys:nil
        options:(NSDirectoryEnumerationSkipsPackageDescendants | NSDirectoryEnumerationSkipsHiddenFiles)
        errorHandler:^(NSURL *url, NSError *error) { return YES; }];
    NSMutableSet *set = [NSMutableSet set];
    for (NSURL *url in enumerator) {
        NSString *filename = [url lastPathComponent];
        if ([filename hasPrefix:SCREENSHOT_FILE_NAME]) {
            [set addObject:filename];
        }
    }
    
    // starting from "1", let's find one that's not already used
    NSString *filename = [NSString stringWithFormat:@"%@.png", SCREENSHOT_FILE_NAME];
    NSInteger index = 2;
    while ([set containsObject:filename]) {
        filename = [NSString stringWithFormat:@"%@ %ld.png", SCREENSHOT_FILE_NAME, index++];
    }
    
    return [folder URLByAppendingPathComponent:filename];
}

@end

#pragma mark - Categories

@implementation NSAlert (Synchronous)

- (NSModalResponse)runModalSheetForWindow:(NSWindow *)aWindow
{
    [self beginSheetModalForWindow:aWindow completionHandler:^(NSModalResponse returnCode) {
        [NSApp stopModalWithCode:returnCode];
    }];
    NSModalResponse modalCode = [NSApp runModalForWindow:[self window]];
    return modalCode;
}

- (NSModalResponse)runModalSheet {
    return [self runModalSheetForWindow:[NSApp mainWindow]];
}

@end // NSAlert (Synchronous)

#pragma mark - C++ Helpers

int ShowModalAlertOfType(int type, const char *message, const char *information) {
    return [theAppDelegate showModalAlertofType:type withMessage:message information:information];
}

void UpdateDriveLights() {
    [theAppDelegate updateDriveLights];
}
