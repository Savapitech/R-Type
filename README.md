# R-Type

[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-informational.svg)
![Platform: Linux](https://img.shields.io/badge/platform-Linux-blue.svg)
![Platform: MacOS](https://img.shields.io/badge/platform-MacOS-blue.svg)
![Platform: Windows](https://img.shields.io/badge/platform-Windows-blue.svg)
[![CI](https://github.com/Savapitech/R-Type/actions/workflows/ci.yml/badge.svg)](https://github.com/Savapitech/R-Type/actions/workflows/ci.yml)
[![Documentation](https://github.com/Savapitech/R-Type/actions/workflows/docs.yml/badge.svg)](https://github.com/Savapitech/R-Type/actions/workflows/docs.yml)
[![Build](https://github.com/Savapitech/R-Type/actions/workflows/build.yml/badge.svg)](https://github.com/Savapitech/R-Type/actions/workflows/build.yml)

A networked, multiplayer remake of the classic horizontal shoot'em up R-Type,
built in C++20 on top of a custom game engine.

Up to four players connect to a central authoritative server and fight waves
of Bydos together. The project is split into reusable layers (an ECS based game
engine, a UDP networking library and a Vulkan sprite renderer) and the R-Type
client and server that are built on top of them.

## Table of contents

- [About the project](#about-the-project)
- [Supported platforms](#supported-platforms)
- [Download a release](#download-a-release)
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

| OS      | Prebuilt release binaries |
| ------- | ------------------------- |
| Linux   | x86_64, arm64             |
| macOS   | x86_64, arm64             |
| Windows | x86_64                    |

The CI builds and tests the server and the client on every platform above (see
[`.github/workflows/build.yml`](.github/workflows/build.yml)). Windows arm64 is
supported from source but no prebuilt binary is published for it yet.

## Download a release

Every tagged release (`v*`) contains standalone binaries built by the CI. The
assets, libraries and sprites are embedded in them, so **the only thing you need
installed is Vulkan** (a Vulkan capable GPU and driver, plus MoltenVK on macOS)
to run the client. The server needs nothing.

| Files                                      | Platform            |
| ------------------------------------------ | ------------------- |
| `r-type_server-linux-x86_64`, `r-type_client-linux-x86_64`     | Linux x86_64   |
| `r-type_server-linux-arm64`, `r-type_client-linux-arm64`       | Linux arm64    |
| `r-type_server-macos-x86_64`, `r-type_client-macos-x86_64`     | macOS Intel    |
| `r-type_server-macos-arm64`, `r-type_client-macos-arm64`       | macOS Apple silicon |
| `r-type_server-windows-x86_64.exe`, `r-type_client-windows-x86_64.exe` | Windows x86_64 |

```sh
chmod +x r-type_server-linux-x86_64 r-type_client-linux-x86_64   # Linux and macOS
./r-type_server-linux-x86_64 &
./r-type_client-linux-x86_64
```

They can be started from any directory. To compile the project yourself, see
[Building](#building).

## Requirements

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

If you only want to play, download a [release](#download-a-release). To build
from source, see [Building](#building).

## Building

[To build from source see the online documentation](https://r-type.savalet.dev/getting_started.html)

## Usage

The binaries load their assets with paths relative to the working directory,
so they must be started **from the repository root** (not necessary with embedded binaries).

### Server

```sh
./bin/r-type_server [port]
```

| Argument | Description                     | Default |
| -------- | ------------------------------- | ------- |
| `port`   | UDP port the server listens on  | `4242`  |

### Client

```sh
./bin/r-type_client [-h host] [-p port]
```

| Argument | Description                 | Default     |
| -------- | --------------------------- | ----------- |
| `host`   | Address of the server       | `127.0.0.1` |
| `port`   | UDP port of the server      | `4242`      |

Start several clients to play together, up to four players per game. Each
player gets a distinct ship color.

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
│   └── Common/          Code shared by both sides: protocol payloads, gameplay systems
├── docs/                Documentation website: Doxygen configuration and guides
├── tests/               Unit and functionnals tests
├── CMakeLists.txt       Build definition
├── Makefile             Build shortcuts
└── vcpkg.json           Dependency manifest
```

## Documentation and useful links

- [Developer documentation](https://r-type.savalet.dev/):
  architecture, engine and network guides, tutorials, technical study and
  API reference. Build it locally with `make docs` (requires Doxygen and
  Graphviz), the result is written to `build/docs/html`.
- [Network protocol specification](https://r-type.savalet.dev/protocol.html):
  every packet and message exchanged between clients and the server.
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
