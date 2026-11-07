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
encoding saves 35 percent. One `Snapshot` message carries up to 46 entities in
a 548 byte datagram.

### 6.4 Compression

Every packet pays 47 bytes of headers (IPv4, UDP and our PacketHeader) and then
carries a few hundred bytes of messages, mostly snapshots of quantized
positions. Compressing the body is the last lever once messages are batched
(see @ref networking_batching). The constraints are particular:

- **Tiny, independent inputs.** A body is between a few bytes and 529 bytes.
  UDP loses and reorders packets, so each one must be decodable alone: a
  streaming compressor that keeps history from one packet to the next cannot be
  used.
- **No history, so the context must be shipped with the program.** The only way
  to give a compressor something to match against on a 100 byte input is a
  dictionary known by both sides.
- **Fixed overhead matters.** A container that adds 20 bytes to a 100 byte
  packet is a loss before any compression happens.
- **Speed.** The network thread compresses every outgoing packet and
  decompresses every incoming one, and a server with 32 clients sends about 900
  packets per second (28 packets per second per client were measured).
- **Untrusted input.** The decoder reads data coming from the network and its
  output must be bounded.

**Method.** We captured the body of every packet sent by the real server
(`RTYPE_CAPTURE_FILE`) during three sessions of 8 bots playing 25 seconds in two
rooms: 16 992 packets, 5.0 MB, 296 bytes on average. The first 80 percent of the
corpus (in time order) trains the dictionary, the last 20 percent (3 399
packets) is the test set. A random split would have been wrong: consecutive
snapshots are nearly identical, so neighbours of a test packet in the training
set flatter the dictionary. The tool is `tools/compression_bench`, the dictionary
is 4 KiB, and every result was checked by a decompression round trip.

| Algorithm | Output / input | Packets not smaller | Compress | Decompress |
| --------- | -------------: | ------------------: | -------: | ---------: |
| LZMA in an xz container | 76.0 % | 59.0 % | 546 µs | 7.1 µs |
| LZMA raw (lzma1, no header) | 56.7 % | 28.1 % | 23.6 µs | 2.8 µs |
| LZMA raw with a preset dictionary | **52.2 %** | 21.5 % | 94.5 µs | 3.3 µs |
| Brotli quality 1 | 70.6 % | 59.0 % | 3.7 µs | 1.2 µs |
| Brotli quality 5 | 58.7 % | 30.5 % | 11.3 µs | 1.6 µs |
| Brotli quality 11 | 62.2 % | 44.8 % | 474 µs | 4.3 µs |
| deflate 9 (raw) | 61.2 % | 19.7 % | 7.8 µs | 1.2 µs |
| LZ4 | 71.5 % | 21.4 % | 0.3 µs | 0.1 µs |
| zstd level 1, no dictionary | 67.1 % | 34.8 % | 3.6 µs | 1.2 µs |
| zstd level 19, no dictionary | 62.1 % | 42.1 % | 20.2 µs | 1.8 µs |
| zstd level 19 with dictionary, standard frame | 59.2 % | 30.8 % | 18.4 µs | 0.8 µs |
| **zstd level 1 with dictionary, minimal frame** | 60.0 % | 26.3 % | **1.0 µs** | **0.5 µs** |

"Packets not smaller" counts the packets for which the output is not smaller
than the input; those are sent uncompressed, so compression never makes a packet
bigger.

**What the numbers say.**

1. *Containers cost more than they save on small inputs.* The xz container is
   19 points worse than raw LZMA, because of its headers and footers. For the
   same reason we remove everything optional from the zstd frame (magic number,
   checksum, content size, dictionary id), which is worth 3 points.
2. *A dictionary is worth about 4 points.* Without history, a general purpose
   compressor starts every packet blind. zstd goes from 67.1 % to 63.3 % with the
   standard frame, LZMA from 56.7 % to 52.2 %.
3. *Nothing compresses these packets below about half.* Positions are
   quantized to 16 bits and probably carry little redundancy in their low bits
   (we did not analyse it), and the algorithms that do best, with a dictionary or
   a strong entropy coder, all end between 52 % and 62 %. The choice is therefore about cost, not about a miracle ratio.

**Choice: zstd level 1 with a trained dictionary and a minimal frame.**

- LZMA with a preset dictionary is the smallest (52.2 %), 8 points better than
  our choice, but it costs 94 µs per packet against 1 µs. At 900 packets per
  second that is 85 ms of every second on the network thread, which also reads,
  writes and tracks every connection, against about 1 ms. Eight points of a body
  that is already 40 percent smaller is not worth almost a tenth of the thread.
  This is the closest call of the study: with few clients, or on a dedicated
  thread, LZMA would be a valid choice.
