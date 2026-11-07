# Architecture overview {#architecture}

[TOC]

This page describes how the project is split into layers, how the two programs
run, and where each part lives in the repository. The details of each layer
are covered in @ref engine and @ref networking.

## Design goals

- **Authoritative server.** The server runs the whole simulation. Clients send
  inputs and display what the server tells them.
- **Decoupled subsystems.** Rendering, audio, input, networking and gameplay
  only meet through ECS components, events and small C++20 concepts.
- **Reusable engine.** Code under `lib/` knows nothing about R-Type, so it can
  drive another game.
- **Data driven content.** Entities are described in JSON blueprints and
  built at run time.

## Layers

\dot "Layer view of the client and the server"
digraph layers {
  rankdir=TB;
  ranksep=0.45;
  nodesep=0.3;
  node [shape=box, style="rounded,filled", fillcolor="#eef3fb", color="#4a6fa5", width=1.9];
  edge [color="#555555"];

  cgame   [label="Client game\n(src/Client)\nscenes, inputs, replication"];
  sgame   [label="Server game\n(src/Server)\nlobby, stage, spawns, rules"];

  cnet    [label="NetworkClient\nhandshake, inputs, decoding"];
  common  [label="Shared code (src/Common)\nprotocol payloads,\ngameplay components and systems", style="rounded,dashed,filled", fillcolor="#ffffff", color="#777777"];
  sserver [label="Server\nmessage decoding, queues"];

  engine  [label="Game engine (lib/GameEngine)\nloop, scenes, event bus, factory,\ngeneric components and systems", fillcolor="#e9f5ee", color="#2f6f4f", width=2.8];

  adapt   [label="Backend adapters\nwindow, input, audio"];
  netlib  [label="Network library (lib/Network)\nframing, tracking, NetworkServer", fillcolor="#fbf3e4", color="#8a5a00"];

  gfx     [label="rtk graphical\n(Vulkan)", fillcolor="#f2f2f2", color="#777777"];
  ecs     [label="rtk ECS", fillcolor="#f2f2f2", color="#777777"];
  asio    [label="Asio", fillcolor="#f2f2f2", color="#777777"];

  {rank=same; cgame; sgame;}
  {rank=same; cnet; common; sserver;}
  {rank=same; adapt; netlib;}
  {rank=same; gfx; ecs; asio;}

  cgame -> cnet; cgame -> common; cgame -> engine; cgame -> adapt;
  sgame -> sserver; sgame -> common; sgame -> engine;
  cnet -> netlib; sserver -> netlib; common -> netlib; common -> engine;
  engine -> ecs;
  engine -> adapt [style=dashed, label=" concepts"];
  adapt -> gfx; netlib -> asio; cnet -> asio;
}
\enddot

**Text description of the diagram.** Both programs sit on top of the same game
engine. The engine wraps the rtk ECS (registry and scheduler) and adds the main
loop, the scene manager, the event bus, the entity factory and a set of
generic systems. The client also uses the `NetworkClient` and two adapters that
connect the engine to the rtk graphical library for drawing, input and sound.
The engine never includes the graphical library: its render and input systems
talk to the adapters through the rtk::ecs::concepts::IsRenderWindow and
rtk::ecs::concepts::IsAudioManager concepts. The server uses the
rtype::server::Server class, which sits on the `NetworkServer` of the network
library. Both sides share the protocol payloads and the gameplay components
found in `src/Common`. The network library only depends on Asio.

## Runtime view

The client runs two threads. The server runs a main thread, a network thread
and one thread per room.

\dot "Threads and data flow between the client and the server" width=100%
digraph runtime {
  rankdir=LR;
  node [shape=box, style="rounded,filled", fillcolor="#eef3fb", color="#4a6fa5"];
  edge [color="#555555", fontsize=10];

  subgraph cluster_c {
    label="Client process"; style="rounded";
    cmain [label="Main thread\nengine loop, 60 FPS\ninput, render, replication"];
    cio   [label="Network thread\nAsio io_context"];
    cq    [label="Event queues\n(mutex)", shape=cylinder, fillcolor="#ffffff"];
  }

  subgraph cluster_s {
    label="Server process"; style="rounded";
    smain [label="Main thread\nengine loop, 60 ticks/s\nconnections, rooms"];
    sio   [label="Network thread\nAsio io_context"];
    sq    [label="Connection and room\nrequest queues (mutex)", shape=cylinder, fillcolor="#ffffff"];
    sbox  [label="Room mailboxes\n(mutex)", shape=cylinder, fillcolor="#ffffff"];
    sroom [label="Room threads, one per room\n60 ticks/s\nsimulation, snapshots"];
    sevt  [label="Room event queue\n(mutex)", shape=cylinder, fillcolor="#ffffff"];
  }

  cmain -> cio [label="post sends"];
  cio -> cq [label="push"]; cq -> cmain [label="poll"];
  smain -> sio [label="post sends"];
  sroom -> sio [label="post sends"];
  sio -> sq [label="push"]; sq -> smain [label="poll"];
  sio -> sbox [label="inputs, queries"];
  smain -> sbox [label="commands"];
  sbox -> sroom [label="drain"];
  sroom -> sevt [label="push"]; sevt -> smain [label="poll"];
  cio -> sio [label="Hello, Input, QueryEntity\n(UDP)"];
  sio -> cio [label="Welcome, CreateEntity,\nDestroyEntity, Snapshot (UDP)"];
}
\enddot

