# Game engine {#engine}

[TOC]

The engine lives in `lib/GameEngine/include`. It is header only and contains no
R-Type specific code. This page explains its building blocks and how a game
plugs into them.

## Entity Component System

The engine uses the rtk ECS library for storage and scheduling.

- An **entity** is a plain `std::size_t` id handed out by the registry. Ids of
  destroyed entities are reused.
- A **component** is a plain struct with no behavior, such as
  component::Transform or component::Velocity. Each component type is stored
  in its own sparse set: a dense array of components, plus a sparse array that
  maps an entity id to its index in the dense array.
- A **system** is a class deriving from `rtk::ecs::ISystem`. It implements
  `onStart()`, `update(registry, dt)` and `onStop()`, and iterates over a
  `view` of the entities that own a given set of components.
- The **scheduler** groups systems in five stages run in this order: `Input`,
  `Update`, `Physics`, `SecondUpdate`, `Render`. Inside a stage, systems run in
  the order they were added.

Destroying an entity is deferred: `kill_entity()` queues it, and the registry
removes it when it is flushed, after every stage. Systems can therefore remove
entities while iterating over a view.

## Components

| Component | Data | Used by |
| --------- | ---- | ------- |
| component::Transform | Position, rotation, scale | Almost every system |
| component::Velocity | Speed on x and y, in pixels per second | rtk::systems::MovementSystem |
| component::AABBCollider | Box, collision layer and mask | rtk::systems::ColliderSystem |
| component::Animation | Sprite sheet frame data | rtk::systems::AnimationSystem |
| component::animator | One animation clip per state | rtk::systems::AnimatorSystem |
| component::Parallax | Scrolling speed and tile width | rtk::systems::ParallaxSystem |
| component::NetworkEntity | Network id and archetype id | Replication on both sides |
| component::Dead | Tag, no data | rtk::systems::DeathSystem, rtype::server::ServerDeathSystem |
| component::SoundRequest | Sound id | Audio |
| rtk::ecs::concepts::SpriteData | Texture, rectangle, color, layer | rtk::systems::RenderSystem |
| component::Controllable | Ship speed, shoot state | Server input handling |
| component::Bullet | Damage | rtk::systems::BulletSystem |
| component::Enemy | Health | rtk::systems::EnemyGCSystem |

The last three are gameplay components and live in `src/Common/components`.

## Systems

| System | Stage | Client | Server | Role |
| ------ | ----- | :----: | :----: | ---- |
| rtk::systems::InputSystem | Input | yes | | Turns keys and gamepad state into action bits. |
| rtk::systems::PlayerSystem | Update | yes | | Sends the action bits to the server. |
| rtk::systems::EnemySpawnerSystem | Update | | yes | Spawns a Bydos every 1.5 seconds. |
| rtk::systems::BulletSystem | Update | | yes | Kills bullets past x = 1920. |
| rtk::systems::AnimatorSystem | Update | yes | | Picks the clip of the current state. |
| rtk::systems::ParallaxSystem | Update | yes | | Scrolls and wraps the starfield. |
| rtk::systems::AudioSystem | Update | yes | | Ticks the audio backend. |
| rtk::systems::DeathSystem | Update | yes | | Destroys dead entities. |
| rtype::server::ServerDeathSystem | Update, SecondUpdate | | yes | Destroys dead entities and tells every client. |
| rtk::systems::EnemyGCSystem | SecondUpdate | yes | yes | Kills enemies past x = -100. |
| rtk::systems::MovementSystem | Physics | yes | yes | Applies velocities. |
| rtk::systems::ColliderSystem | Physics | yes | yes | Detects overlapping colliders. |
| rtk::systems::DebugCollisionSystem | Physics | yes | | Draws collider outlines on demand. |
| rtk::systems::AnimationSystem | Render | yes | | Advances sprite sheet frames. |
| rtk::systems::RenderSystem | Render | yes | | Draws every sprite. |

The client also runs movement between two snapshots, so entities keep moving
smoothly with their blueprint velocity until the next server update corrects
their position.

## Engine context

rtk::EngineContext is passed to the game, its scenes and its event handlers. It
gives access to:

| Member | Purpose |
| ------ | ------- |
| `registry` | Entities and components. |
| `scheduler` | Systems. |
| `eventBus` | Event bus shared by everything. |
| `_entityFactory` | Blueprint based entity creation. |

A game is any type that satisfies the rtk::Game concept: it must provide
`onLoad(EngineContext &)`, and may provide `onUpdate(EngineContext &, float)`,
`shouldContinue()` and `onUnload(EngineContext &)`.

## Event bus

busEvent is a publish and subscribe bus indexed by event type. Any struct can
be an event.

```cpp
context.eventBus.subscribe<rtk::collision::Collision>(
    [](const rtk::collision::Collision &event) { /* react */ });

context.eventBus.publish(rtk::collision::Collision{first, second});
```

