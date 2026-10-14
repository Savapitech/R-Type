# R-Type - Part 1 checklist

Everything the subject requires for the first delivery (software architecture and first game prototype).

**Deadline:** the last commit before **Sunday 11:42 PM** is the one evaluated, in a defense in front of the local team.

Legend: **MUST** = mandatory, **SHOULD** = strongly expected, **MAY** = optional.

---

## 1. Gameplay

- [ ] **MUST** - The game is playable end to end, over the network, with several clients on one server - _the client is a local single-player prototype and does not use the Network lib yet; the server has no game logic_
- [x] **MUST** - The client displays a slow horizontal scrolling starfield (stars, planets, ...) - _no background at all yet_
- [x] **MUST** - Starfield scrolling, entity behavior and overall time flow are driven by timers, never by CPU speed - _engine movement and animation use `dt` and the client caps at 60 FPS; the starfield and the server loop do not exist yet_
- [x] **MUST** - Players move with the arrow keys - _verified in code only; note that every entity of `GameScene` (including the placeholder enemy) gets `Controllable`, so all of them move together_
- [x] **MUST** - Bydos monsters exist in the game - _only an `Enemy` collision layer exists; no monster entity, sprite or behavior_
- [ ] **MUST** - Missiles exist (players and enemies can shoot) - _player missiles exist locally on the client (`PlayerSystem`, `Bullet`); no enemy missiles_
- [x] **MUST** - Monsters spawn randomly on the right side of the screen
- [x] **MUST** - The four players of a game are distinctly identifiable (color, sprite, ...)
- [x] Basic sound design (music and/or sound effects) - _the graphical lib provides a sound manager, nothing uses it yet_
- [x] Use the sprite set provided with the subject (or other freely available R-Type sprites)

Optional elements of the official screen (not mandatory for part 1):

- [ ] **MAY** - Monster that spawns a powerup on death
- [x] **MAY** - Stage obstacles
- [ ] **MAY** - Destroyable tiles

---

## 2. Server

- [x] **MUST** - The server is authoritative: it runs all the game logic and clients comply with its decisions - _`Server` only receives and decodes `Input` packets so far; there is no simulation_
- [x] **MUST** - The server notifies every client when a monster spawns
- [x] **MUST** - The server notifies every client when a monster moves
- [x] **MUST** - The server notifies every client when a monster is destroyed
- [ ] **MUST** - The server notifies every client when a monster fires
- [x] **MUST** - The server notifies every client when a player is killed
- [x] **MUST** - The server notifies clients of the other players' actions (movement, shooting, ...)
- [x] **MUST** - The server is multithreaded and never blocks waiting for client messages - _network I/O runs on its own asio thread; the main thread is free for the game loop_
- [x] **MUST** - The game loop runs frame after frame on the server regardless of client activity - _`main.cpp` only has an empty `for(;;);` placeholder: measured at 100% of a CPU core, and an infinite loop without side effects is undefined behavior in C++20_
- [x] **MUST** - If a client crashes, the server keeps running - _verified: clients vanishing without `Disconnect` do not affect the server; nothing detects them though (see timeout below)_
- [ ] **MUST** - If a client crashes, the other clients **in the same game** are notified (requires a notion of game / room) - _no game / room concept yet_
- [ ] **MUST** - The server is robust and keeps running whatever goes wrong - _survived ~300 random packets, but accepts unlimited connections that never expire, and burns a full core_
- [x] **MUST** - If raw system sockets are used (instead of Asio), they are wrapped in proper abstractions - _Asio is used, wrapped in `UdpTransport`_
- [ ] Disconnection timeout handling (a lost `Disconnect` packet must not leave ghost clients) - _no timeout or heartbeat in `NetworkServer`; a connection is only removed on an explicit `Disconnect`_

---

## 3. Client