- Brotli quality 5 (58.7 %) beats zstd without dictionary and is 1.3 points
  from our choice, but it is 11 times slower (11.3 µs) and cannot be given a
  dictionary of our own in the standard API, while its built-in one is made of
  web text. Quality 11 costs 474 µs and is worse on small inputs.
- LZ4 is the fastest, but only reaches 71.5 %.
- deflate (61.2 %) is 8 times slower than zstd for a worse ratio. We did not
  measure it with a preset dictionary, which zlib supports.
- Higher zstd levels bring little: level 19 is 18 times slower for 0.8 points
  with a dictionary. Level 1 is also marginally better than level 3 on this
  corpus (60.0 % against 60.5 %).
- zstd is widely deployed, maintained, available in vcpkg, and its dictionary
  support was designed for exactly this case: small messages of a known format.

**Where it sits.** Messages are batched first, then the body of each packet is
compressed once (a resend sends the same bytes again), then the PacketHeader is
written in clear, because the receiver needs `connectionId`, the sequence and
the acks before it decides to decompress. A bit of the `priority` byte tells the
receiver that the body is compressed. See @ref networking_compression.

**Measured effect.** On the same 8 bot test, the average datagram sent by the
server went from 314 to 183 bytes (minus 42 percent) once compression was
enabled, with no loss and no retransmission. Packets sent by the clients stay at
31 bytes: they carry one 13 byte `Input` message per frame, which is too small
to compress, and the 19 byte header is the real cost there.

**Limits and follow-ups.**

- The corpus comes from bots with random inputs on a debug build. The
  dictionary should be trained again on captures of human games before a
  release, with `compression_bench`, by replacing
  `lib/Network/src/protocol/zstd_dict.bin`. The dictionary is part
  of the protocol: changing it requires a new protocol version.
- Compression saves bytes, not packets, and the 47 bytes of headers remain.
- A general purpose compressor is the wrong tool for the snapshot itself.
  Sending only what changed since the last acknowledged snapshot (delta
  compression) would remove more than any algorithm above, and is the next step
  if bandwidth becomes a problem.

### 6.5 Hiding the latency

With an authoritative server, the ship of the player used to move only when a
snapshot came back: the delay between a key press and the movement was the round
trip time plus about 35 ms (135 ms at 100 ms RTT, 235 ms at 200 ms, 535 ms at 500
ms, measured with `netem`, see @ref network_benchmark). The subject lists the
usual techniques; this is how they compare for a shoot'em up whose enemies and
missiles are simulated by the server.

| Technique | What it fixes | Cost | Our decision |
| --------- | ------------- | ---- | ------------ |
| Nothing (what we had) | Nothing | None | Ship reacts one round trip time late. Rejected. |
| **Client side prediction with server reconciliation** | The delay of the local ship, which is the one the player controls and feels | The client simulates the movement of its ship, the protocol must tell which input the server has applied, hits and power ups are corrected afterwards | **Done.** The movement is one small function shared by client and server, and the position error is small (see below). |
| Entity interpolation | The 20 Hz steps of the other entities and a lost snapshot | The other entities are shown about 100 ms in the past, a buffer of snapshots | Not done yet. The enemies already move in straight lines and the players fly slowly, so the gain is mostly visual smoothness. It is the next step. |
| Server side lag compensation | A shot that hits what the shooter saw, for hit scan weapons | The server keeps a history of positions and rewinds it for each shot | Not needed: missiles are entities simulated by the server, they hit what the server has. Missiles leave from the server ship, which is behind the ship drawn by the client by half a round trip time (15 pixels at 100 ms). Accepted. |
| Rollback netcode | Everything, by replaying the whole game after each correction | Both sides must simulate the same game deterministically, with a snapshot of the whole state per frame | Too heavy: the server uses variable time steps, random spawns and a Lua scripting layer. Rejected. |
| Input delay | Makes every player equally late | Adds latency for everyone | Opposite of what we want. Rejected. |

**Why prediction first.** The player controls only the ship, and the ship is the
only thing whose delay the player feels at each key press. The enemies and
missiles are not controlled, so seeing them half a round trip time late is
barely noticed in a game this slow. Predicting only the ship needs no change in
how the server simulates the world.

**How it works.** The client moves its ship at once with
protocol::shipVelocity(), the function the server calls too. Each snapshot
carries the network id of the ship of the receiver and the client tick that the
server position accounts for. The client takes the server position, replays the
movements it made since that tick, and gets where the ship should be now. It then
moves toward that point, half of the gap at each snapshot, and jumps if the gap is
above 96 pixels. The server computes the tick from the current run of actions:
`first client tick of the run + server ticks elapsed since`. See @ref protocol,
section 10.1.