Handlers run immediately, on the thread that publishes. The bus is how systems
and the game talk to each other without holding references to one another. For
example, rtk::systems::ColliderSystem publishes collisions and the game decides
what they mean, and rtk::systems::EnemySpawnerSystem publishes
rtype::events::EnemySpawnedEvent so that the server can tell the clients.

## Scenes

rtk::SceneManager holds the active scene in a `std::variant`. Every scene type
must satisfy the rtk::Scene concept: a static `Type` identifier and the
`onEnter()`, `onUpdate()` and `onExit()` methods. `onUpdate()` returns the
identifier of the next scene, or `std::nullopt` to stay.

\dot "Client and server scene transitions" width=100%
digraph scenes {
  rankdir=LR;
  node [shape=box, style="rounded,filled", fillcolor="#eef3fb", color="#4a6fa5"];
  edge [fontsize=10];
  subgraph cluster_client {
    label="Client"; style="rounded";
    Menu -> Connection [label="Confirm pressed"];
    Connection -> Game [label="Welcome received"];
    Connection -> Menu [label="no answer in 10 s"];
  }
  subgraph cluster_server {
    label="Server"; style="rounded";
    Lobby -> InGame [label="2 players, or 30 s\nwith 1 player"];
    InGame -> Lobby [label="every player left"];
  }
}
\enddot

**Text description of the diagram.** The client starts on the menu scene. When
the player presses Confirm it moves to the connection scene, which sends a
`Hello` every second. It goes to the game scene when the server answers, or
back to the menu after 10 seconds without an answer. The server starts in the
lobby scene. It moves to the in game scene when two players are connected, or
30 seconds after the first player joined. It goes back to the lobby when every
player has left.

## Data driven entities

rtk::factory::EntityFactory builds entities from JSON blueprints stored in
`assets/descriptionEntity`. A blueprint lists components by key:

```json
{
  "components": {
    "transform": { "position": { "x": 0.0, "y": 0.0 }, "scale": { "x": 2.0, "y": 2.0 } },
    "sprite":    { "texture": "assets/entityTexture/bidos.png", "size": { "w": 32, "h": 32 },
                   "textureRect": { "x": 0, "y": 0, "w": 32, "h": 32 } },
    "collider":  { "width": 32, "height": 32, "layer": "Enemy", "mask": ["PlayerProjectile", "Player"] },
    "velocity":  { "x": -200.0, "y": 0.0 },
    "enemy":     {},
    "networkEntity": {}
  }
}
```

Each key is matched with a builder registered by the game with
`registerBuilder()`, for example `transform` with component::transformFromJson().
Unknown keys are ignored, so the server can load the same blueprints as the
client and simply skip the `sprite` key.

Every blueprint name is hashed to a 16-bit **archetype id** with
rtk::hashArchetype(). Client and server compute the same id from the same name,
so the server only has to send that id over the network to tell a client which
blueprint to instantiate.

## Collisions

rtk::systems::ColliderSystem places every collider in a uniform grid of 64 pixel
cells, then tests only the pairs that share a cell. A pair collides when the
boxes overlap and each collider mask contains the other collider layer. Each
pair is reported once per frame, in the rtk::collision::CollisionBuffer and as
a rtk::collision::Collision event.

| Layer | Meaning |
| ----- | ------- |
| `Player` | Player ships |
| `Enemy` | Bydos |
| `PlayerProjectile` | Player missiles |
| `EnemyProjectile` | Enemy missiles |
| `Environment` | Stage walls and obstacles |

On the server, the game turns collisions into gameplay: a player missile and an
enemy destroy each other, a missile hitting a wall disappears, and a ship
touching an enemy or a wall is destroyed. Destroyed entities get the
component::Dead tag and are removed at the end of the stage.

## Input

Input is split in two steps so that gameplay never sees physical keys.

1. rtk::Input::InputMapping binds up to four physical controls (keys, gamepad
   buttons, gamepad axes with a threshold, mouse buttons) to each of 32 actions.
2. rtk::systems::InputSystem reads the window events every frame and sets bit n
   of rtk::InputState::actionMask when any control bound to action n is active.

The client defines its actions in rtype::Action. Changing a control is one
`bind` call and does not touch any gameplay code.

## Rendering and audio backends

The engine does not include the graphical library. It defines what it needs as
concepts:

- rtk::ecs::concepts::IsRenderWindow: poll events into an
  rtk::ecs::concepts::Event, begin a frame, draw a batch of
  rtk::ecs::concepts::SpriteData, end the frame.
- rtk::ecs::concepts::IsAudioManager: load, play and remove sounds, and tick
  once per frame.

The client implements them with RtkWindowAdapter and
rtype::client::RtkAudioAdapter. rtk::systems::RenderSystem only reads
component::Transform and rtk::ecs::concepts::SpriteData: it knows nothing about
speed, health or damage. Replacing the renderer means writing a new adapter
that satisfies the concept.
