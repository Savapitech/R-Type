# Technical and comparative study {#technical_study}

[TOC]

This page justifies each technical choice of the project. For every topic it
lists the options we considered, compares them on the criteria that matter for
a real time networked game, and states what we chose and why. Weak points of
the current choices are listed too, with the planned answer.

## 1. Criteria

| Criterion | Why it matters here |
| --------- | ------------------- |
| Latency and performance | The game simulates and draws at 60 frames per second on both sides. |
| Portability | The server and the client must run on Linux, macOS and Windows, on x86_64 and arm64. |
| Decoupling | Rendering, networking and gameplay must stay independent layers. |
| Robustness | Any byte from the network may be hostile. |
| Simplicity | A small team, six weeks, and code that new members can read. |
| Subject constraints | C++, a build system generator, a package manager, UDP, no full game engine. |

## 2. Programming language

| Option | Performance | Portability | Ecosystem for games | Team knowledge |
| ------ | ----------- | ----------- | ------------------- | -------------- |
| **C++20** | Native, no garbage collector | Every platform | Largest (Asio, Vulkan, SDL, SFML) | High |
| Rust | Native, memory safe | Every platform | Growing, fewer mature 2D libraries | Low |
| C# | JIT and garbage collector pauses | Good with .NET | Strong, but mostly tied to Unity | Medium |
| C | Native | Every platform | Large but low level | High |

**Choice: C++20.** It is required by the project, and it is also the best fit:
deterministic memory management with no collector pause, direct access to
Vulkan and to the operating system, and modern features that we use on purpose.
Concepts (rtk::Game, rtk::Scene, rtk::ecs::concepts::IsRenderWindow) give
interfaces checked at compile time without virtual calls, `std::variant` holds
the active scene, and `std::bit_cast` converts sprites and big-endian values
without undefined behavior. And this is the language that Epitech required us to use.

## 3. Graphics, window, input and audio library

| Option | Scope | Rendering path | Sprite batching | Portability | Notes |
| ------ | ----- | -------------- | --------------- | ----------- | ----- |
| **rtk graphical** | Window, sprites, input, sound | Vulkan | Yes, hardware instancing | Linux, Windows, macOS release builds | Sprite layout identical to the engine one |
| SFML 2 or 3 | Window, 2D, audio, network | OpenGL | Manual with vertex arrays | Linux, Windows, macOS | Very easy to use, one draw call per sprite by default |
| SDL3 | Window, input, audio, 2D renderer | Several backends | Basic | Every platform | Low level, C API |
| Raylib | Window, 2D and 3D, audio | OpenGL | Internal | Every platform | Broad scope, game oriented helpers |

**Choice: rtk graphical.** It does only what the subject allows a library to do:
open a window, draw sprites, read inputs and play sounds. It receives all the
sprites of a frame in a single call and renders them with Vulkan hardware
instancing and bindless textures, which keeps the cost per sprite low even with
many bullets and tiles on screen. Its sprite structure has the
same memory layout as rtk::ecs::concepts::SpriteData, so the client adapter
converts a whole frame with `std::bit_cast`, without copying field by field.

**Drawbacks.** It requires a Vulkan driver, and it has a much smaller community
than SFML or SDL. To limit the risk, the engine never includes it: the render
and input systems only depend on the rtk::ecs::concepts::IsRenderWindow concept
and the audio system on rtk::ecs::concepts::IsAudioManager. Switching to SFML
or SDL would only require a new adapter class.

## 4. Engine architecture

### 4.1 Object model

| Option | Decoupling | Data locality | Adding a feature | Drawbacks |
| ------ | ---------- | ------------- | ---------------- | --------- |
| Inheritance tree (`Entity`, `Ship`, `Enemy`) | Low, behavior and data mixed | Poor, objects scattered on the heap | New subclass, often a diamond | Rigid, "god" base classes |
| Component objects (Unity style `GameObject` with virtual components) | Medium | Poor, one allocation per component | New component class | Virtual call per component per frame |
| **Entity Component System** | High, data and behavior separated | Good, one dense array per component type | New component and system, nothing else changes | Logic spread across systems, needs a clear naming scheme |

**Choice: ECS**, as recommended by the subject. It gives exactly the decoupling
we need: the render system reads only component::Transform and
rtk::ecs::concepts::SpriteData and knows nothing about speed, health or damage.
The same components and systems run on the server and on the client, and the
server simply does not register the rendering ones.

### 4.2 Communication between systems

