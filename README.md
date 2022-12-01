# aminogfx-gl

AminoGfx implementation for OpenGL 2 / OpenGL ES 2. Node.js based animation framework supporting images, texts, primitives, 3D transformations and realtime animations. Hardware accelerated video support on Raspberry Pi.

## Platforms

* macOS
* Linux
* Raspberry Pi

## Requirements

In order to build the native components a couple of libraries and tools are needed.

* Node.js 4.x to 18.x
  * There is a bug in Node.js v6.9.1 (see <https://github.com/nodejs/node/issues/9288>; fixed in Node.js > 6.10).
* Freetype 2.7
* libpng
* libjpeg
* libswscale

The node-gyp module has to be globally installed:

```bash
sudo npm i node-gyp -g
```

### macOS

* GLFW 3.3
* FFMPEG

MacPorts setup:

```bash
sudo port install pkgconfig glfw freetype ffmpeg +quartz
```

Homebrew setup:

```bash
brew install pkg-config
brew tap homebrew/versions
brew install glfw3
brew install freetype
```

### Linux

* libegl1-mesa-dev
* mesa-common-dev
* libdrm-dev
* libgbm-dev
* libfreetype6-dev
* libjpeg-dev
* libswscale-dev
* libglew-dev
* libglfw3
* libglfw3-dev
* libavformat-dev
* libvips-dev

Setup:

```bash
sudo apt-get install libegl1-mesa-dev mesa-common-dev libdrm-dev libgbm-dev libfreetype6-dev libjpeg-dev libswscale-dev libglew-dev libglfw3 libglfw3-dev libavformat-dev libvips-dev
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
* libva-dev

OS is Raspberry Pi OS (former Raspbian).

Setup:

```bash
sudo rpi-update
sudo apt-get install libegl1-mesa-dev libdrm-dev libgbm-dev libfreetype6-dev libjpeg-dev libavformat-dev libswscale-dev libavcodec-dev libva-dev g++
```

## Installation

```bash
npm install
```

## Build

During development you'll want to rebuild the source constantly:

```bash
npm install --build-from-source
```

Or use:

```bash
./rebuild.sh
```

## Demo

```bash
node demos/circle.js
```

Example of all supported features are in the demos subfolder.

## Troubleshooting

* node: ../src/rpi.cpp:209: void AminoGfxRPi::initEGL(): Assertion `success >= 0' failed.
  * select a screen resolution with raspi-config