**Text description of the diagram.** On both sides the main thread runs the
engine loop and never waits for the network. A second thread runs an Asio
`io_context` that receives and sends UDP datagrams. When a datagram arrives,
the network thread validates it, decodes it and pushes the result into a queue
protected by a mutex. The main thread drains these queues once per frame. To
send, the main thread posts a task to the network thread, which owns the
socket. The client sends `Hello`, `Input` and `QueryEntity` messages. The
server answers with `Welcome`, then replicates the world with `CreateEntity`,
`DestroyEntity` and `Snapshot` messages.

On the server, the main thread does not simulate anything: it handles
connections and owns the rooms. Each room simulates its own world on its own
thread, as described in @ref room_threads.

## Room threads {#room_threads}

A rtype::server::RoomWorker owns the rtype::server::GameInstance of one room
and runs it at 60 ticks per second. Rooms share no gameplay state, so a slow
room does not slow the others down, and an exception only stops the room that
threw it: its players go back to the room list while the other rooms keep
playing.

Threads never call each other. Each piece of state has one owner, and the other
threads send it messages:

| State | Owner | How the other threads reach it |
| ----- | ----- | ------------------------------ |
| rtype::server::GameInstance and its ECS world | Room thread | rtype::server::RoomMailbox |
| rtype::server::RoomManager and ready states | Main thread | rtype::server::RoomEventQueue |
| Sockets and connections | Network thread | NetworkServer::send() |

- **Inputs and entity queries** go straight from the network thread to the
  mailbox of the room, found with rtype::server::RoomRouter. They are bounded
  per room, so a flooding client can only delay its own room.
- **Joining** is asynchronous. The main thread adds the player to the room and
  sends a command. The room creates the entity and answers with an event, then
  the main thread sends `RoomCreated` or `RoomJoined`.
- **Leaving** is answered by the room thread: `RoomLeft` is sent once the room
  stopped replicating to the client, so that no entity shows up afterwards.
- **Starting a round** is decided by the main thread when every player is ready.
  The end of a round comes back as an event.

To check the threading after a change, build the server with ThreadSanitizer
and play a few rounds in several rooms, joining and leaving them:

```sh
make server_tsan
setarch -R build-tsan/bin/r-type_server -p 4242
```

`setarch -R` turns off address randomization, which makes ThreadSanitizer crash
at startup on recent Linux kernels. Every data race is printed as a
`WARNING: ThreadSanitizer` report.

## Frame loop

rtk::GameEngine::run() drives both programs with the same loop:

1. Call `onLoad()` on the game, then `onStart()` on every system.
2. Measure the time since the previous frame and clamp it to 0.25 seconds.
3. Call `onUpdate(context, dt)` on the game. Both games use it to process
   network events and to update the active scene.
4. Run every system with the same `dt`, stage by stage, in this order:
   `Input`, `Update`, `Physics`, `SecondUpdate`, `Render`. Destroyed entities
   are flushed after each stage.
5. Sleep until the next frame, with a 60 frames per second target.
6. When the game stops, call `onStop()` on every system and `onUnload()` on the
   game.

All movement, animation, scrolling and spawn timers are multiplied by `dt`, so
the game runs at the same speed on every machine.

## Coordinate system and units

| Quantity | Unit and convention |
| -------- | ------------------- |
| Position | Pixels. Origin at the top left of the screen, x grows to the right, y grows downwards. |
| Visible area | 1280 by 720 pixels on the client. The server simulates a wider strip: enemies spawn between x = 1920 and x = 2920. |
| Rotation | Radians. |
| Speed | Pixels per second. |
| Time | Seconds, as a `float`. |
| Server tick | One per server frame, about 60 per second. |

A component::Transform stores the top left corner of an entity. Colliders are
offset from that corner and scaled with the entity.

## Repository map

| Path | Content |
| ---- | ------- |
| `lib/GameEngine/include` | Engine headers: rtk::GameEngine, rtk::EngineContext, components, systems, rtk::SceneManager, busEvent, rtk::factory::EntityFactory, rtk::Input::InputMapping. Header only. |
| `lib/Network/src` | Network library: PacketReader, PacketWriter, PacketHeader, MessageHeader, MessageParser, HandshakeHeader, PacketTracker, Connection, UdpTransport, NetworkServer. Built as a static library. |
| `src/Common` | Code shared by both programs: protocol payloads in `Protocol/`, gameplay components and systems, rtype::server::NetworkIdGenerator. |
| `src/Server` | `r-type_server`: rtype::server::Game, rtype::server::Server, server scenes, rtype::server::ServerDeathSystem. |
| `src/Client` | `r-type_client`: rtype::client::Game, rtype::client::NetworkClient, client scenes, rtk::systems::PlayerSystem, backend adapters. |
| `assets` | Sprites, sounds and JSON entity blueprints. |
| `docs` | This website: Doxygen configuration, guides and style sheet. |
| `tests` | Network library tests. |

## Main design decisions

| Decision | Short reason | Details |
| -------- | ------------ | ------- |
| Entity Component System | Decouples data from behavior and keeps systems independent. | @ref technical_study |
| Event bus | Lets systems react to each other without direct references. | @ref engine |
| Scenes in a `std::variant` | Compile time checked state machine, no heap allocation. | @ref engine |
| Concepts for backends | The engine has no dependency on the renderer or the audio library. | @ref engine |
| UDP with a custom binary protocol | Low latency, compact packets, full control over validation. | @ref protocol |
| Dedicated network thread | The game loop never blocks on the network. | @ref networking |
| One thread per room | A slow or crashing room does not affect the others. | @ref room_threads |
| JSON blueprints | New entities without recompiling. | @ref tutorials |
