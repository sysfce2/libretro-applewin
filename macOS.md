# macOS

## Introduction

Mariani is a native macOS UI for [AppleWin](https://github.com/AppleWin/AppleWin), by way of [Andrea](https://github.com/audetto)'s [Raspberry Pi port](https://github.com/audetto/AppleWin). I built this because while I greatly respect cross-platform UI, I don't actually like to use it, and I wanted a native experience under macOS and one did not seem to exist.

But if what you want is a macOS command-line app, you can build it from Andrea's repo with the instructions below.

### Features

- Native, universal macOS UI
- Screen recording
- Copy screenshot to pasteboard
- Disk image browser
- Floppy and hard disk image creation
- Full-screen support

### AppleWin Features Not Yet Supported

- Debugger and memory viewer
- Load/save emulator state
- Cassette support

### Known Issues

- plugging or unplugging a VidHD does not refresh the display correctly
- Screen recording does not record audio
- Screen recording seems to occur at half speed on 120Hz devices
- Certain key combinations are not sent to the emulator
- Code could be better commented

### Suspected Issues

- Joystick may not work. Support should be inherited from the Raspberry Pi port but I don't have one yet and haven't tested it.
- AppleWin supports lots of esoteric hardware, I've really just been using the floppy drive, hard drive, 80-column card, and Mockingboard.
- Moving the main window to another monitor may cause issues.

## Build Mariani

### Dependencies

The easiest way to build Mariani is to satisfy the dependencies using [Homebrew](https://brew.sh). After you install Homebrew, pick up the required packages below:

```
brew install Boost sdl2
```

### Checkout

Now we're ready to grab the source code:

```
git clone https://github.com/sh95014/AppleWin.git --recursive
```

Load up the Xcode project, and build the "Mariani" target for "My Mac".

"Mariani Pro" contains an additional feature to browse inside a floppy disk image that you've loaded into the emulator. For now you need to go to `source/frontends/mariani/CiderPress/nufxlib` and run `./configure`, and then build the "Mariani Pro" target back in Xcode. (Somebody let me know how to improve that please!)

"Mariani Pro Universal" is even more of a pain. Unfortunately, Homebrew does not support universal (x86 and ARM) libraries, so you'll have to grab the [Development Library for SDL](https://www.libsdl.org/download-2.0.php) and build Boost yourself. Here's a script that should help:

```
#!/bin/sh

rm -rf arm64 x86_64 universal stage bin.v2
rm -f b2 project-config*
./bootstrap.sh cxxflags="-arch x86_64 -arch arm64" cflags="-arch x86_64 -arch arm64" linkflags="-arch x86_64 -arch arm64"
./b2 toolset=clang-darwin target-os=darwin architecture=arm abi=aapcs cxxflags="-arch arm64" cflags="-arch arm64" linkflags="-arch arm64" -a
mkdir -p arm64 && cp stage/lib/*.dylib arm64 && cp stage/lib/*.a arm64
./b2 toolset=clang-darwin target-os=darwin architecture=x86 cxxflags="-arch x86_64" cflags="-arch x86_64" linkflags="-arch x86_64" abi=sysv binary-format=mach-o -a
mkdir x86_64 && cp stage/lib/*.dylib x86_64 && cp stage/lib/*.a x86_64
mkdir universal
for dylib in arm64/*; do 
  lipo -create -arch arm64 $dylib -arch x86_64 x86_64/$(basename $dylib) -output universal/$(basename $dylib); 
done
for dylib in universal/*; do
  lipo $dylib -info;
done
```

## Build sa2

sa2 is the binary produced by Andrea's port.

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