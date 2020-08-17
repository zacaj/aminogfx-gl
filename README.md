# This Fork
Rough WIP fixes and modifications:
- Windows and Linux support
    - Windows support is missing video, jpeg, etc support, since I didn't want to deal with all those libraries, etc when porting
    - Linux support is probably complete, but untested
- RPI
    - only tested on RPi 3 + Zero W with fake KMS driver, not RPi 4
    - Builds using older DispManX code instead of newer GBM, to work with older RPi/driver
    - partial keyboard support (needs more keys mapped from linux to glfw codes)
- Functionality
    - Changed how origin works so it makes more sense
    - Added partial typescript bindings
    - Allow passing 'null' as the target of keyboard event listeners to always receive events
    - Fix texture leaking when changing source of ImageView- now updates GL texture with new data
    - Fix mouse click detection issue when using originX/Y (still ignores rotation)



# aminogfx-gl

AminoGfx implementation for OpenGL 2 / OpenGL ES 2. Node.js based animation framework supporting images, texts, primitives, 3D transformations and realtime animations. Hardware accelerated video support on Raspberry Pi.

## Platforms

* macOS
* Raspberry Pi

## Requirements

In order to build the native components a couple of libraries and tools are needed.

* Node.js 4.x to 14.x
 * There is a bug in Node.js v6.9.1 (see https://github.com/nodejs/node/issues/9288; fixed in Node.js > 6.10).
* Freetype 2.7
* libpng
* libjpeg
* libswscale

### macOS

* GLFW 3.3
* FFMPEG

MacPorts setup:

```
sudo port install glfw freetype ffmpeg
```

Homebrew setup:

```
brew install pkg-config
brew tap homebrew/versions
brew install glfw3
brew install freetype
```

### Raspberry Pi

* libegl1-mesa-dev
* libdrm-dev
* libgbm-dev
* libfreetype6-dev
* libjpeg-dev
* libav
* libswscale-dev
* libavcodec-dev
* Raspbian (other Linux variants should work too)

Setup:

```
sudo rpi-update
sudo apt-get install libegl1-mesa-dev libdrm-dev libgbm-dev libfreetype6-dev libjpeg-dev libavformat-dev libswscale-dev libavcodec-dev
```

## Installation

```
npm install
```

## Build

During development you'll want to rebuild the source constantly:

```
npm install --build-from-source
```

Or use:

```
./rebuild.sh
```

## Demo

```
node demos/circle.js
```

Example of all supported features are in the demos subfolder.

## Troubleshooting

* node: ../src/rpi.cpp:209: void AminoGfxRPi::initEGL(): Assertion `success >= 0' failed.
  * select a screen resolution with raspi-config
