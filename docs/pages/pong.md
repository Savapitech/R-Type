# Second game: Pong {#pong}

[TOC]

Pong is a small networked game built on the same engine as R-Type. It exists
to show that the engine knows nothing about R-Type: Pong reuses the engine
(`lib/GameEngine`), the ECS and the graphical library, but none of the R-Type
code under `src/`. Its sources live in `pong/`.

## Building and running

Pong is not part of the default build. Build it with:

```sh
make pong
```

This configures the project with `-DRTYPE_BUILD_PONG=ON` and builds `pong`,
`pong_server` and the two audio plugins in `bin/` and `bin/plugins/`. Start
both programs from the repository root, as they load `pong/asset/` and the
plugins with relative paths:

```sh
./bin/pong_server [-p port]          # default port 4242
./bin/pong [-h host] [-p port]       # start two clients to play
```

The first client gets the left paddle, the second the right one, and the
following ones watch the match.

| Action | Keys |
| ------ | ---- |
| Move the paddle | W or Z, S, or the arrow keys |
| Start the match | Space or Enter |
| Back to the menu | Escape |
| Switch the audio plugin, in game | Space |

## What Pong takes from the engine

| Engine feature | Use in Pong |
| -------------- | ----------- |
| `rtk::GameEngine` and the rtk::Game concept | Main loop at a fixed frame rate, for the client and the server |
| ECS registry and scheduler | Paddles, ball, walls and scores are entities, updated by systems |
| Scene manager | Menu scene and game scene of the client |
| Data driven factory | Every entity is a JSON blueprint in `pong/asset/description/` |
| `MovementSystem`, `ColliderSystem` | Ball and paddle movement and bounces |
| `InputSystem` and `InputMapping` | Keyboard bindings of the actions in `pong/client/action.hpp` |
| `RenderSystem` and `ParticleManager` | Sprites and the particles of a bounce |
| Event bus | `BallBounceEvent` and `ScoreChangedEvent` trigger sounds and the score display |
| Runtime plugins (`rtk::audio::DynamicLibrary`, `rtk::audio::IAudioEngine`) | Audio is loaded from `bin/plugins/` at run time and can be swapped while playing |

## Runtime audio plugins

The engine only defines the `IAudioEngine` interface and a loader for shared
libraries. Pong ships two implementations, built as modules:

| Plugin | File | Behaviour |
| ------ | ---- | --------- |
| `pong_audio_rtk` | `bin/plugins/libpong_audio_rtk` + platform extension | Plays sounds with the rtk graphical library |
| `pong_audio_dummy` | `bin/plugins/libpong_audio_dummy` + platform extension | Logs the calls, plays nothing, for headless runs and tests |

The client loads the rtk plugin at start and switches between the two when the
player presses Space during a match. A new audio backend only needs a new
plugin, the game is not rebuilt.

## What stays in the game

Pong has its own small UDP protocol, written with Asio in
`pong/common/protocol/PongProtocol.hpp`: a connect request, a slot assignment,
the inputs of a paddle and a snapshot of the match. The server is
authoritative: it moves the ball and the paddles and sends a snapshot every
frame. This protocol is unrelated to the R-Type one described in @ref protocol.