**Measured effect** (@ref network_benchmark). The delay seen by the player went
from the round trip time plus 35 ms to 0 ms in the same frame, at every latency up
to 500 ms and with up to 50 percent loss. The gap between the predicted ship and
the server position has a median below 1 pixel, a 95th percentile of at most 10
pixels, and the prediction had to jump only twice in 15 213 comparisons, both on a
5 KB/s link.

**Limits.** The server position is only trusted through the acknowledged tick,
which assumes that the client and the server tick at the same rate. We measured
with bots that tick at exactly 60 per second: a real client has frames of different
durations, so the gap will be larger than measured. Other players are still late
and in steps.

### 6.6 Adapting to the link

The subject asks to test a very low bandwidth, 5 KB/s. Each client used to receive
7.7 KB/s of snapshots and creations and to send 1.9 KB/s of inputs (12.4 KB/s on
the wire with the IP and UDP headers). On a link that cannot carry this, a queue
grows in front of it, and all the packets arrive late: a 10 KB/s link added 775
ms, a 5 KB/s link made the game unplayable.

**Sending less upstream.** An input was sent 60 times per second, 31 bytes each,
19 of which are the packet header, while the server keeps the last actions it
received. It is now sent when the keys change, twice more against packet loss, and
every 50 ms otherwise: 21 packets per second instead of 61, 0.66 KB/s instead of
1.90 KB/s.

**Sending fewer snapshots when the link is saturated.** The question is how the
server knows that a client's link is saturated.

| Signal | Good | Bad | Decision |
| ------ | ---- | --- | -------- |
| Packet loss | Easy to count with the ack bit field | Loss also comes from radio noise, where sending less does not help. With a deep buffer it comes late, after seconds of delay | Rejected as the only signal. |
| Round trip time of the reliable packets (what the resend logic used) | Already measured | Too few samples: it said 28 ms while the real delay was 775 ms | Rejected. |
| **Smallest round trip time of every acknowledged packet over half a second** | About 20 samples per second and per client, grows as soon as a queue forms, ignores the delay before the acknowledgement because of the minimum | The empty link round trip time must be known: it is the smallest value seen, taken in the lobby where nothing is sent | **Chosen.** |
| Bandwidth estimation by packet pairs, or a report from the client | Gives the capacity directly | Needs a probing protocol or a new message, fragile on a shared link | Too heavy for the gain. |

When the smallest round trip time of a client is more than 60 ms above its normal
value, the server sends it one snapshot out of 2, 3 or 4 (down to 5 per second),
and goes straight to 4 if it is more than 300 ms above. It speeds up one step after
6 seconds without growth, because trying a faster rate on a link that cannot
carry it fills the queue again.

**Measured effect.** With one player, a 10 KB/s link went from 775 to 64 ms of
confirmation delay. Four players on a congested mobile profile (400 kbit/s) went
from 501 to 227 ms, and on a 50 KB/s link from 481 to 59 ms. A 5 KB/s link, which
did not work at all, now gives 149 ms and world updates every 177 ms, but one run
out of two was disconnected. The reason is the burst of creations at the start of
the game, which overflows the queue before the adaptation reacts. Pacing it is a
known follow-up.

**What was not done.** The way down stays at 7.5 KB/s per client. Delta
snapshots, which the subject suggests, would reduce it further by sending only the
entities that changed, and would make narrower links possible. We chose to adapt
the rate first because it solves the saturation without changing the format of the
snapshot a second time.

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
| Custom binary protocol | Lying length fields, truncated messages, huge counts that trigger large allocations | Declared sizes are compared to the received bytes, packets with trailing bytes are dropped, fixed payloads must match their exact size, and `entityCount` is capped at 46 before any allocation. |
| UDP | Source address spoofing, replay, flooding | Connected packets must carry the connection id bound to their source address. Duplicate and old packets are dropped. Old inputs are rejected by input sequence. Invalid datagrams get no answer, so the server cannot be used to amplify traffic. Datagrams larger than 548 bytes are dropped by the transport. |
| Compression | Decompression bomb: a few bytes that expand to megabytes, or a malformed frame | The decoder writes into a buffer of `MAX_MESSAGE_SIZE` (529) bytes and fails instead of growing, the frame has no declared size to trust, and decompression only happens after the packet passed the connection and sequence checks, so spoofed and duplicate packets cost nothing. |
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
| Packet size | Batching per priority, zstd level 1 with a trained dictionary |
| Reliability | Sequence and ack bitfield, full snapshots, query based repair |
| Latency | Prediction and reconciliation of the local ship, no interpolation yet |
| Narrow links | Inputs sent on change, snapshot rate lowered when the round trip time grows |
| Build | CMake and vcpkg |
| Storage | Versioned plain files for assets, memory for live state |