| Option | Coupling | Ordering | Notes |
| ------ | -------- | -------- | ----- |
| Direct calls between systems | Strong | Explicit | Systems need references to each other |
| Shared components only | Weak | Implicit, through stages | Good for continuous data, awkward for one shot events |
| **Mediator, as a typed event bus** | Weak | Immediate | Good for one shot events such as a collision or a spawn |

**Choice: components for continuous state, plus an event bus for events.**
busEvent is a mediator: publishers and subscribers only know the event type. It
is how rtk::systems::ColliderSystem reports collisions without knowing the
gameplay rules, and how rtk::systems::EnemySpawnerSystem tells the server code
that an enemy must be announced to the clients.

### 4.3 Scene management

| Option | Allocation | Checked at | Notes |
| ------ | ---------- | ---------- | ----- |
| Abstract `Scene` class and `std::unique_ptr` | One per switch | Run time | Classic, needs virtual calls |
| Stack of scenes | One per push | Run time | Useful for overlays such as pause |
| **`std::variant` of scene types** | None | Compile time | The set of scenes is fixed per program |

**Choice: `std::variant`.** rtk::SceneManager checks at compile time that every
scene satisfies rtk::Scene and can be built from the shared dependency object.
Only the active scene is alive and switching does not allocate.

## 5. Algorithms and data structures

| Problem | Options considered | Chosen | Why |
| ------- | ------------------ | ------ | --- |
| Component storage | Map per entity, array of `std::optional`, archetype tables (EnTT, flecs), **sparse set** | Sparse set (rtk ECS) | O(1) insert, remove and lookup, dense arrays for iteration, simple to understand and debug. Archetype tables iterate faster on many components but cost more on every add and remove. |
| Entity destruction | Immediate, **deferred** | Deferred kill flushed after each stage | Systems can kill entities while iterating over a view. |
| Broad phase collision | Brute force O(n^2), quadtree, sweep and prune, **uniform spatial hash grid** | Grid of 64 pixel cells, rebuilt every frame | Sprites are 32 to 64 pixels and spread fairly evenly on a horizontal strip, which is the best case for a uniform grid. Rebuilding is linear, and the pair set guarantees each pair is reported once. A quadtree adapts to uneven density we do not have, and adds rebuild cost. |
| Narrow phase collision | Circles, polygons (SAT), **axis aligned boxes** | AABB overlap test | Exact enough for 2D sprites, four comparisons per pair. |
| Collision filtering | Pairwise rule table, **layer and mask bits** | 32-bit layer and mask | One AND per side, configured from JSON by name. |
| Blueprint identification on the wire | Name string, central enum, **16-bit hash of the name** | FNV-1a folded to 16 bits | Two bytes per entity, no shared table to keep in sync: both sides hash the same names. The current blueprint names were checked and do not collide. |
| Duplicate and loss detection | Per packet ack, TCP, **sequence plus 32-bit ack bitfield** | Sliding window in PacketTracker | Constant time and memory, and one header acknowledges 33 packets, so acks survive packet loss. |
| Positions on the wire | `float` (4 bytes), **16-bit fixed point** | 0.25 pixel steps over -8192 to 8191.75 | Halves the size of each coordinate with no visible loss. |
| Rotations on the wire | `float` radians, **16-bit fraction of a turn** | 65536 steps per turn | 0.0055 degree resolution, wraps naturally. |
| Thread hand off | Lock free queue, **mutex protected `std::queue`** | One mutex per queue | Only a few dozen messages per frame, so contention is negligible and the code stays obviously correct. |
| Event dispatch | `std::any` list, virtual visitor, **map from `std::type_index` to handlers** | busEvent | Constant time lookup by type, any struct can be an event. |

**Custom algorithms.** We wrote four algorithms ourselves instead of taking a
library, each for a precise reason:

- The **spatial hash grid** avoids pulling a full physics engine for what is
  only box overlap on a 2D strip.
- The **packet tracker** is part of our protocol: its window size and its
  classification rules are protocol rules.
- The **position and rotation quantization** is tied to the screen size and the
  precision the game needs.
- The **archetype hash** lets data defined in JSON be identified on the wire
  without a hand maintained id table.

## 6. Networking

### 6.1 Transport

| Option | Latency under loss | Ordering | Message boundaries | Notes |
| ------ | ------------------ | -------- | ------------------ | ----- |
| TCP | High: one lost segment blocks every following byte (head of line blocking) | Guaranteed | None, it is a byte stream | Retransmits stale positions that are useless by the time they arrive |
| **UDP** | Low: each datagram stands alone | None | Kept, one datagram is one packet | Loss, duplication and reordering must be handled by the application |
| UDP plus a TCP side channel | Low for game data | Mixed | Mixed | Two connections to manage and secure |

