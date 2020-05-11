# Building vlc-tip-plugin
This document contains instructions to build the plugin from source.

To build the plugin you need the __libVLC SDK__ (VLC header files, the pkg-config files, and the import libraries) and the development toolchain (gcc, make and pkg-config).
The build process is slightly different depending on what OS you build on.

## Docker
The most easy way to build the plugin for Linux and Windows is to use Docker. There are Dockerfile in the *docker* directory for creating a docker image with all required packages installed.
To build the docker image run the following commands:
```sh
cd vlc-tip-plugin
docker build -t vlc-plugin-build docker
```

Now to build the plugin for both Linux and Windows (32-bit and 64-bit) you can just run the commands:
```sh
cd vlc-tip-plugin
docker run --rm -v "$PWD:/plugin" vlc-plugin-build
```

The result plugin files will be stored under the *build* directory.

## Linux
On Debian/Ubuntu, you can simply install the required packages using apt-get:
```sh
sudo apt-get install libvlc-dev libvlccore-dev gcc make pkg-config
```
After installing the packages cd to the plugin directory and run make:
```sh
cd vlc-tip-plugin
make
```
And then you can install the TIP plugin:
```sh
sudo make install
```
## Windows
First of all, you need to install development toolchain. I recommend to use **MSYS2** (www.msys2.org) to build the plugin on Windows. After installing MSYS2 open the MSYS2 shell and install the MinGW compiler toolchain:
```sh
pacman -S base-devel mingw-w64-x86_64-toolchain
```
Then to get __libVLC SDK__ on Windows, you need to download the .7z version of VLC. For example, you can download libVLC SDK 3.0.8 for Windows 64-bit from the link below:
http://get.videolan.org/vlc/3.0.8/win64/vlc-3.0.8-win64.7z

Extract the _sdk_ directory from the downloaded vlc-*-win*.7z file.
After that you need to fix pkg-config files inside the sdk directory. Open the __MSYS2 MinGW__ shell and run the commands below:
```sh
cd sdk
sed -i "s|^prefix=.*|prefix=${PWD}|g" lib/pkgconfig/*.pc
export PKG_CONFIG_PATH="${PWD}/lib/pkgconfig:$PKG_CONFIG_PATH"
```
Make sure that the following commands print the correct include and library paths: 
```sh
pkg-config --cflags vlc-plugin 
pkg-config --libs vlc-plugin
```
Now you are ready to build the plugin, cd to the plugin directory and run make:
```sh
cd vlc-tip-plugin
make
```
A file named **_libtip_plugin.dll_** should be generated.
## macOS
First of all, you need to install development toolchain. Launch the Terminal and type the following command:
```sh
brew install make gcc pkg-config
```
Then to get __libVLC SDK__ on macOS, you need to install [VLC](https://www.videolan.org/). VLC for macOS contains all libVLC SDK files except some header and pkg-config files. You can copy the missing files from libVLC SDK for Windows.
```sh
curl -OL ftp://ftp.videolan.org/pub/videolan/vlc/3.0.8/win64/vlc-3.0.8-win64.7z
brew install p7zip
7za x vlc-3.0.8-win64.7z -owin-sdk sdk\* -r

mkdir macos-sdk && cd macos-sdk
cp -r /Applications/VLC.app/Contents/MacOS/{include,lib} .
cp -r ../win-sdk/vlc-3.0.8/sdk/include/vlc/plugins ./include/vlc/
cp -r ../win-sdk/vlc-3.0.8/sdk/lib/pkgconfig ./lib/
sed -i "" "s|^prefix=.*|prefix=${PWD}|g" lib/pkgconfig/*.pc
export PKG_CONFIG_PATH="${PWD}/lib/pkgconfig"
```
Now you are ready to build the plugin, cd to the plugin directory and run make:
```sh
cd vlc-tip-plugin
make
```
A file named **_libtip_plugin.dylib_** should be generated.