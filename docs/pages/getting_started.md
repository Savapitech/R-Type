# Getting started {#getting_started}

[TOC]

This page takes you from a fresh clone to a running game, then shows how to
build this documentation locally. R-Type runs on Linux, macOS and Windows, on
x86_64 and arm64.

## Download a release (recommended)

Every tagged release on GitHub holds standalone binaries built by the CI, named
`r-type_server-<platform>` and `r-type_client-<platform>`:

| Platform | Suffix |
| -------- | ------ |
| Linux x86_64 | `linux-x86_64` |
| Linux arm64 | `linux-arm64` |
| macOS Intel | `macos-x86_64` |
| macOS Apple silicon | `macos-arm64` |
| Windows x86_64 | `windows-x86_64` (`.exe`) |

These are the *embedded* binaries (see @ref embedded_build): the sprites,
sounds, JSON blueprints, the graphical library and the C++ runtime are all
inside the executable. The only thing to install is **Vulkan**, and only to run
the client. The server needs nothing.

| OS | What to install |
| -- | --------------- |
| Linux | A Vulkan driver and the loader: `sudo apt install libvulkan1 mesa-vulkan-drivers` (NVIDIA users get them from the proprietary driver). The X11 client library, `libx11-6`, is present on any desktop install. |
| macOS | The Vulkan loader and MoltenVK: `brew install vulkan-loader molten-vk` |
| Windows | Nothing more than an up to date GPU driver, which ships `vulkan-1.dll`. |