- [x] **MUST** - The client contains everything needed to display the game and handle player input - _window, rendering, input mapping (keyboard and gamepad), scenes_
- [ ] **MUST** - No authoritative gameplay logic in the client (death of an enemy or a player, item pickup, ... are decided by the server) - _movement, shooting and collisions all run locally today_
- [ ] **MAY** - Local prediction of the local player movement and missile movement, always overridden by the server
- [x] Graphics library is not a game engine and not too broad in scope (no UE, Unity, Godot, ...) - _custom Vulkan sprite renderer (`rtk_graphical`)_

---

## 4. Network protocol

- [x] **MUST** - The protocol is binary
- [x] **MUST** - All in-game communication (entities, movements, events) uses UDP
- [x] If a TCP channel is added, it is strongly justified in writing - _no TCP channel_
- [x] **MUST** - A malformed packet never crashes the client or the server - _server verified with truncated, oversized and ~300 random packets; the client has no network code yet, re-test once it does_
- [ ] **MUST** - A malformed packet never causes excessive memory consumption - _verified problem: 200 `Hello` packets from 200 sockets created 200 connections; the map is unbounded and nothing expires entries_
- [ ] **MUST** - A malformed packet never compromises server security (buffer overflows included)
- [x] **MUST** - The protocol is documented (an RFC is a good format) - _`src/protocole.md`_
- [ ] **SHOULD** - The documentation is enough to write a new client without reading the code - _`protocole.md` and the implementation disagree: it defines one `CommandType` enum (`Connect`, `Welcome` with nonce / network id / tick, ...), the lib uses a separate `Hello` / `Welcome` handshake and its own `MessageType` values; the packet envelope (`PacketHeader`, sequence / ack) is not documented_

---

## 5. Game engine architecture

- [x] **MUST** - Rendering, networking and game logic are visible, decoupled subsystems / layers - _`lib/GameEngine` (ECS systems, scenes), `lib/Network`, and the renderer behind `RenderConcept` and `RtkWindowAdapter`; one leak: `DebugCollisionSystem` in the engine takes a concrete `rtk::RenderWindow`_
- [x] **MUST** - The engine defines how an in-game object is represented - _entity (`size_t`) plus components (`Transform`, `SpriteData`, `AABBCollider`, `Animation`, `Velocity`, ...)_
- [x] **MUST** - The engine defines the coordinate system - _`Transform.position` in screen pixels, y pointing down (implicit, to be documented)_
- [x] **MUST** - The engine defines how systems (graphics, physics, network, ...) communicate - _through components and `CollisionBuffer`; the `busEvent` event bus is in `EngineContext` but nothing publishes or subscribes yet_
- [x] The rendering system only needs an entity appearance and position, not gameplay data such as damage or speed - _`RenderSystem` reads `Transform` and `SpriteData` only_
- [ ] Architecture pattern chosen and justified (ECS and Mediator are the recommended ones) - _ECS, event bus and scene manager are chosen; the justification is not written_

---

## 6. Software engineering

### Build and dependencies

- [x] **MUST** - A build system generator is used (CMake, Xmake, Meson, ...)
- [x] **MUST** - A package manager handles third-party dependencies (Conan, Vcpkg or CMake CPM) - _vcpkg manifest; the ECS and graphical libs are fetched as release archives with `FetchContent`; `factoryEntity.hpp` includes `nlohmann/json.hpp`, which is not in `vcpkg.json` (unused for now, will break when included)_
- [ ] **MUST** - The project is self-contained: it builds and runs without altering the system - _builds once the xcb, X11 and Wayland dev packages and `zip` / `unzip` are installed (undocumented); at run time `librtk_graphical.so` cannot find the vcpkg `libvulkan.so.1` unless the system has one, because the client RUNPATH does not apply to it_
- [x] **MUST** - No reliance on system-wide libraries or headers, except compilers, SDKs and low-level platform libraries (OpenGL, X11, ...)
- [x] **MUST** - Dependencies' source code is never copied into the repository - _`External/` is git-ignored_

### Platforms

