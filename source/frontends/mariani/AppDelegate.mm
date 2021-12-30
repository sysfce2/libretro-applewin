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

#import "Interface.h"

#import "CommonTypes.h"
#import "EmulatorViewController.h"

#define TARGET_FPS          60

@interface AppDelegate ()

@property (strong) IBOutlet NSWindow *window;

@property NSTimer *timer;
@property Initialisation *initialisation;
@property LoggerContext *logger;
@property RegistryContext *registryContext;

@property (strong) IBOutlet EmulatorViewController *emulatorVC;

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
        
        self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / TARGET_FPS target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
    }
}

- (void)timerFired {
    sa2::writeAudio();
    
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

@end
