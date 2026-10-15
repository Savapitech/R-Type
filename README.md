# R-Type

[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-informational.svg)
![Platform: Linux, Windows, MacOS](https://img.shields.io/badge/platform-Linux-lightgrey.svg)
[![CI](https://github.com/Savapitech/R-Type/actions/workflows/ci.yml/badge.svg)](https://github.com/Savapitech/R-Type/actions/workflows/ci.yml)

A networked, multiplayer remake of the classic horizontal shoot'em up R-Type,
built in C++20 on top of a custom game engine.

Up to four players connect to a central authoritative server and fight waves
of Bydos together. The project is split into reusable layers (an ECS based game
engine, a UDP networking library and a Vulkan sprite renderer) and the R-Type
client and server that are built on top of them.

## Table of contents

- [About the project](#about-the-project)
- [Supported platforms](#supported-platforms)
- [Requirements](#requirements)
- [Quick start](#quick-start)
- [Building](#building)
- [Usage](#usage)
- [Project layout](#project-layout)
- [Documentation and useful links](#documentation-and-useful-links)
- [Contributing](#contributing)
- [Authors](#authors)
- [License](#license)

## About the project

This repository contains two programs:

| Binary          | Role                                                                                          |
| --------------- | --------------------------------------------------------------------------------------------- |
| `r-type_server` | Authoritative, multithreaded game server. Runs the game simulation and replicates it to clients over UDP. |
| `r-type_client` | Graphical client. Renders the game, plays sounds and sends the player's inputs to the server.  |

Main characteristics:

- **Client and server architecture.** The server owns the game state and the
  clients comply with its decisions.
- **Binary protocol over UDP.** All in-game communication (entities, movements,
  events) goes through a documented binary protocol.
- **Custom game engine.** Rendering, networking and game logic are decoupled
  subsystems built around an Entity Component System, a scene manager and an
  event bus.
- **Data driven entities.** Entities are described in JSON blueprints under
  `assets/descriptionEntity/`, so new content does not require C++ changes.
- **Time based simulation.** Movement, animation and scrolling are driven by
  the elapsed frame time, never by CPU speed.

The project is developed as part of the Epitech Advanced C++ curriculum.

## Supported platforms

| Platform         |
| ---------------- |
| Linux (x64, arm64)  |
| Windows (x64, arm64)    |
| MacOS (x64, arm64)    |

## Requirements

### Toolchain

| Tool                                      | Minimum version |
| ----------------------------------------- | --------------- |
| C++20 compiler (GCC, Clang or MSVC)       | C++20 support   |
| [CMake](https://cmake.org/)               | 3.20            |
| [vcpkg](https://github.com/microsoft/vcpkg) | any recent release |
| GNU Make (optional, for the `Makefile`)   | any             |
| Git, curl, zip, unzip, tar, pkg-config    | any             |

### System libraries (Linux)

Only low level platform libraries are taken from the system. On Debian or
Ubuntu they can be installed with:

```sh
sudo apt install build-essential cmake git curl zip unzip tar pkg-config \
    libx11-dev libx11-xcb-dev libxcb1-dev libxrandr-dev libwayland-dev \
    libvulkan1 mesa-vulkan-drivers
```

A GPU and driver with Vulkan support are required to run the client.

### Third-party dependencies

All other dependencies are downloaded automatically during the configure step.
Nothing is vendored in the repository.

| Dependency                                              | Purpose                       | Provided by |
| ------------------------------------------------------- | ----------------------------- | ----------- |
| [Asio](https://think-async.com/Asio/)                   | Asynchronous UDP networking   | vcpkg       |
| [nlohmann/json](https://github.com/nlohmann/json)       | Entity blueprint loading      | vcpkg       |
| [GLM](https://github.com/g-truc/glm)                    | Math                          | vcpkg       |
| [miniaudio](https://miniaud.io/)                        | Audio playback                | vcpkg       |
| [stb](https://github.com/nothings/stb)                  | Image loading                 | vcpkg       |
| Vulkan headers and loader                               | Rendering backend             | vcpkg       |
| [rtk ECS](https://github.com/rtk-lib/ECS)               | Entity Component System       | CMake `FetchContent` |
| [rtk graphical](https://github.com/rtk-lib/graphical)   | Window, rendering, input, audio | CMake `FetchContent` |

The vcpkg manifest is [`vcpkg.json`](vcpkg.json).

## Quick start

```sh
# 1. Install vcpkg once (skip if VCPKG_ROOT is already set)
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg

# 2. Clone and build
git clone git@github.com:Savapitech/R-Type.git
cd R-Type
make

# 3. Run a server and a client, from the repository root
./bin/r-type_server &
./bin/r-type_client
```

## Building

The `VCPKG_ROOT` environment variable must point to your vcpkg installation.
The first build takes longer because vcpkg compiles the dependencies; they are
cached for the following builds.

### With the Makefile

| Command      | Effect                                                    |
| ------------ | --------------------------------------------------------- |
| `make`       | Configure and build in `Release` mode                     |
| `make debug` | Configure and build in `Debug` mode                       |
| `make clean` | Remove the CMake build directory                          |
| `make fclean`| Also remove the fetched libraries and the binaries        |
| `make re`    | Full rebuild                                              |
| `make format`| Format the sources with `clang-format`                    |

### With CMake directly

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

On Windows, run the same commands from a Developer PowerShell for Visual
Studio, with `VCPKG_ROOT` set.

Both executables are written to the `bin/` directory at the root of the
repository.

## Usage

The binaries load their assets with paths relative to the working directory,
so they must be started **from the repository root**.

### Server

```sh
./bin/r-type_server [port]
```

| Argument | Description                     | Default |
| -------- | ------------------------------- | ------- |
| `port`   | UDP port the server listens on  | `4242`  |

### Client

```sh
./bin/r-type_client
```

The client currently connects to `127.0.0.1` on UDP port `4242`. Start several
clients to play together, up to four players per game. Each player gets a
distinct ship color.

### Controls

| Action        | Keyboard     | Gamepad                    |
| ------------- | ------------ | -------------------------- |
| Move          | Arrow keys   | D-pad or left stick        |
| Shoot         | Space        | Right trigger (R2)         |
| Confirm (menu)| Enter        | Cross / A                  |

## Project layout

```text
.
├── assets/              Sprites, sounds and JSON entity blueprints
├── lib/
│   ├── GameEngine/      Engine core: ECS components, systems, scenes, event bus
│   └── Network/         UDP transport, packet framing, connections, server
├── src/
│   ├── Client/          r-type_client: scenes, input, rendering adapters
│   ├── Server/          r-type_server: game simulation and replication
│   ├── Common/          Code shared by both sides: protocol payloads, gameplay systems
│   └── protocole.md     Network protocol specification
├── tests/               Unit and functionnals tests
├── CMakeLists.txt       Build definition
├── Makefile             Build shortcuts
└── vcpkg.json           Dependency manifest
```

## Documentation and useful links

- [Network protocol specification](src/protocole.md): message types and
  payload formats exchanged between clients and the server.
- [Issue tracker](https://github.com/Savapitech/R-Type/issues): bug reports
  and feature requests.
- [Pull requests](https://github.com/Savapitech/R-Type/pulls): ongoing work.
- [rtk-lib/ECS](https://github.com/rtk-lib/ECS) and
  [rtk-lib/graphical](https://github.com/rtk-lib/graphical): the ECS and
  rendering libraries used by the engine.
- [vcpkg documentation](https://learn.microsoft.com/vcpkg/): help with the
  package manager.

## Contributing

Contributions follow a pull request workflow:

1. Create a feature branch from `main`.
2. Format the code with `make format` before committing.
3. Write commit messages that follow
   [Conventional Commits](https://www.conventionalcommits.org/), for example
   `feat(server): broadcast monster spawns`. The CI rejects commits that do
   not match the format checked by
   [`.github/scripts/check-commit-msg.sh`](.github/scripts/check-commit-msg.sh).
4. Open a pull request against `main` and request a review from another
   team member.

Please open an issue before starting work on a large change.

## Authors

| Name               | Contact                                                      |
| ------------------ | ------------------------------------------------------------ |
| Arthur Gauthier    | [Arthur-gtr](https://github.com/Arthur-gtr) |
| Titouan Hochet     | [Titouanhct](https://github.com/Titouanhct)   |
| Enzo Iffis         | [eiffis](https://github.com/eiffis)           |
| Savinien Petitjean | [savalet](https://github.com/savalet) |

## License

This project is licensed under the GNU General Public License v3.0. See the
[LICENSE](LICENSE) file for the full text.

R-Type is a trademark of its respective owners. This project is a non-commercial
educational remake and is not affiliated with or endorsed by them.