Then download the two files (from https://github.com/Savapitech/R-Type/releases/latest) for your platform and start them from any
directory:

```sh
chmod +x r-type_server-linux-x86_64 r-type_client-linux-x86_64   # Linux and macOS
./r-type_server-linux-x86_64 &
./r-type_client-linux-x86_64
```

On Windows, double click the `.exe` files or run them from a terminal. The
server listens on UDP port 4242 and the client connects to `127.0.0.1:4242`,
see @ref running for the options and the controls.

Windows arm64 is supported from source, but the CI publishes no binary for it.

## Build from source

This is the path for developers. It produces the regular binaries, which read
their assets from the `assets/` folder of the repository.

### Requirements

| Tool | Notes |
| ---- | ----- |
| C++20 compiler | GCC (Linux), Apple Clang (macOS) or MSVC (Windows) |
| CMake | 3.20 or newer |
| vcpkg | `VCPKG_ROOT` must point to the installation |
| GNU Make | Optional, drives the `Makefile` (`choco install make` on Windows) |
| Git, curl, zip, unzip, tar, pkg-config | Used by vcpkg to fetch dependencies |
| Vulkan capable GPU and driver | Needed to run the client only |

The low level system libraries are installed per platform.

**Linux** (Debian or Ubuntu, x86_64 and arm64):

```sh
sudo apt install build-essential cmake git curl zip unzip tar pkg-config \
    libx11-dev libx11-xcb-dev libxcb1-dev libxrandr-dev libwayland-dev \
    libvulkan1 mesa-vulkan-drivers
```

**macOS** (Intel and Apple silicon), with Homebrew:

```sh
xcode-select --install
brew install cmake ninja git pkg-config vulkan-headers vulkan-loader molten-vk
```

**Windows** (x86_64 and arm64): install Visual Studio 2022 or newer with the
"Desktop development with C++" workload, Git, CMake, GNU Make
(`choco install make`) and the Vulkan SDK from
https://vulkan.lunarg.com/sdk/home.

Every other dependency (Asio, nlohmann/json, GLM, miniaudio, stb, the Vulkan
loader, the rtk ECS and the rtk graphical library) is downloaded during the
configure step. Nothing is installed system wide.

### Building

The `VCPKG_ROOT` environment variable must point to your vcpkg installation.
The first build takes longer because vcpkg compiles the dependencies; they are
cached for the following builds.

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

git clone https://github.com/Savapitech/R-Type.git
cd R-Type
make
```

On Windows, use a Developer PowerShell for Visual Studio instead:

```powershell
git clone https://github.com/microsoft/vcpkg.git $HOME\vcpkg
& $HOME\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = "$HOME\vcpkg"

git clone https://github.com/Savapitech/R-Type.git
cd R-Type
make
```

`make` configures CMake with the vcpkg toolchain and builds in `Release` mode.
The two executables are written to `bin/`.

#### With the Makefile

| Command | Effect |
| ------- | ------ |
| `make` | Release build |
| `make server`, `make client` | Builds only one of the two programs (the server needs neither Vulkan nor the graphical library) |
| `make debug` | Debug build |
| `make clean` | Removes the CMake build directory |
| `make fclean` | Also removes fetched libraries and binaries |
| `make re` | Full rebuild |
| `make format` | Formats the sources with clang-format |
| `make docs` | Builds this documentation website |
| `make server_embedded`, `make client_embedded` | Standalone binaries as published in the releases, see below |

#### With CMake directly

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

On Windows, run the same commands from a Developer PowerShell for Visual
Studio, with `VCPKG_ROOT` set. The executables are written to the `bin/`
directory at the root of the repository.

### Building the embedded binaries {#embedded_build}

The embedded binaries are the ones attached to the releases. The CI builds them
with the same two commands, using the same requirements as above:

```sh
make server_embedded
make client_embedded
```

They are written to `bin/r-type_server_embedded` and
`bin/r-type_client_embedded` (with `.exe` on Windows). They use their own build
directories, `build-embedded-server` and `build-embedded-client`, so they never
share a CMake cache with the regular build. On Windows the CI sets
`EMBED_TRIPLET=x64-windows-static` so vcpkg builds static libraries, which is
also the default of the Makefile on that platform.

**What it does.** A regular build needs the `assets/` folder next to the working
directory and, for the client, the graphical shared library next to the
executable. An embedded build is a single file that carries all of it, so it
can be copied anywhere and run.

**How it works.**

- *Assets.* At build time `cmake/EmbedAssets.cmake` reads every file of
  `assets/` and generates `EmbeddedAssets.cpp`, a C++ file that holds each file
  as a byte array together with a table of paths. It also computes a build id,
  the hash of all file contents. This file is compiled into the
  `ResourcesEmbedded` library, which defines `RTYPE_EMBED_ASSETS`.
- *Start up.* Both programs call `rtype::res::mount()` first. In an embedded
  build it extracts the assets into `<temp dir>/rtype-<build id>/`, skipping the
  files that already exist with the right size, then changes the working
  directory there. The rest of the code keeps loading `assets/...` with relative
  paths and cannot tell the difference. A regular build makes `mount()` do
  nothing. Because the build id is part of the folder name, two versions never
  overwrite each other's files.
- *Libraries.* The client links the static build of the graphical library
  (`rtk_graphical-<platform>-static`, downloaded from the
  [rtk-lib/graphical](https://github.com/rtk-lib/graphical) releases) instead
  of the shared one. The C++ runtime is linked statically too (`-static-libstdc++
  -static-libgcc` on Linux, the static MSVC runtime on Windows). What remains
  dynamic are system libraries: the Vulkan loader, X11 on Linux and the system
  frameworks on macOS, which is why Vulkan is the only thing to install.

## Running {#running}

Both programs load their assets with paths relative to the working directory,
so start them from the repository root.

```sh
./bin/r-type_server [port]   # listens on UDP port 4242 by default
./bin/r-type_client          # connects to 127.0.0.1:4242
```

On Windows, run `.\bin\r-type_server.exe` and `.\bin\r-type_client.exe`.

Start one client per player, up to four. The server waits in a lobby and starts
the round as soon as two players are connected, or 30 seconds after the first
player joined.

| Action | Keyboard | Gamepad |
| ------ | -------- | ------- |
| Move | Arrow keys | D-pad or left stick |
| Shoot | Space | Right trigger |
| Confirm in menus | Enter | Cross or A |
| Quit | Escape | |
| Show collision boxes | Hold F3 and press B | |

## Building the documentation

The website is generated by Doxygen from the guides in `docs/pages` and from
the comments of the public headers.

```sh
sudo apt install doxygen graphviz
make docs
```

`make docs` downloads the doxygen-awesome theme into `External/` on first use,
then writes the website to `build/docs/html`. Open
`build/docs/html/index.html` in a browser. Warnings, such as a public function
without documentation, are written to `build/docs/warnings.log`.

On macOS, use `brew install doxygen graphviz`; on Windows, `choco install
doxygen.install graphviz`.

The website is rebuilt and published automatically for every push on `main`.
