# macOS

## Introduction

This is intended to be a native macOS UI for [AppleWin](https://github.com/AppleWin/AppleWin), by way of [Andrea](https://github.com/audetto)'s [Raspberry Pi port](https://github.com/audetto/AppleWin). You can build a macOS command-line app from Andrea's repo with the instructions below.

The rationale for this fork is to experiment with a native macOS UI, but you should be able to build [sa2](source/frontends/sdl/README.md) from here as well. You can check out the work-in-progress by opening Mariani.xcodeproj in Xcode.

## Build

### Dependencies

You can easily pick up the libraries not available by default on macOS using [Homebrew](https://brew.sh). After you install Homebrew, pick up the required packages below:

```
brew install cmake pkgconfig libyaml minizip libslirp libpcap Boost sdl2 sdl2_image
```

### Checkout

Next, you'll probably want to generate an Xcode project to take advantage of source code indexing and finding the lines of code with warnings or errors. The parameters assume you just want the imgui frontend:

```
git clone https://github.com/sh95014/AppleWin.git --recursive
cd AppleWin
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE -DBUILD_SA2=ON -G Xcode ..
open applewin.xcodeproj
```

The project should now launch in Xcode. Select the `ALL_BUILD` target and build. You can look under `Products` in the left pane to see where it is, because unfortunately Xcode does not seem to be able to run and debug the binary.

Or, you can follow basically the same as in [Linux](linux.md), also simplified to build only the sa2 frontend:

```
git clone https://github.com/sh95014/AppleWin.git --recursive
cd AppleWin
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE -DBUILD_SA2=ON ..
make
```

It works pretty well (macOS Monterey on M1 Pro) as far as I can tell, but I certainly haven't tried all the peripherals and available software. YMMV.