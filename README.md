# aminogfx-gl

AminoGfx implementation for OpenGL 2 / OpenGL ES 2. Node.js based animation framework supporting images, texts, primitives, 3D transformations and realtime animations. Hardware accelerated video support on Raspberry Pi.

## Platforms

* macOS
* Raspberry Pi

## Requirements

In order to build the native components a couple of libraries and tools are needed.

* Node.js 4.x to 16.x
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
sudo port install pkgconfig glfw freetype ffmpeg +quartz
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