**Choice: UDP only.** It is required by the subject for all game traffic, and
it is the right tool: the world state is sent again 20 times per second, so a
lost snapshot is simply replaced by the next one. Delivering a stale snapshot
late, as TCP would, only adds lag. The few messages that matter more
(creations) are repaired with `QueryEntity`. We do not need any message that
would justify a TCP channel. Same as for C++, forced by Epitech to use it.

### 6.2 Network library

| Option | Portability | Async model | Dependencies | Notes |
| ------ | ----------- | ----------- | ------------ | ----- |
| Raw BSD sockets and Winsock | Two code paths | Manual (`poll`, `select`) | None | Must be wrapped in our own abstractions |
| **Asio, standalone** | Linux, Windows, macOS | Proactor, completion handlers on our thread | Header only, no Boost | Explicitly allowed by the subject |
| Boost.Asio | Same as Asio | Same | All of Boost | Heavier for the same API |
| ENet, GameNetworkingSockets | Good | Own event loop | Library and its protocol | Brings its own protocol, so we would not design ours |

**Choice: standalone Asio.** One code base runs on Linux, macOS and Windows, the
asynchronous handlers run on a thread we control, and it adds no other
dependency. Libraries such as ENet would solve reliability for us, but the
project asks us to design the protocol, and we want full control over
validation of every byte.

### 6.3 Serialization

| Option | Size | Endianness safe | Padding safe | Cost | Attack surface |
| ------ | ---- | --------------- | ------------ | ---- | -------------- |
| `memcpy` of native structs | Small | No | No | None | Low |
| JSON or another text format | Large | Yes | Yes | Parsing | Large parser |
| Protocol Buffers, FlatBuffers | Small | Yes | Yes | Code generation, extra dependency | Medium |
| **Fixed layout structs of rtype::protocol::BigEndian fields** | Small | Yes | Yes | None | Low |

**Choice: fixed layout big-endian structs.** rtype::protocol::BigEndian stores a
value as a byte array, so its alignment is 1 and a struct made only of such
fields has no padding, whatever the field order. A `static_assert` on every
payload size enforces it. The payload can then be copied byte for byte into the
packet, on any host. Versioning is manual, which is acceptable for a protocol
of eight messages.

**Measured effect of quantization.** A snapshot entry is 11 bytes. With a
32-bit `float` per coordinate and for the rotation it would be 17 bytes, so the
encoding saves 35 percent. One `Snapshot` message carries up to 47 entities in
a 548 byte datagram.

## 7. Build and dependencies

| Option | Integration with CMake | Binary cache | Notes |
| ------ | ---------------------- | ------------ | ----- |
| Conan | Through generators | Yes | Python tool, separate profile system |
| **vcpkg in manifest mode** | Native toolchain file | Yes | Dependencies listed in one JSON manifest |
| CMake CPM | Native | No, sources rebuilt | Simple but slower clean builds |

**Choice: CMake and vcpkg.** The manifest lists every third party library and
vcpkg builds them inside the project tree, so nothing is installed on the
system. The two rtk libraries are fetched as prebuilt release archives with
CMake `FetchContent`, which avoids rebuilding them.

## 8. Storage

### 8.1 What the project stores

| Data | Where | Written at run time | Lifetime |
| ---- | ----- | ------------------- | -------- |
| Sprites and sounds | PNG and WAV files in `assets` | No | Versioned in Git |
| Entity blueprints | JSON files in `assets/descriptionEntity` | No | Versioned in Git |
| World state | Server memory, in the ECS registry | Yes | One round |
| Connections | Server memory | Yes | One session |

There is no player account, progression or score to keep between runs yet.

### 8.2 Comparison

| Technique | Persistence | Reliability | Constraints | Fit |
| --------- | ----------- | ----------- | ----------- | --- |
| **Plain files (JSON, PNG, WAV)** | Permanent, read only | Versioned and reviewed in Git, parse errors stop the program at startup | Human readable, easy to diff, slower to parse than binary | Assets and blueprints |
| Binary asset pack | Permanent, read only | Needs its own tooling to verify | Fast loading, not diffable | Large games, not needed at our size |
| **In memory only** | None | Lost on crash or restart | Fastest, no I/O | Live world state |
| SQLite | Permanent, single file | ACID transactions, survives crashes | No server, one writer at a time | Scores and settings, planned |
| Client and server database (PostgreSQL) | Permanent | Replication, backups | A service to run and secure | Accounts across many servers, beyond our scope |

