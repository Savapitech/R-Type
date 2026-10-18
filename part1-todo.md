# R-Type - Part 1 checklist

Legend: **MUST** = mandatory, **SHOULD** = strongly expected, **MAY** = optional.

---

## 1. Gameplay

- [x] **MUST** - The game is playable end to end, over the network, with several clients on one server
- [x] **MUST** - The client displays a slow horizontal scrolling starfield (stars, planets, ...)
- [x] **MUST** - Starfield scrolling, entity behavior and overall time flow are driven by timers, never by CPU speed
- [x] **MUST** - Players move with the arrow keys
- [x] **MUST** - Bydos monsters exist in the game
- [x] **MUST** - Missiles exist (players and enemies can shoot)
- [x] **MUST** - Monsters spawn randomly on the right side of the screen
- [x] **MUST** - The four players of a game are distinctly identifiable (color, sprite, ...)
- [x] Basic sound design (music and/or sound effects)
- [x] Use the sprite set provided with the subject (or other freely available R-Type sprites)

Optional elements of the official screen (not mandatory for part 1):

- [ ] **MAY** - Monster that spawns a powerup on death
- [x] **MAY** - Stage obstacles
- [ ] **MAY** - Destroyable tiles

---

## 2. Server

- [x] **MUST** - The server is authoritative: it runs all the game logic and clients comply with its decisions
- [x] **MUST** - The server notifies every client when a monster spawns
- [x] **MUST** - The server notifies every client when a monster moves
- [x] **MUST** - The server notifies every client when a monster is destroyed
- [x] **MUST** - The server notifies every client when a monster fires
- [x] **MUST** - The server notifies every client when a player is killed
- [x] **MUST** - The server notifies clients of the other players' actions (movement, shooting, ...)
- [x] **MUST** - The server is multithreaded and never blocks waiting for client messages
- [x] **MUST** - The game loop runs frame after frame on the server regardless of client activity
- [x] **MUST** - If a client crashes, the server keeps running
- [x] **MUST** - If a client crashes, the other clients **in the same game** are notified (requires a notion of game / room)
- [x] **MUST** - The server is robust and keeps running whatever goes wrong
- [x] **MUST** - If raw system sockets are used (instead of Asio), they are wrapped in proper abstractions
- [x] Disconnection timeout handling (a lost `Disconnect` packet must not leave ghost clients)

---

## 3. Client

- [x] **MUST** - The client contains everything needed to display the game and handle player input
- [x] **MUST** - No authoritative gameplay logic in the client (death of an enemy or a player, item pickup, ... are decided by the server)
- [x] **MAY** - Local prediction of the local player movement and missile movement, always overridden by the server
- [x] Graphics library is not a game engine and not too broad in scope (no UE, Unity, Godot, ...)

---

## 4. Network protocol

- [x] **MUST** - The protocol is binary
- [x] **MUST** - All in-game communication (entities, movements, events) uses UDP
- [x] If a TCP channel is added, it is strongly justified in writing
- [x] **MUST** - A malformed packet never crashes the client or the server
- [ ] **MUST** - A malformed packet never causes excessive memory consumption
- [ ] **MUST** - A malformed packet never compromises server security (buffer overflows included)
- [x] **MUST** - The protocol is documented (an RFC is a good format)
- [x] **SHOULD** - The documentation is enough to write a new client without reading the code

---

## 5. Game engine architecture

- [x] **MUST** - Rendering, networking and game logic are visible, decoupled subsystems / layers
- [x] **MUST** - The engine defines how an in-game object is represented
- [x] **MUST** - The engine defines the coordinate system
- [x] **MUST** - The engine defines how systems (graphics, physics, network, ...) communicate
- [x] The rendering system only needs an entity appearance and position, not gameplay data such as damage or speed
- [x] Architecture pattern chosen and justified (ECS and Mediator are the recommended ones)

---

## 6. Software engineering

### Build and dependencies

- [x] **MUST** - A build system generator is used (CMake, Xmake, Meson, ...)
- [x] **MUST** - A package manager handles third-party dependencies (Conan, Vcpkg or CMake CPM)
- [x] **MUST** - The project is self-contained: it builds and runs without altering the system
- [x] **MUST** - No reliance on system-wide libraries or headers, except compilers, SDKs and low-level platform libraries (OpenGL, X11, ...)
- [x] **MUST** - Dependencies' source code is never copied into the repository

### Platforms

- [x] **MUST** - Client and server run on Linux
- [x] **SHOULD** - Client and server also run on Windows (MacOS and WSL do not count)
- [x] **SHOULD** - Cross-play works between clients of different operating systems
- [x] Cross-platform constraints (endianness, sockets, paths, ...) decided early

### Development workflow

- [x] **MUST** - Feature branches
- [x] **MUST** - Merge requests / pull requests
- [x] **MUST** - Issues to track work
- [x] **MUST** - Tags for important milestones
- [x] **MUST** - Clean commits (content and description)
- [x] Linters and formatters configured (`clang-tidy`, `clang-format`)
- [x] CI/CD building the project (optional)
- [x] CI uses caches or prebuilt images so dependencies are not fully fetched on every commit
- [x] No team member dedicated full time to CI/CD

---

## 7. Documentation

All documentation is written in **English** (**MUST**).

### README

- [x] Project purpose
- [x] Dependencies, requirements and supported platforms
- [x] Build instructions
- [x] Usage instructions
- [x] License
- [x] Authors / contacts
- [x] Useful links and quick start

### Developer documentation

Must be higher level than Doxygen-style class and function descriptions.

- [x] Architecture diagrams (layer / subsystem view)
- [x] Overview of the main systems and how they map to the code
- [x] Tutorials and how-tos
- [x] Contribution guidelines
- [x] Coding conventions

### Technical and comparative study

Justify each technology choice (language, graphics library, algorithms, networking technique, ...).

- [x] Algorithms, data structures and design patterns: why these ones, and why any custom-designed algorithm was needed
- [x] Storage: comparison of techniques (persistence, reliability, constraints)
- [x] Security: main vulnerabilities of each technology and data integrity
- [x] Security: how security is monitored in the long term

### Publication

- [x] **MUST** - Documentation available online in a modern way (static site or wiki)
- [x] Interlinked pages
- [x] Quick-access outline
- [x] Search bar
- [x] No PDF or docx as the main documentation format

### Protocol documentation

- [x] Every command and packet sent between client and server is described
- [x] Written so that a third party could implement a new client

---

## 8. Accessibility

The project **and its documentation** must be accessible. Each category needs a concrete solution (it can also appear in the comparative study).

- [x] Physical and motor disabilities: solution defined and implemented
- [x] Audio and visual disabilities: solution defined and implemented
- [ ] Mental and cognitive disabilities: solution defined and implemented
- [x] Documentation accessibility handled

---

## 9. Before the defense

- [ ] Everything is merged on the evaluated branch before Sunday 11:42 PM
- [ ] Fresh clone builds and runs on Linux with the documented instructions only
- [ ] A full game with several clients was played and works
- [ ] A client crash was tested, and the server and other clients survived
- [ ] Malformed packets were tested, and neither the server nor the client crashed
- [ ] Documentation website is online and up to date