- [ ] **MUST** - Client and server run on Linux - _both targets build; the server runs; the client was not seen running in the audit environment (missing `libvulkan.so.1` lookup, then no Vulkan driver). It also loads `menu.png` and `assets/bullet.png`, which are not in the repository. To confirm on a team machine_
- [ ] **SHOULD** - Client and server also run on Windows (MacOS and WSL do not count) - _CMake has `WIN32` branches for the graphical lib; nothing was built on Windows, and `BigEndian.hpp` uses GCC / Clang builtins only_
- [ ] **SHOULD** - Cross-play works between clients of different operating systems
- [ ] Cross-platform constraints (endianness, sockets, paths, ...) decided early - _wire endianness is fixed (big-endian, handled by `BigEndian`) and sockets go through Asio; the rest is not decided_

### Development workflow

- [x] **MUST** - Feature branches - _`core/link_ECS_and_GRAPHICAL_lib`, `server`, `LibNetwork`_
- [x] **MUST** - Merge requests / pull requests - _e.g. PR #19_
- [ ] **MUST** - Issues to track work - _could not be verified from the repository_
- [ ] **MUST** - Tags for important milestones - _no tags_
- [x] **MUST** - Clean commits (content and description) - _Conventional Commits checked by the `commit_lint` CI job; a few slipped through (`refractor(Input)` on `main`, `Feat(lib/network)` on `LibNetwork`)_
- [ ] Linters and formatters configured (`clang-tidy`, `clang-format`) - _`make format` exists, but there is no `.clang-format` and no `clang-tidy`_
- [ ] CI/CD building the project (optional) - _the CI only lints commit messages; the build job is commented out and refers to another project_
- [ ] CI uses caches or prebuilt images so dependencies are not fully fetched on every commit
- [x] No team member dedicated full time to CI/CD

---

## 7. Documentation

All documentation is written in **English** (**MUST**).

### README

- [ ] Project purpose
- [ ] Dependencies, requirements and supported platforms
- [ ] Build instructions
- [ ] Usage instructions
- [ ] License
- [ ] Authors / contacts
- [ ] Useful links and quick start

### Developer documentation

Must be higher level than Doxygen-style class and function descriptions.

- [ ] Architecture diagrams (layer / subsystem view)
- [ ] Overview of the main systems and how they map to the code
- [ ] Tutorials and how-tos
- [ ] Contribution guidelines
- [ ] Coding conventions

### Technical and comparative study

Justify each technology choice (language, graphics library, algorithms, networking technique, ...).

- [ ] Algorithms, data structures and design patterns: why these ones, and why any custom-designed algorithm was needed
- [ ] Storage: comparison of techniques (persistence, reliability, constraints)
- [ ] Security: main vulnerabilities of each technology and data integrity
- [ ] Security: how security is monitored in the long term

### Publication

- [ ] **MUST** - Documentation available online in a modern way (static site or wiki)
- [ ] Interlinked pages
- [ ] Quick-access outline
- [ ] Search bar
- [ ] No PDF or docx as the main documentation format

### Protocol documentation

- [ ] Every command and packet sent between client and server is described - _commands are described in `src/protocole.md`; the packet envelope and handshake are not_
- [ ] Written so that a third party could implement a new client

---

## 8. Accessibility

The project **and its documentation** must be accessible. Each category needs a concrete solution (it can also appear in the comparative study).

- [ ] Physical and motor disabilities: solution defined and implemented - _`InputMapping` allows several bindings per action (keyboard, gamepad button and axis, mouse), but there is no in-game remapping or documented option_
- [ ] Audio and visual disabilities: solution defined and implemented
- [ ] Mental and cognitive disabilities: solution defined and implemented
- [ ] Documentation accessibility handled

---

## 9. Before the defense

- [ ] Everything is merged on the evaluated branch before Sunday 11:42 PM
- [ ] Fresh clone builds and runs on Linux with the documented instructions only
- [ ] A full game with several clients was played and works
- [ ] A client crash was tested, and the server and other clients survived
- [ ] Malformed packets were tested, and neither the server nor the client crashed
- [ ] Documentation website is online and up to date