**Choice.** Assets and blueprints are plain files under version control:
designers edit them without recompiling, every change is reviewed, and a broken
file makes the program fail at startup instead of in the middle of a game. The
live world stays in memory: a round lasts a few minutes and has no value once
it ends, so persisting it would only add I/O on the game loop. If a scoreboard
is added, SQLite is the planned choice because it gives crash safe transactions
in a single file without running a database server.

## 9. Security

### 9.1 Main vulnerabilities by technology

| Technology | Typical vulnerability | Mitigation in the project |
| ---------- | --------------------- | ------------------------- |
| C++ | Buffer overflow, out of bounds read, use after free | All network reads go through PacketReader, which checks the remaining size before every read. Payloads are copied into fixed size structs, never cast in place. Owned memory uses containers and smart pointers. |
| Custom binary protocol | Lying length fields, truncated messages, huge counts that trigger large allocations | Declared sizes are compared to the received bytes, packets with trailing bytes are dropped, fixed payloads must match their exact size, and `entityCount` is capped at 47 before any allocation. |
| UDP | Source address spoofing, replay, flooding | Connected packets must carry the connection id bound to their source address. Duplicate and old packets are dropped. Old inputs are rejected by input sequence. Invalid datagrams get no answer, so the server cannot be used to amplify traffic. Datagrams larger than 548 bytes are dropped by the transport. |
| Asio asynchronous I/O | Handler running after the object it uses was destroyed | Send buffers are kept alive by shared pointers captured in the handler, and the network thread is joined before the server object is destroyed. |
| Multithreading | Data races | Only the network thread touches the socket and the connection table. Data crosses threads through mutex protected queues, and flags are atomic. |
| JSON blueprints | Malformed file, unexpected types | Files ship with the game and are read only. Parse errors throw at startup, and missing fields fall back to defaults. |
| Third party dependencies | Compromised or changed upstream release | Dependencies come from vcpkg and from release archives, never copied into the repository. |

### 9.2 Data integrity

- Corruption in transit is caught by the UDP checksum, and then by the
  structural checks above: a corrupted datagram almost always fails a length or
  range check and is dropped.
- The server is the only source of truth. Clients send intentions, so a
  modified client cannot move another ship, create entities or declare a kill.
- Inputs are applied in order thanks to the input sequence, and a replayed or
  delayed input is ignored.

### 9.3 Known weaknesses

| Weakness | Risk | Planned answer |
| -------- | ---- | -------------- |
| Sequential connection ids | An attacker able to spoof the address of a player can guess its id | Random 32-bit connection ids, then a nonce exchanged in the handshake. |
| No encryption or authentication | Traffic can be read and forged on the path | Acceptable for a LAN game. A keyed message authentication code per packet is the next step for Internet play. |
| rtk libraries fetched from the latest release | An upstream change can alter the build without any change in our repository | Pin release versions and verify archive hashes in CMake. |

### 9.4 Long term security monitoring

| Measure | Status | What it catches |
| ------- | ------ | --------------- |
| Pull request review on every change | In place | Logic errors and unsafe code before merge |
| Conventional commit check in CI | In place | Keeps history readable for audits |
| NetworkMetrics counters (bytes, packets, duplicates, out of order) | In place | Abnormal traffic patterns |
| Server log of connections, disconnections and framing errors | In place | Suspicious clients, visible in the server console |
| AddressSanitizer and UndefinedBehaviorSanitizer build in CI | Planned | Memory errors during tests |
| Fuzzing of MessageParser, rtype::protocol::readSnapshot() and HandshakeHeader with libFuzzer | Planned | Parser crashes on unexpected input |
| clang-tidy with the `bugprone` and `cert` checks in CI | Planned | Common unsafe patterns |
| Pinned dependency versions with automatic update alerts | Planned | Known vulnerabilities in third party code |
| Per address counters of dropped datagrams, with a warning threshold | Planned | Floods and scanning attempts |

## 10. Summary

| Topic | Choice |
| ----- | ------ |
| Language | C++20 |
| Rendering, input, audio | rtk graphical behind engine concepts |
| Architecture | ECS, event bus as mediator, `std::variant` scenes |
| Component storage | Sparse sets |
| Collisions | Uniform spatial hash grid and AABB |
| Transport | UDP only |
| Network library | Standalone Asio |
| Serialization | Fixed layout big-endian structs, quantized positions |
| Reliability | Sequence and ack bitfield, full snapshots, query based repair |
| Build | CMake and vcpkg |
| Storage | Versioned plain files for assets, memory for live state |
