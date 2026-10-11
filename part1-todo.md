# R-Type - Part 1 checklist

Everything the subject requires for the first delivery (software architecture and first game prototype).

**Deadline:** the last commit before **Sunday 11:42 PM** is the one evaluated, in a defense in front of the local team.

Legend: **MUST** = mandatory, **SHOULD** = strongly expected, **MAY** = optional.

---

## 1. Gameplay

- [ ] **MUST** - The game is playable end to end, over the network, with several clients on one server
- [ ] **MUST** - The client displays a slow horizontal scrolling starfield (stars, planets, ...)
- [ ] **MUST** - Starfield scrolling, entity behavior and overall time flow are driven by timers, never by CPU speed
- [ ] **MUST** - Players move with the arrow keys
- [ ] **MUST** - Bydos monsters exist in the game
- [ ] **MUST** - Missiles exist (players and enemies can shoot)
- [ ] **MUST** - Monsters spawn randomly on the right side of the screen
- [ ] **MUST** - The four players of a game are distinctly identifiable (color, sprite, ...)
- [ ] Basic sound design (music and/or sound effects)
- [ ] Use the sprite set provided with the subject (or other freely available R-Type sprites)

Optional elements of the official screen (not mandatory for part 1):

- [ ] **MAY** - Monster that spawns a powerup on death
- [ ] **MAY** - Stage obstacles
- [ ] **MAY** - Destroyable tiles

---

## 2. Server

- [ ] **MUST** - The server is authoritative: it runs all the game logic and clients comply with its decisions
- [ ] **MUST** - The server notifies every client when a monster spawns
- [ ] **MUST** - The server notifies every client when a monster moves
- [ ] **MUST** - The server notifies every client when a monster is destroyed
- [ ] **MUST** - The server notifies every client when a monster fires
- [ ] **MUST** - The server notifies every client when a player is killed
- [ ] **MUST** - The server notifies clients of the other players' actions (movement, shooting, ...)
- [ ] **MUST** - The server is multithreaded and never blocks waiting for client messages
- [ ] **MUST** - The game loop runs frame after frame on the server regardless of client activity
- [ ] **MUST** - If a client crashes, the server keeps running
- [ ] **MUST** - If a client crashes, the other clients **in the same game** are notified (requires a notion of game / room)
- [ ] **MUST** - The server is robust and keeps running whatever goes wrong
- [ ] **MUST** - If raw system sockets are used (instead of Asio), they are wrapped in proper abstractions
- [ ] Disconnection timeout handling (a lost `Disconnect` packet must not leave ghost clients)

---

## 3. Client

- [ ] **MUST** - The client contains everything needed to display the game and handle player input
- [ ] **MUST** - No authoritative gameplay logic in the client (death of an enemy or a player, item pickup, ... are decided by the server)
- [ ] **MAY** - Local prediction of the local player movement and missile movement, always overridden by the server
- [ ] Graphics library is not a game engine and not too broad in scope (no UE, Unity, Godot, ...)

---

## 4. Network protocol

- [ ] **MUST** - The protocol is binary
- [ ] **MUST** - All in-game communication (entities, movements, events) uses UDP
- [ ] If a TCP channel is added, it is strongly justified in writing
- [ ] **MUST** - A malformed packet never crashes the client or the server
- [ ] **MUST** - A malformed packet never causes excessive memory consumption
- [ ] **MUST** - A malformed packet never compromises server security (buffer overflows included)
- [ ] **MUST** - The protocol is documented (an RFC is a good format)
- [ ] **SHOULD** - The documentation is enough to write a new client without reading the code

---

## 5. Game engine architecture

- [ ] **MUST** - Rendering, networking and game logic are visible, decoupled subsystems / layers
- [ ] **MUST** - The engine defines how an in-game object is represented
- [ ] **MUST** - The engine defines the coordinate system
- [ ] **MUST** - The engine defines how systems (graphics, physics, network, ...) communicate
- [ ] The rendering system only needs an entity appearance and position, not gameplay data such as damage or speed
- [ ] Architecture pattern chosen and justified (ECS and Mediator are the recommended ones)

---

## 6. Software engineering

### Build and dependencies

- [ ] **MUST** - A build system generator is used (CMake, Xmake, Meson, ...)
- [ ] **MUST** - A package manager handles third-party dependencies (Conan, Vcpkg or CMake CPM)
- [ ] **MUST** - The project is self-contained: it builds and runs without altering the system
- [ ] **MUST** - No reliance on system-wide libraries or headers, except compilers, SDKs and low-level platform libraries (OpenGL, X11, ...)
- [ ] **MUST** - Dependencies' source code is never copied into the repository

### Platforms

- [ ] **MUST** - Client and server run on Linux
- [ ] **SHOULD** - Client and server also run on Windows (MacOS and WSL do not count)
- [ ] **SHOULD** - Cross-play works between clients of different operating systems
- [ ] Cross-platform constraints (endianness, sockets, paths, ...) decided early

### Development workflow

- [ ] **MUST** - Feature branches
- [ ] **MUST** - Merge requests / pull requests
- [ ] **MUST** - Issues to track work
- [ ] **MUST** - Tags for important milestones
- [ ] **MUST** - Clean commits (content and description)
- [ ] Linters and formatters configured (`clang-tidy`, `clang-format`)
- [ ] CI/CD building the project (optional)
- [ ] CI uses caches or prebuilt images so dependencies are not fully fetched on every commit
- [ ] No team member dedicated full time to CI/CD

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

- [ ] Every command and packet sent between client and server is described
- [ ] Written so that a third party could implement a new client

---

## 8. Accessibility

The project **and its documentation** must be accessible. Each category needs a concrete solution (it can also appear in the comparative study).

- [ ] Physical and motor disabilities: solution defined and implemented
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
