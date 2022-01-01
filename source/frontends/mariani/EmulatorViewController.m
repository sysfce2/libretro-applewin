//
//  EmulatorViewController.m
//  Mariani
//
//  Copyright © 2021 Apple Inc.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#import "EmulatorViewController.h"

@import MetalKit;
#import "EmulatorRenderer.h"

@interface EmulatorViewController ()

@end

@implementation EmulatorViewController {
    MTKView *_view;
    EmulatorRenderer *_renderer;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    _view = (MTKView *)self.view;
    _view.enableSetNeedsDisplay = NO;
    _view.device = MTLCreateSystemDefaultDevice();
}

- (void)createScreen:(FrameBuffer *)frameBuffer {
    if (_renderer == nil) {
        _renderer = [[EmulatorRenderer alloc] initWithMetalKitView:_view frameBuffer:frameBuffer];
        if (_renderer == nil)
        {
            NSLog(@"Renderer initialization failed");
            return;
        }

        // Initialize the renderer with the view size.
        [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];
        
        _view.delegate = _renderer;
    }
    
    [_renderer createTexture];
}

- (void)updateScreen:(FrameBuffer *)frameBuffer {
    [_renderer updateTextureWithData:frameBuffer->data];
}

@end
